#include "engine/bufferscalers/enginebufferscalest.h"

#include "moc_enginebufferscalest.cpp"

// Fixes redefinition warnings from SoundTouch.
#include <soundtouch/SoundTouch.h>

#include "engine/readaheadmanager.h"
#include "util/math.h"
#include "util/sample.h"

using namespace soundtouch;

namespace {

// Due to filtering and oversampling, SoundTouch is some samples behind (offset).
// The value below was experimental identified using a saw signal and SoundTouch 2.1.1
constexpr SINT kSeekOffsetFramesV20101 = 429;

// From V 2.3.0 Soundtouch has no initial offset at unity. With a time stretch
// factor tau = tempo / pitch != 1 the audio is still shifted against the
// position Mixxx reports, see positionOffsetFrames() below.

constexpr SINT kBackBufferFrameSize = 512;

// The reported position follows the offset target with at most this fraction
// of the frames consumed per callback (a 5% speed deviation of the reported
// position), so a tempo change never makes the play position jump.
constexpr double kMaxPositionOffsetSlew = 0.05;

}  // namespace

EngineBufferScaleST::EngineBufferScaleST(ReadAheadManager* pReadAheadManager)
        : m_pReadAheadManager(pReadAheadManager),
          m_pSoundTouch(std::make_unique<soundtouch::SoundTouch>()),
          m_bBackwards(false),
          m_bAntiAliasFilter(true),
          m_positionOffsetTarget(0.0),
          m_positionOffsetApplied(0.0) {
    m_pSoundTouch->setRate(m_dBaseRate);
    m_pSoundTouch->setPitch(1.0);
    m_pSoundTouch->setSetting(SETTING_USE_QUICKSEEK, 1);
    // Initialize the internal buffers to prevent re-allocations
    // in the real-time thread.
    onSignalChanged();
}

EngineBufferScaleST::~EngineBufferScaleST() {
}

void EngineBufferScaleST::setScaleParameters(double base_rate,
                                             double* pTempoRatio,
                                             double* pPitchRatio) {

    // Negative speed means we are going backwards. pitch does not affect
    // the playback direction.
    m_bBackwards = *pTempoRatio < 0;

    // It's an error to pass a rate or tempo smaller than MIN_SEEK_SPEED to
    // SoundTouch (see definition of MIN_SEEK_SPEED for more details).
    double speed_abs = fabs(*pTempoRatio);
    if (speed_abs > MAX_SEEK_SPEED) {
        speed_abs = MAX_SEEK_SPEED;
    } else if (speed_abs < MIN_SEEK_SPEED) {
        speed_abs = 0;
    }

    // Let the caller know if we clamped their value.
    *pTempoRatio = m_bBackwards ? -speed_abs : speed_abs;

    // Include baserate in rate_abs so that we do samplerate conversion as part
    // of rate adjustment.
    if (speed_abs != m_dTempoRatio) {
        // Note: A rate of zero would make Soundtouch crash,
        // this is caught in scaleBuffer()
        m_pSoundTouch->setTempo(speed_abs);
        m_dTempoRatio = speed_abs;
    }
    if (base_rate != m_dBaseRate) {
        m_pSoundTouch->setRate(base_rate);
        m_dBaseRate = base_rate;
    }

    if (*pPitchRatio != m_dPitchRatio) {
        // Note: pitch ratio must be positive
        double pitch = fabs(*pPitchRatio);
        if (pitch > 0.0) {
            m_pSoundTouch->setPitch(pitch);
        }
        m_dPitchRatio = *pPitchRatio;
    }
    updateAntiAliasFilter();
    m_positionOffsetTarget = positionOffsetFrames();

    // NOTE(rryan) : There used to be logic here that clear()'d when the player
    // changed direction. I removed it because this is handled by EngineBuffer.
}

