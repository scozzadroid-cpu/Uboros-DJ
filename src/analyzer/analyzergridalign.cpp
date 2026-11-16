#include "analyzer/analyzergridalign.h"

#include <dsp/transforms/FFT.h>

#include <QtDebug>
#include <algorithm>
#include <array>
#include <cmath>

#include "analyzer/analyzertrack.h"
#include "library/rekordbox/rekordboxconstants.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"

namespace {

// Same parameters as usb/bpmcheck (the measurement the method comes from):
// 1024 frames window (23 ms at 44.1 kHz), 128 frames hop (2.9 ms)
constexpr int kFftSize = 1024;
constexpr int kHop = 128;
constexpr int kBins = kFftSize / 2 + 1;

constexpr double kMaxLagMs = 80.0;
constexpr double kLagStepMs = 0.5;
constexpr int kLagCount = static_cast<int>(2 * kMaxLagMs / kLagStepMs) + 1;

// Beats near the start and the end are often silence or a fade
constexpr int kSkipBeats = 16;
constexpr int kMinBeats = 64;

// Decision (measured on 156 tracks, 2026-11-16): the peak of the folded
// onsets is at least 1.4 times the median of the curve, and at least three
// quarters of the track have their own peak within 5 ms of it (a quarter that
// disagrees is usually a break or the outro; two that disagree are a drifting
// grid or a wrong BPM, that a shift cannot fix)
constexpr double kMinSharpness = 1.4;
constexpr double kQuarterToleranceMs = 5.0;
constexpr int kMinAgreeingQuarters = 3;

double lagMs(int index) {
    return -kMaxLagMs + index * kLagStepMs;
}

int peakIndex(const std::array<double, kLagCount>& curve) {
    return static_cast<int>(std::max_element(curve.cbegin(), curve.cend()) - curve.cbegin());
}

} // namespace

AnalyzerGridAlign::AnalyzerGridAlign(UserSettingsPointer pConfig)
        : m_pConfig(std::move(pConfig)) {
}

AnalyzerGridAlign::~AnalyzerGridAlign() = default;

// static
ConfigKey AnalyzerGridAlign::enabledConfigKey() {
    return ConfigKey(QStringLiteral("[BPM]"), QStringLiteral("AlignRekordboxGrids"));
}

bool AnalyzerGridAlign::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        mixxx::audio::ChannelCount channelCount,
        SINT frameLength) {
    if (frameLength <= 0 || !m_pConfig->getValue(enabledConfigKey(), true)) {
        return false;
    }
    const TrackPointer pTrack = track.getTrack();
    if (pTrack->isBpmLocked()) {
        return false;
    }
    const auto pBeats = pTrack->getBeats();
    if (!pBeats || !mixxx::rekordboxconstants::needsAlignment(pBeats->getSubVersion())) {
        return false;
    }

    m_sampleRate = sampleRate;
    m_channelCount = channelCount;
    m_frameLength = frameLength;
    m_pFft = std::make_unique<FFTReal>(kFftSize);
    m_window.resize(kFftSize);
    for (int i = 0; i < kFftSize; ++i) {
        m_window[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (kFftSize - 1));
    }
    m_frame.resize(kFftSize);
    m_re.resize(kFftSize);
    m_im.resize(kFftSize);
    m_prevSpectrum.assign(kBins, 0.0f);
    m_spectrum.resize(kBins);
    m_pending.clear();
    m_pending.reserve(kFftSize + 8192);
    m_flux.clear();
    m_flux.reserve(frameLength / kHop + 1);
    return true;
}

bool AnalyzerGridAlign::processSamples(const CSAMPLE* pIn, SINT count) {
    const int channels = m_channelCount;
    const SINT frames = count / channels;
    const float scale = 1.0f / channels;
    for (SINT f = 0; f < frames; ++f) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += pIn[f * channels + c];
        }
        m_pending.push_back(sum * scale);
        if (m_pending.size() == static_cast<size_t>(kFftSize)) {
            processFrame();
            m_pending.erase(m_pending.begin(), m_pending.begin() + kHop);
        }
    }
    return true;
}

void AnalyzerGridAlign::processFrame() {
    for (int i = 0; i < kFftSize; ++i) {
        m_frame[i] = m_pending[i] * m_window[i];
    }
    m_pFft->forward(m_frame.data(), m_re.data(), m_im.data());
    float flux = 0.0f;
    for (int k = 0; k < kBins; ++k) {
        const float magnitude = static_cast<float>(std::hypot(m_re[k], m_im[k]));
        m_spectrum[k] = std::log1p(100.0f * magnitude);
        flux += std::max(0.0f, m_spectrum[k] - m_prevSpectrum[k]);
    }
    // The first frame has no predecessor
    m_flux.push_back(m_flux.empty() ? 0.0f : flux);
    std::swap(m_spectrum, m_prevSpectrum);
}

