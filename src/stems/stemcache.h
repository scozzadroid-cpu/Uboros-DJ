#pragma once

#include <QFile>
#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>

namespace mixxx {
namespace stems {

/// The stems computed on the device for one track, in a memory mapped file
/// of the app cache (so they survive reloading the track and do not stay in
/// RAM). Written by the separation thread, read by the caching reader worker
/// (decoding) and checked by the engine (which chunks became ready).
///
/// Layout: header, one ready flag per reader chunk (kChunkFrames frames),
/// then frames × 6 int16 (drums L R, bass + melody L R, vocals L R).
class StemCache {
  public:
    /// Frames per ready flag = CachingReaderChunk::kFrames, so that a chunk
    /// of the reader is either all stems or all mix
    static constexpr long kChunkFrames = 8192;

    /// Opens (or creates) the cache of a track. nullptr on failure.
    static std::shared_ptr<StemCache> open(const QString& trackLocation,
            long frames,
            int sampleRate);
    ~StemCache();

    long frames() const {
        return m_frames;
    }
    long chunkCount() const {
        return (m_frames + kChunkFrames - 1) / kChunkFrames;
    }
    QString path() const {
        return m_file.fileName();
    }

    /// Quality of the stems of a chunk, kept in the ready flag of the file
    enum Level : uint8_t {
        kNotReady = 0,
        kLight = 1,    // Spleeter, for slow phones
        kBalanced = 2, // htdemucs
    };

    /// Separation thread: stores final stems (6 floats per frame) and marks
    /// the chunks that are now complete as ready at this level (never lower
    /// than the level a chunk already has)
    void write(long start, long count, const float* stems, Level level);
    /// Separation thread: a new pass over the track starts; the frames
    /// written per chunk are counted from zero again
    void beginPass();

    bool isChunkReady(long chunk) const;
    Level chunkLevel(long chunk) const;
    /// Lowest level among the chunks of the frames [start, end)
    Level minLevel(long start, long end) const;
    bool isFrameReady(long frame) const {
        return frame >= 0 && frame < m_frames && isChunkReady(frame / kChunkFrames);
    }
    long readyChunkCount() const;
    /// Changes whenever chunks become ready (the engine polls it)
    uint32_t generation() const {
        return m_generation.load(std::memory_order_acquire);
    }

    /// Reader: the 8 channels (4 stereo slots) of [start, start + count).
    /// Ready frames get the stems (slot 3 silent), the others the mix in
    /// slot 2 (bass + melody) so that they sound like the plain track.
    void readSlots(long start, long count, const float* mixStereo, float* out8) const;

  private:
    StemCache(const QString& path, long frames);
    bool map(int sampleRate);

    QFile m_file;
    long m_frames;
    uint8_t* m_pReady = nullptr; // chunkCount() flags
    int16_t* m_pData = nullptr;  // frames × 6
    // Frames written per chunk (a chunk is ready when all its frames are)
    std::unique_ptr<std::atomic<int32_t>[]> m_chunkFill;
    std::atomic<uint32_t> m_generation{0};
};

} // namespace stems
} // namespace mixxx