void EngineBufferScaleST::updateAntiAliasFilter() {
    // SoundTouch resamples by rate * pitch
    const double rate = m_dBaseRate * fabs(m_dPitchRatio);
    const bool resampling = fabs(rate - 1.0) > 1e-9;
    if (resampling != m_bAntiAliasFilter) {
        // Only sets a flag in SoundTouch: real-time safe
        m_pSoundTouch->setSetting(SETTING_USE_AA_FILTER, resampling ? 1 : 0);
        m_bAntiAliasFilter = resampling;
    }
}

double EngineBufferScaleST::positionOffsetFrames() const {
    if (SoundTouch::getVersionId() < 20302 || !getOutputSignal().isValid() ||
            m_dTempoRatio == 0.0 || m_dPitchRatio == 0.0) {
        // Only measured with SoundTouch 2.3.3
        return 0.0;
    }
    // SoundTouch stretches time with WSOLA: it copies sequences of
    // seekWindowLength input frames 1:1 to the output and moves its input
    // pointer by tau * (seekWindowLength - overlapLength) per sequence. Since
    // 2.3.0 the start of each sequence is aligned with the nominal position,
    // but inside the sequence the input advances by 1 per output frame instead
    // of tau, so the heard audio drifts away by (1 - tau) per output frame and
    // snaps back at the next sequence. Averaged over a sequence (including the
    // cross-faded overlap) the heard position differs from the nominal one by
    //   (1 - tau) * seekWindowLength / 2     (in stretcher input frames).
    // Converted to track frames and output-domain sequence lengths:
    //   (pitch - tempo) * baseRate * (outputSequence + overlapOut) / 2
    // Measured with a position-encoded test signal (SoundTouch 2.3.3, quick
    // seek, auto sequence, 48 kHz): the uncompensated error was -4.8 ms at
    // tempo 0.9, +3.7 ms at 1.1, -9.9 ms at 0.8, -45 ms at 0.5, +9.8 ms at 1.5;
    // this model matches within 0.7 ms for tempo 0.5 ... 2.0, independent of
    // pitch and of the track/output sample rate ratio.
    // Positive result: the heard audio is ahead of the consumed position.
    const double pitch = fabs(m_dPitchRatio);
    const double stRate = m_dBaseRate * pitch; // rate of SoundTouch's transposer
    // Same rounding as TDStretch::calculateOverlapLength()
    int overlapFrames = static_cast<int>(getOutputSignal().getSampleRate().value()) *
            m_pSoundTouch->getSetting(SETTING_OVERLAP_MS) / 1000;
    overlapFrames = math_max(overlapFrames, 16);
    overlapFrames -= overlapFrames % 8;
    // Above rate 1.0 the transposer runs after the stretcher and shortens
    // the sequences; SETTING_NOMINAL_OUTPUT_SEQUENCE already accounts for it.
    const double sequenceOut =
            m_pSoundTouch->getSetting(SETTING_NOMINAL_OUTPUT_SEQUENCE) +
            overlapFrames / math_max(1.0, stRate);
    return (pitch - m_dTempoRatio) * m_dBaseRate * sequenceOut / 2;
}

void EngineBufferScaleST::onSignalChanged() {
    int backBufferSize = kBackBufferFrameSize * getOutputSignal().getChannelCount();
    if (m_bufferBack.size() == backBufferSize) {
        m_bufferBack.clear();
    } else {
        m_bufferBack = mixxx::SampleBuffer(backBufferSize);
    }
    if (!getOutputSignal().isValid()) {
        return;
    }
    m_pSoundTouch->setSampleRate(getOutputSignal().getSampleRate());
    m_pSoundTouch->setChannels(getOutputSignal().getChannelCount());

    // Setting the tempo to a very low value will force SoundTouch
    // to preallocate buffers large enough to (almost certainly)
    // avoid memory reallocations during playback.
    m_pSoundTouch->setTempo(0.1);
    m_pSoundTouch->setTempo(m_dTempoRatio);
    clear();
    m_positionOffsetTarget = positionOffsetFrames();
}

