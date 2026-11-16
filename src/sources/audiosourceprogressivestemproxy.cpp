#include "sources/audiosourceprogressivestemproxy.h"

#include "engine/engine.h"
#include "stems/stemcache.h"
#include "util/logger.h"

namespace mixxx {

namespace {

const Logger kLogger("AudioSourceProgressiveStemProxy");

audio::SignalInfo stemSignalInfo(const audio::SignalInfo& stereo) {
    return audio::SignalInfo(audio::ChannelCount::stem(), stereo.getSampleRate());
}

} // namespace

AudioSourceProgressiveStemProxy::AudioSourceProgressiveStemProxy(
        AudioSourcePointer pStereoSource,
        std::shared_ptr<stems::StemCache> pCache)
        : AudioSourceProxy(std::move(pStereoSource),
                  stemSignalInfo(pStereoSource->getSignalInfo())),
          m_pCache(std::move(pCache)) {
    DEBUG_ASSERT(m_pAudioSource->getSignalInfo().getChannelCount() ==
            audio::ChannelCount::stereo());
}

ReadableSampleFrames AudioSourceProgressiveStemProxy::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    const SINT frames = sampleFrames.frameLength();
    if (m_mix.size() < frames * 2) {
        m_mix = SampleBuffer(frames * 2);
    }
    const auto readable = readSampleFramesClampedOn(*m_pAudioSource,
            WritableSampleFrames(sampleFrames.frameIndexRange(),
                    SampleBuffer::WritableSlice(m_mix.data(), frames * 2)));
    const IndexRange range = readable.frameIndexRange();
    if (range.empty()) {
        return ReadableSampleFrames(range);
    }
    const SINT offset = range.start() - sampleFrames.frameIndexRange().start();
    CSAMPLE* out = sampleFrames.writableData(offset * 8);
    m_pCache->readSlots(range.start(), range.length(), readable.readableData(), out);
    return ReadableSampleFrames(range,
            SampleBuffer::ReadableSlice(out, range.length() * 8));
}

} // namespace mixxx
