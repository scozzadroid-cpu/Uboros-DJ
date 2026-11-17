#include "stems/htdemucs.h"

#include <MNN/Interpreter.hpp>
#include <MNN/MNNForwardType.h>
#include <MNN/expr/Executor.hpp>
#include <MNN/expr/ExprCreator.hpp>
#include <MNN/expr/Module.hpp>
#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mixxx {
namespace stems {

namespace {

constexpr int kMagChannels = 4;                            // re/im of 2 channels
constexpr long kMagSize = long(kMagChannels) * kBins * kSpecFrames;
constexpr int kStftPad = kHop / 2 * 3;                      // htdemucs _spec padding (1536)
constexpr long kPaddedLength = long(kSpecFrames) * kHop + 2 * kStftPad; // 347136
constexpr int kAllFrames = kSpecFrames + 4;                 // torch.stft frames (center=True)
constexpr int kCenterPad = kNfft / 2;
// Output sample s of a segment is sample (kCenterPad + kStftPad + s) of the
// overlap-added iSTFT frames
constexpr long kOutputStart = kCenterPad + kStftPad;

/// torch "reflect" padding index
inline long reflect(long i, long n) {
    if (i < 0) {
        i = -i;
    }
    if (i >= n) {
        i = 2 * (n - 1) - i;
    }
    return i;
}

void normalize(const float* in, long n, float* out, double* pMean, double* pStd) {
    double sum = 0;
    for (long i = 0; i < n; ++i) {
        sum += in[i];
    }
    const double mean = sum / n;
    double sq = 0;
    for (long i = 0; i < n; ++i) {
        const double d = in[i] - mean;
        sq += d * d;
    }
    const double std = std::sqrt(sq / (n - 1)); // torch.std is unbiased
    const double scale = 1.0 / (1e-5 + std);
    for (long i = 0; i < n; ++i) {
        out[i] = static_cast<float>((in[i] - mean) * scale);
    }
    *pMean = mean;
    *pStd = std;
}

} // namespace

// ---------------------------------------------------------------- network

struct HtdemucsNet::Impl {
    std::shared_ptr<MNN::Express::Executor::RuntimeManager> pRuntime;
    std::shared_ptr<MNN::Express::Module> pModule;
    bool cacheUpdated = false;
};

HtdemucsNet::HtdemucsNet() = default;
HtdemucsNet::~HtdemucsNet() = default;

std::string HtdemucsNet::cacheFile(const Config& config) {
    std::string tuning;
    if (config.backend == Backend::OpenCl) {
        tuning = config.tuning == Config::Tuning::Wide
                ? "_wide"
                : (config.tuning == Config::Tuning::Fast ? "_fast" : "_none");
    }
    return config.cacheDir + "/htdemucs3_" +
            (config.backend == Backend::OpenCl ? "opencl" : "cpu") +
            (config.lowPrecision ? "_fp16" : "_fp32") + tuning + ".cache";
}

bool HtdemucsNet::init(const Config& config) {
    using namespace MNN;
    using namespace MNN::Express;
    m_pImpl = std::make_unique<Impl>();
    ScheduleConfig sc;
    BackendConfig bc;
    bc.precision = config.lowPrecision ? BackendConfig::Precision_Low
                                       : BackendConfig::Precision_High;
    bc.memory = BackendConfig::Memory_Normal;
    bc.power = BackendConfig::Power_High;
    sc.backendConfig = &bc;
    sc.backupType = MNN_FORWARD_CPU;
    if (config.backend == Backend::OpenCl) {
        sc.type = MNN_FORWARD_OPENCL;
        // For OpenCL numThread carries the GPU mode (buffer memory, kernel tuning)
        int tuning = MNN_GPU_TUNING_NONE;
        if (config.tuning == Config::Tuning::Fast) {
            tuning = MNN_GPU_TUNING_FAST;
        } else if (config.tuning == Config::Tuning::Wide) {
            tuning = MNN_GPU_TUNING_WIDE;
        }
        sc.numThread = MNN_GPU_MEMORY_BUFFER | tuning;
    } else {
        sc.type = MNN_FORWARD_CPU;
        sc.numThread = config.threads;
    }
    m_pImpl->pRuntime.reset(Executor::RuntimeManager::createRuntimeManager(sc));
    if (!m_pImpl->pRuntime) {
        m_error = "cannot create the MNN runtime";
        return false;
    }
    if (!config.cacheDir.empty()) {
        m_pImpl->pRuntime->setCache(cacheFile(config));
    }
    Module::Config mc;
    mc.shapeMutable = false;
    m_pImpl->pModule.reset(Module::load({"mix", "mag"},
            {"x", "xt"},
            config.modelPath.c_str(),
            m_pImpl->pRuntime,
            &mc));
    if (!m_pImpl->pModule) {
        m_error = "cannot load the model " + config.modelPath;
        return false;
    }
    m_backend = config.backend;
    return true;
}

bool HtdemucsNet::run(const float* mix, const float* mag, float* x, float* xt) {
    using namespace MNN::Express;
    if (!m_pImpl || !m_pImpl->pModule) {
        return false;
    }
    auto mixVar = _Input({1, 2, kSegmentFrames}, NCHW, halide_type_of<float>());
    auto magVar = _Input({1, kMagChannels, kBins, kSpecFrames}, NCHW, halide_type_of<float>());
    std::memcpy(mixVar->writeMap<float>(), mix, sizeof(float) * 2 * kSegmentFrames);
    std::memcpy(magVar->writeMap<float>(), mag, sizeof(float) * kMagSize);
    std::vector<VARP> outs = m_pImpl->pModule->onForward({mixVar, magVar});
    if (outs.size() != 2) {
        m_error = "unexpected network outputs";
        return false;
    }
    const float* px = outs[0]->readMap<float>();
    const float* pxt = outs[1]->readMap<float>();
    if (!px || !pxt) {
        m_error = "cannot read the network outputs";
        return false;
    }
    std::memcpy(x, px, sizeof(float) * kStemCount * kMagSize);
    std::memcpy(xt, pxt, sizeof(float) * kStemCount * 2 * kSegmentFrames);
    if (!m_pImpl->cacheUpdated) {
        // Kernel tuning of the first run, reused by the next starts
        m_pImpl->pRuntime->updateCache();
        m_pImpl->cacheUpdated = true;
    }
    return true;
}

// ---------------------------------------------------------------- FFT

struct TrackSeparator::Fft {
    float* time;
    fftwf_complex* freq;
    fftwf_plan forward;
    fftwf_plan backward;
    Fft() {
        time = static_cast<float*>(fftwf_malloc(sizeof(float) * kNfft));
        freq = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * (kNfft / 2 + 1)));
        forward = fftwf_plan_dft_r2c_1d(kNfft, time, freq, FFTW_ESTIMATE);
        backward = fftwf_plan_dft_c2r_1d(kNfft, freq, time, FFTW_ESTIMATE);
    }
    ~Fft() {
        fftwf_destroy_plan(forward);
        fftwf_destroy_plan(backward);
        fftwf_free(time);
        fftwf_free(freq);
    }
};

