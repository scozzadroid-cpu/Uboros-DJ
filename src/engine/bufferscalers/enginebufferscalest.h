#pragma once

#include <memory>

#include "engine/bufferscalers/enginebufferscale.h"
#include "util/samplebuffer.h"

class ReadAheadManager;

namespace soundtouch {
class SoundTouch;
}  // namespace soundtouch

// Uses libsoundtouch to scale audio.
class EngineBufferScaleST : public EngineBufferScale {
    Q_OBJECT
  public:
    explicit EngineBufferScaleST(
            ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleST() override;

    void setScaleParameters(double base_rate,
                            double* pTempoRatio,
                            double* pPitchRatio) override;

    // Scale buffer.
    double scaleBuffer(
            CSAMPLE* pOutputBuffer,
            SINT iOutputBufferSize) override;

    // Flush buffer.
    void clear() override;

  private:
    void onSignalChanged() override;

    // The read-ahead manager that we use to fetch samples
    ReadAheadManager* m_pReadAheadManager;

    // SoundTouch time/pitch scaling lib
    std::unique_ptr<soundtouch::SoundTouch> m_pSoundTouch;

    // Temporary buffer for reading from the RAMAN.
    mixxx::SampleBuffer m_bufferBack;

    // Holds the playback direction.
    bool m_bBackwards;

    // SoundTouch runs its anti-alias FIR filter even when it does not
    // resample (rate 1.0, e.g. keylock at the original key on a file with the
    // engine sample rate). On ARM its multichannel path is plain C: with an
    // 8-channel stem deck it took 40% of the audio thread and caused
    // underruns. Enabled only when the rate is not 1.0.
    void updateAntiAliasFilter();
    bool m_bAntiAliasFilter;

    // Average offset (track frames) between the audio SoundTouch outputs and
    // the position given by the consumed frames, for the current parameters.
    // The position reported to EngineBuffer is corrected by this value, like
    // the start delay compensation of EngineBufferScaleRubberBand, so the play
    // position and the beat distance used by sync match the audio.
    double positionOffsetFrames() const;
    double m_positionOffsetTarget;
    double m_positionOffsetApplied;
};