void AnalyzerGridAlign::storeResults(TrackPointer pTrack) {
    const auto pBeats = pTrack->getBeats();
    // Another analyzer or the user may have replaced the grid meanwhile
    if (!pBeats || pTrack->isBpmLocked() ||
            !mixxx::rekordboxconstants::needsAlignment(pBeats->getSubVersion())) {
        return;
    }
    const int fluxCount = static_cast<int>(m_flux.size());
    if (fluxCount < 2) {
        return;
    }

    std::vector<double> beats;
    const auto end = mixxx::audio::FramePos(static_cast<double>(m_frameLength));
    for (auto it = pBeats->iteratorFrom(mixxx::audio::kStartFramePos);
            it != pBeats->cend() && *it < end;
            ++it) {
        beats.push_back(it->value());
    }
    const int usable = static_cast<int>(beats.size()) - 2 * kSkipBeats;
    if (usable < kMinBeats) {
        qInfo() << "Grid alignment: too few beats" << beats.size() << "in" << pTrack->getTitle();
        pTrack->trySetBeats(mixxx::Beats::fromByteArray(pBeats->getSampleRate(),
                pBeats->getVersion(),
                mixxx::rekordboxconstants::unsureSubversion,
                pBeats->toByteArray()));
        return;
    }

    // Folded onset strength per quarter of the track and lag. Flux value i
    // belongs to the window that starts at i * kHop, i.e. centered at
    // i * kHop + kFftSize / 2.
    const double framesPerMs = m_sampleRate.value() / 1000.0;
    std::array<std::array<double, kLagCount>, 4> quarters{};
    for (int b = 0; b < usable; ++b) {
        const double beat = beats[kSkipBeats + b];
        auto& curve = quarters[b * 4 / usable];
        for (int l = 0; l < kLagCount; ++l) {
            const double pos = (beat + lagMs(l) * framesPerMs - kFftSize / 2.0) / kHop;
            const int k = static_cast<int>(std::floor(pos));
            if (k < 0 || k >= fluxCount - 1) {
                continue;
            }
            const double a = pos - k;
            curve[l] += (1.0 - a) * m_flux[k] + a * m_flux[k + 1];
        }
    }
    std::array<double, kLagCount> total{};
    for (const auto& curve : quarters) {
        for (int l = 0; l < kLagCount; ++l) {
            total[l] += curve[l];
        }
    }
    const int peak = peakIndex(total);
    std::array<double, kLagCount> sorted = total;
    std::nth_element(sorted.begin(), sorted.begin() + kLagCount / 2, sorted.end());
    const double median = sorted[kLagCount / 2];
    const double sharpness = median > 0 ? total[peak] / median : 0.0;

    // Sub-step position of the peak (parabola through the 3 points)
    double shiftMs = lagMs(peak);
    if (peak > 0 && peak < kLagCount - 1) {
        const double y0 = total[peak - 1];
        const double y1 = total[peak];
        const double y2 = total[peak + 1];
        const double denominator = y0 - 2 * y1 + y2;
        if (denominator < 0) {
            shiftMs += kLagStepMs * 0.5 * (y0 - y2) / denominator;
        }
    }

    int agreeing = 0;
    QStringList quarterPeaks;
    for (const auto& curve : quarters) {
        const double quarterPeak = lagMs(peakIndex(curve));
        quarterPeaks << QString::number(quarterPeak);
        if (std::abs(quarterPeak - lagMs(peak)) <= kQuarterToleranceMs) {
            ++agreeing;
        }
    }
    // A peak at the edge of the range is not a peak
    const bool atEdge = peak == 0 || peak == kLagCount - 1;
    const bool sure = !atEdge && sharpness >= kMinSharpness &&
            agreeing >= kMinAgreeingQuarters;

    qInfo().noquote() << "Grid alignment:" << (sure ? "moved" : "unsure, kept")
                      << QString::number(shiftMs, 'f', 1) << "ms, sharpness"
                      << QString::number(sharpness, 'f', 2) << "quarters"
                      << quarterPeaks.join(QLatin1Char('/')) << "for" << pTrack->getArtist()
                      << "-" << pTrack->getTitle();

    if (!sure) {
        pTrack->trySetBeats(mixxx::Beats::fromByteArray(pBeats->getSampleRate(),
                pBeats->getVersion(),
                mixxx::rekordboxconstants::unsureSubversion,
                pBeats->toByteArray()));
        return;
    }

    // Kicks at beat + shift: move the grid by the shift, and the cues that
    // were placed on it (rekordbox memory/hot cues and loops, the user's own
    // hot cues); intro/outro and the sound range come from the audio
    const double shiftFrames = shiftMs * framesPerMs;
    const auto translated = pBeats->tryTranslate(shiftFrames);
    if (!translated) {
        return;
    }
    const auto pAligned = mixxx::Beats::fromByteArray(pBeats->getSampleRate(),
            (*translated)->getVersion(),
            mixxx::rekordboxconstants::alignedSubversion(shiftMs),
            (*translated)->toByteArray());
    if (!pAligned || !pTrack->trySetBeats(pAligned)) {
        return;
    }
    const auto mainCue = pTrack->getMainCuePosition();
    if (mainCue.isValid()) {
        pTrack->setMainCuePosition(mainCue + shiftFrames);
    }
    for (const CuePointer& pCue : pTrack->getCuePoints()) {
        switch (pCue->getType()) {
        case mixxx::CueType::HotCue:
        case mixxx::CueType::Loop:
        case mixxx::CueType::Jump:
            pCue->shiftPositionFrames(shiftFrames);
            break;
        default:
            break;
        }
    }
}

void AnalyzerGridAlign::cleanup() {
    m_pFft.reset();
    m_pending.clear();
    m_pending.shrink_to_fit();
    m_flux.clear();
    m_flux.shrink_to_fit();
}