// ---------------------------------------------------------------- track

TrackSeparator::TrackSeparator(HtdemucsNet* pNet, double overlap)
        : m_pNet(pNet),
          m_overlap(overlap),
          m_pFft(std::make_unique<Fft>()) {
    initWeights();
    // torch.hann_window (periodic)
    m_window.resize(kNfft);
    for (int n = 0; n < kNfft; ++n) {
        m_window[n] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * n / kNfft));
    }
    // iSTFT window envelope (all kAllFrames frames, zero padded ones included)
    m_envelope.assign(kSegmentFrames, 0.0f);
    for (int t = 0; t < kAllFrames; ++t) {
        const long frameStart = long(t) * kHop;
        for (int n = 0; n < kNfft; ++n) {
            const long s = frameStart + n - kOutputStart;
            if (s >= 0 && s < kSegmentFrames) {
                m_envelope[s] += m_window[n] * m_window[n];
            }
        }
    }
    m_mix.resize(2 * kSegmentFrames);
    m_mag.resize(kMagSize);
    m_x.resize(kStemCount * kMagSize);
    m_xt.resize(kStemCount * 2 * kSegmentFrames);
    m_stem.resize(kSegmentFrames);
}

TrackSeparator::TrackSeparator(SegmentNet* pNet, double overlap)
        : m_pSegmentNet(pNet),
          m_segmentFrames(pNet->segmentFrames()),
          m_overlap(overlap),
          m_pFft(std::make_unique<Fft>()) {
    initWeights();
    m_mix.resize(2 * m_segmentFrames);
    // The stems of a segment
    m_xt.resize(kStemCount * 2 * m_segmentFrames);
}

