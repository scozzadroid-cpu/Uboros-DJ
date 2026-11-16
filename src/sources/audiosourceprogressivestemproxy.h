#pragma once

#include <memory>

#include "sources/audiosourceproxy.h"
#include "util/samplebuffer.h"

namespace mixxx {

namespace stems {
class StemCache;
}

/// Plays a plain stereo track as a stem track (4 stereo slots) whose stems
/// are computed on the device while it plays ("instant stems", see
/// ~/mixxx-android/stems/ISTANTANEI.md). Where the separation is not ready
/// yet, slot 2 (bass + melody) carries the whole mix and the others are
/// silent, so the deck sounds exactly like the plain track; the chunks of
/// StemCache that are ready carry drums, bass + melody and vocals.
class AudioSourceProgressiveStemProxy : public AudioSourceProxy {
  public:
    AudioSourceProgressiveStemProxy(
            AudioSourcePointer pStereoSource,
            std::shared_ptr<stems::StemCache> pCache);
    ~AudioSourceProgressiveStemProxy() override = default;

    const std::shared_ptr<stems::StemCache>& cache() const {
        return m_pCache;
    }

  protected:
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

  private:
    std::shared_ptr<stems::StemCache> m_pCache;
    SampleBuffer m_mix;
};

} // namespace mixxx