void EngineBufferScaleST::clear() {
    m_pSoundTouch->clear();
    // After a clear the first sequence starts aligned, the offset builds up
    // again from 0 while the reported position slews towards the target.
    m_positionOffsetApplied = 0.0;

    // compensate seek offset for a rate of 1.0
    if (SoundTouch::getVersionId() < 20302) {
        DEBUG_ASSERT(SoundTouch::getVersionId() >= 20101);
        // from SoundTouch 2.3.0 the initial offset is corrected internally
        m_effectiveRate = m_dBaseRate * m_dTempoRatio;
        SampleUtil::clear(m_bufferBack.data(), m_bufferBack.size());
        m_pSoundTouch->putSamples(m_bufferBack.data(), kSeekOffsetFramesV20101);
    }
}

double EngineBufferScaleST::scaleBuffer(
        CSAMPLE* pOutputBuffer,
        SINT iOutputBufferSize) {
    if (m_dBaseRate == 0.0 || m_dTempoRatio == 0.0 || m_dPitchRatio == 0.0) {
        SampleUtil::clear(pOutputBuffer, iOutputBufferSize);
        // No actual samples/frames have been read from the
        // unscaled input buffer!
        return 0.0;
    }

    double readFramesProcessed = 0;
    SINT remaining_frames = getOutputSignal().samples2frames(iOutputBufferSize);
    CSAMPLE* read = pOutputBuffer;
    bool last_read_failed = false;
    while (remaining_frames > 0) {
        SINT received_frames = m_pSoundTouch->receiveSamples(
                read, remaining_frames);
        DEBUG_ASSERT(remaining_frames >= received_frames);
        remaining_frames -= received_frames;
        readFramesProcessed += m_effectiveRate * received_frames;
        read += getOutputSignal().frames2samples(received_frames);

        if (remaining_frames > 0) {
            // The requested setting becomes effective after all previous frames have been processed
            m_effectiveRate = m_dBaseRate * m_dTempoRatio;
            SINT iAvailSamples = m_pReadAheadManager->getNextSamples(
                    // The value doesn't matter here. All that matters is we
                    // are going forward or backward.
                    (m_bBackwards ? -1.0 : 1.0) * m_effectiveRate,
                    m_bufferBack.data(),
                    m_bufferBack.size(),
                    getOutputSignal().getChannelCount());
            SINT iAvailFrames = getOutputSignal().samples2frames(iAvailSamples);

            if (iAvailFrames > 0) {
                last_read_failed = false;
                m_pSoundTouch->putSamples(m_bufferBack.data(), iAvailFrames);
            } else {
                // We may get 0 samples once if we just hit a loop trigger, e.g.
                // when reloop_toggle jumps back to loop_in, or when moving a
                // loop causes the play position to be moved along.
                if (last_read_failed) {
                    // If we get 0 samples repeatedly, add silence that allows
                    // to flush the last samples out of Soundtouch.
                    // m_pSoundTouch->flush() must not be used, because it allocates
                    // a temporary buffer in the heap which maybe locking
                    qDebug() << "ReadAheadManager::getNextSamples() returned "
                                "zero samples repeatedly. Padding with silence.";
                    SampleUtil::clear(m_bufferBack.data(), m_bufferBack.size());
                    // putSamples() takes frames, not samples
                    m_pSoundTouch->putSamples(m_bufferBack.data(),
                            getOutputSignal().samples2frames(m_bufferBack.size()));
                }
                last_read_failed = true;
            }
        }
    }

    // Move the reported position towards the audio that is actually heard.
    // The step is limited and proportional to the consumed frames, so the
    // result is never negative (ReadAheadManager can't go back) and the
    // reported position never jumps.
    if (readFramesProcessed > 0) {
        const double maxStep = kMaxPositionOffsetSlew * readFramesProcessed;
        const double step = math_clamp(
                m_positionOffsetTarget - m_positionOffsetApplied, -maxStep, maxStep);
        m_positionOffsetApplied += step;
        readFramesProcessed += step;
    }

    // readFramesProcessed is interpreted as the total number of frames
    // consumed to produce the scaled buffer. Due to this, we do not take into
    // account directionality or starting point.
    return readFramesProcessed;
}