void TrackSeparator::initWeights() {
    // demucs: triangle with its maximum in the middle, normalized to 1
    m_weight.resize(m_segmentFrames);
    const long half = m_segmentFrames / 2;
    for (long i = 0; i < half; ++i) {
        m_weight[i] = static_cast<float>(i + 1);
    }
    for (long i = half; i < m_segmentFrames; ++i) {
        m_weight[i] = static_cast<float>(m_segmentFrames - i);
    }
    const float maxWeight = *std::max_element(m_weight.begin(), m_weight.end());
    for (float& w : m_weight) {
        w /= maxWeight;
    }
}

TrackSeparator::~TrackSeparator() = default;

void TrackSeparator::setTrack(const float* interleaved, long frames) {
    m_frames = frames;
    // demucs: ref = mono mix; wav = (wav - ref.mean()) / ref.std()
    double sum = 0;
    for (long i = 0; i < frames; ++i) {
        sum += 0.5 * (double(interleaved[2 * i]) + interleaved[2 * i + 1]);
    }
    m_mean = sum / frames;
    double sq = 0;
    for (long i = 0; i < frames; ++i) {
        const double d = 0.5 * (double(interleaved[2 * i]) + interleaved[2 * i + 1]) - m_mean;
        sq += d * d;
    }
    m_std = std::sqrt(sq / (frames - 1)) + 1e-8;
    for (int c = 0; c < 2; ++c) {
        m_track[c].resize(frames);
        for (long i = 0; i < frames; ++i) {
            m_track[c][i] = static_cast<float>((interleaved[2 * i + c] - m_mean) / m_std);
        }
    }
    // As demucs: int((1 - overlap) * segment_length) in double (with a float
    // overlap the stride can be one frame shorter: every segment shifted)
    const long stride = static_cast<long>((1.0 - m_overlap) * m_segmentFrames);
    m_offsets.clear();
    for (long offset = 0; offset < frames; offset += stride) {
        m_offsets.push_back(offset);
    }
    m_done.assign(m_offsets.size(), false);
    m_segmentOut.assign(m_offsets.size(), std::vector<float>());
}

int TrackSeparator::segmentAt(long frame) const {
    for (int i = 0; i < segmentCount(); ++i) {
        if (frame < m_offsets[i] + m_segmentFrames) {
            return i;
        }
    }
    return segmentCount() - 1;
}

void TrackSeparator::spectrogram(const float* mix, float* mag) const {
    // htdemucs _spec: reflect pad (kStftPad, ...) to kPaddedLength, then
    // torch.stft(center=True) reflect pads kCenterPad more; keep frames 2..2+336
    const float norm = 1.0f / std::sqrt(float(kNfft)); // normalized=True
    for (int c = 0; c < 2; ++c) {
        const float* in = mix + long(c) * kSegmentFrames;
        for (int t = 0; t < kSpecFrames; ++t) {
            const long frameStart = long(t + 2) * kHop;
            for (int n = 0; n < kNfft; ++n) {
                const long i = reflect(frameStart + n - kCenterPad, kPaddedLength);
                const long j = reflect(i - kStftPad, kSegmentFrames);
                m_pFft->time[n] = in[j] * m_window[n];
            }
            fftwf_execute(m_pFft->forward);
            for (int f = 0; f < kBins; ++f) {
                mag[(long(2 * c) * kBins + f) * kSpecFrames + t] = m_pFft->freq[f][0] * norm;
                mag[(long(2 * c + 1) * kBins + f) * kSpecFrames + t] = m_pFft->freq[f][1] * norm;
            }
        }
    }
}

void TrackSeparator::inverse(const float* spec, float* out) const {
    // spec: [re/im][kBins][kSpecFrames] of one channel of one stem.
    // torch.istft(normalized=True, center=True): frames * sqrt(n_fft), irfft
    // (1/n_fft), window, overlap-add, divided by the window envelope.
    std::fill(out, out + kSegmentFrames, 0.0f);
    const float scale = std::sqrt(float(kNfft)) / kNfft;
    for (int t = 0; t < kSpecFrames; ++t) {
        for (int f = 0; f < kBins; ++f) {
            m_pFft->freq[f][0] = spec[long(f) * kSpecFrames + t];
            m_pFft->freq[f][1] = spec[(long(kBins) + f) * kSpecFrames + t];
        }
        m_pFft->freq[kBins][0] = 0.0f; // the dropped Nyquist bin
        m_pFft->freq[kBins][1] = 0.0f;
        fftwf_execute(m_pFft->backward);
        const long frameStart = long(t + 2) * kHop; // 2 zero frames before
        for (int n = 0; n < kNfft; ++n) {
            const long s = frameStart + n - kOutputStart;
            if (s >= 0 && s < kSegmentFrames) {
                out[s] += m_pFft->time[n] * scale * m_window[n];
            }
        }
    }
    for (long s = 0; s < kSegmentFrames; ++s) {
        out[s] /= m_envelope[s];
    }
}

bool TrackSeparator::processSegment(int segment, const Output& output) {
    if (segment < 0 || segment >= segmentCount() || m_done[segment]) {
        return true;
    }
    const long offset = m_offsets[segment];
    const long length = std::min<long>(m_segmentFrames, m_frames - offset);
    // demucs TensorChunk.padded(): a short last chunk is centered, taking the
    // audio before it, and zero padded after the end of the track
    const long delta = m_segmentFrames - length;
    const long start = offset - delta / 2;
    for (int c = 0; c < 2; ++c) {
        for (long i = 0; i < m_segmentFrames; ++i) {
            const long src = start + i;
            m_mix[long(c) * m_segmentFrames + i] =
                    (src >= 0 && src < m_frames) ? m_track[c][src] : 0.0f;
        }
    }
    const long trim = delta / 2; // demucs center_trim
    std::vector<float>& segOut = m_segmentOut[segment];
    if (m_pSegmentNet) {
        if (!m_pSegmentNet->separate(m_mix.data(), m_xt.data())) {
            return false;
        }
        segOut.assign(size_t(kStemCount) * 2 * length, 0.0f);
        for (int sc = 0; sc < kStemCount * 2; ++sc) {
            std::copy_n(m_xt.data() + sc * m_segmentFrames + trim,
                    length,
                    segOut.data() + sc * length);
        }
        return finishSegment(segment, output);
    }
    spectrogram(m_mix.data(), m_mag.data());
    // The normalizations of the network input, done here in double: their
    // reductions overflow in fp16
    std::vector<float> mixn(2 * kSegmentFrames);
    std::vector<float> magn(kMagSize);
    double mixMean, mixStd, magMean, magStd;
    normalize(m_mix.data(), 2 * kSegmentFrames, mixn.data(), &mixMean, &mixStd);
    normalize(m_mag.data(), kMagSize, magn.data(), &magMean, &magStd);
    if (!m_pNet->run(mixn.data(), magn.data(), m_x.data(), m_xt.data())) {
        return false;
    }
    segOut.assign(size_t(kStemCount) * 2 * length, 0.0f);
    for (int s = 0; s < kStemCount; ++s) {
        float* x = m_x.data() + long(s) * kMagSize;
        for (long i = 0; i < kMagSize; ++i) {
            x[i] = static_cast<float>(x[i] * magStd + kSlotMeanFactor[s] * magMean);
        }
        for (int c = 0; c < 2; ++c) {
            inverse(x + long(2 * c) * kBins * kSpecFrames, m_stem.data());
            const float* xt = m_xt.data() + (long(s) * 2 + c) * kSegmentFrames;
            float* out = segOut.data() + (long(s) * 2 + c) * length;
            for (long j = 0; j < length; ++j) {
                out[j] = m_stem[trim + j] +
                        static_cast<float>(xt[trim + j] * mixStd + kSlotMeanFactor[s] * mixMean);
            }
        }
    }
    return finishSegment(segment, output);
}

bool TrackSeparator::finishSegment(int segment, const Output& output) {
    const long offset = m_offsets[segment];
    const long length = segmentEnd(segment) - offset;
    m_done[segment] = true;

    // Frames of this segment that are final now: split at the boundaries of
    // the neighbours (overlap < 50%: only they can cover the same frames)
    std::vector<long> cuts = {offset, offset + length};
    for (int n : {segment - 1, segment + 1}) {
        if (n >= 0 && n < segmentCount()) {
            for (long b : {m_offsets[n], segmentEnd(n)}) {
                if (b > offset && b < offset + length) {
                    cuts.push_back(b);
                }
            }
        }
    }
    std::sort(cuts.begin(), cuts.end());
    for (size_t k = 0; k + 1 < cuts.size(); ++k) {
        if (cuts[k] < cuts[k + 1] && isRangeFinal(cuts[k], cuts[k + 1])) {
            emitFinal(cuts[k], cuts[k + 1], output);
        }
    }
    // Free the segments whose frames are all final
    for (int n : {segment - 1, segment, segment + 1}) {
        if (n >= 0 && n < segmentCount() && m_done[n] &&
                isRangeFinal(m_offsets[n], segmentEnd(n))) {
            std::vector<float>().swap(m_segmentOut[n]);
        }
    }
    return true;
}

void TrackSeparator::emitFinal(long start, long end, const Output& output) {
    // demucs: weighted average of the covering segments, then est * std + mean
    // (once per source summed into the slot)
    constexpr long kBlock = 16384;
    m_emit.resize(size_t(kBlock) * 2 * kStemCount);
    for (long blockStart = start; blockStart < end; blockStart += kBlock) {
        const long count = std::min(kBlock, end - blockStart);
        std::fill(m_emit.begin(), m_emit.begin() + count * 2 * kStemCount, 0.0f);
        std::vector<float> weightSum(count, 0.0f);
        for (int n = 0; n < segmentCount(); ++n) {
            const long segStart = m_offsets[n];
            const long segLength = segmentEnd(n) - segStart;
            const long a = std::max(blockStart, segStart);
            const long b = std::min(blockStart + count, segStart + segLength);
            if (a >= b) {
                continue;
            }
            const std::vector<float>& segOut = m_segmentOut[n];
            for (long p = a; p < b; ++p) {
                const long j = p - segStart;
                const float w = m_weight[j];
                weightSum[p - blockStart] += w;
                float* dst = m_emit.data() + (p - blockStart) * 2 * kStemCount;
                for (int s = 0; s < kStemCount; ++s) {
                    for (int c = 0; c < 2; ++c) {
                        dst[s * 2 + c] += w * segOut[(long(s) * 2 + c) * segLength + j];
                    }
                }
            }
        }
        for (long i = 0; i < count; ++i) {
            float* dst = m_emit.data() + i * 2 * kStemCount;
            for (int s = 0; s < kStemCount; ++s) {
                for (int c = 0; c < 2; ++c) {
                    dst[s * 2 + c] = static_cast<float>(dst[s * 2 + c] / weightSum[i] * m_std +
                            kSlotMeanFactor[s] * m_mean);
                }
            }
        }
        output(blockStart, count, m_emit.data());
    }
}

int TrackSeparator::doneCount() const {
    return static_cast<int>(std::count(m_done.begin(), m_done.end(), true));
}

bool TrackSeparator::isRangeFinal(long start, long end) const {
    for (int i = 0; i < segmentCount(); ++i) {
        const long segStart = m_offsets[i];
        const long segEnd = std::min<long>(segStart + m_segmentFrames, m_frames);
        if (segStart < end && segEnd > start && !m_done[i]) {
            return false;
        }
    }
    return true;
}

} // namespace stems
} // namespace mixxx
