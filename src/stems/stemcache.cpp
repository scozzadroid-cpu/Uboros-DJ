#include "stems/stemcache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QHash>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>
#include <cstring>

#include "util/logger.h"

namespace mixxx {
namespace stems {

namespace {

const Logger kLogger("StemCache");

constexpr char kMagic[8] = {'M', 'X', 'S', 'T', 'E', 'M', 'C', '1'};
constexpr int kHeaderSize = 64;
constexpr int kSlots = 6;               // 3 stereo stems
constexpr float kScale = 16384.0f;      // int16 with 2x headroom (stems can exceed the mix)

struct Header {
    char magic[8];
    int64_t frames;
    int32_t sampleRate;
    int32_t chunkFrames;
};

QString cacheDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
            QStringLiteral("/stems");
}

// A track takes ~30 MB per minute: keep about 20 tracks, and never leave
// the phone with less than 2 GB free because of the stems
constexpr qint64 kMaxCacheBytes = qint64(3) << 30;
constexpr qint64 kMinFreeBytes = qint64(2) << 30;

QMutex s_openMutex;
QHash<QString, int> s_openPaths; // caches mapped right now (per deck), never removed

/// Removes the least recently used caches until the limits are met.
/// keepBytes: room needed for the cache about to be created.
void pruneCache(qint64 keepBytes) {
    QFileInfoList files = QDir(cacheDir()).entryInfoList(
            {QStringLiteral("*.stemcache")}, QDir::Files);
    std::sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.lastModified() < b.lastModified();
    });
    qint64 total = keepBytes;
    for (const auto& file : std::as_const(files)) {
        total += file.size();
    }
    qint64 free = QStorageInfo(cacheDir()).bytesAvailable() - keepBytes;
    QMutexLocker locker(&s_openMutex);
    for (const auto& file : std::as_const(files)) {
        if (total <= kMaxCacheBytes && free >= kMinFreeBytes) {
            break;
        }
        if (s_openPaths.contains(file.absoluteFilePath())) {
            continue;
        }
        if (QFile::remove(file.absoluteFilePath())) {
            kLogger.info() << "Removed the old stems" << file.fileName()
                           << file.size() / (1 << 20) << "MB";
            total -= file.size();
            free += file.size();
        }
    }
}

} // namespace

StemCache::StemCache(const QString& path, long frames)
        : m_file(path),
          m_frames(frames) {
}

StemCache::~StemCache() {
    if (m_pReady) {
        m_file.unmap(m_pReady - kHeaderSize);
    }
    m_file.close();
    QMutexLocker locker(&s_openMutex);
    const QString path = QFileInfo(m_file.fileName()).absoluteFilePath();
    if (--s_openPaths[path] <= 0) {
        s_openPaths.remove(path);
    }
}

std::shared_ptr<StemCache> StemCache::open(const QString& trackLocation,
        long frames,
        int sampleRate) {
    if (frames <= 0) {
        return nullptr;
    }
    QDir().mkpath(cacheDir());
    const QByteArray hash = QCryptographicHash::hash(
            trackLocation.toUtf8(), QCryptographicHash::Sha1)
                                    .toHex()
                                    .left(24);
    const QString path = cacheDir() + QChar('/') + QString::fromLatin1(hash) +
            QStringLiteral(".stemcache");
    {
        QMutexLocker locker(&s_openMutex);
        ++s_openPaths[QFileInfo(path).absoluteFilePath()];
    }
    if (!QFile::exists(path)) {
        pruneCache(kHeaderSize + qint64(frames) * kSlots * qint64(sizeof(int16_t)));
    }
    std::shared_ptr<StemCache> pCache(new StemCache(path, frames));
    if (!pCache->map(sampleRate)) {
        return nullptr;
    }
    // The modification time orders the caches by last use
    pCache->m_file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
    return pCache;
}

bool StemCache::map(int sampleRate) {
    const qint64 size = kHeaderSize + chunkCount() +
            qint64(m_frames) * kSlots * qint64(sizeof(int16_t));
    if (!m_file.open(QIODevice::ReadWrite)) {
        kLogger.warning() << "Cannot open" << m_file.fileName() << m_file.errorString();
        return false;
    }
    Header header;
    bool valid = false;
    if (m_file.size() == size &&
            m_file.read(reinterpret_cast<char*>(&header), sizeof(header)) == sizeof(header)) {
        valid = std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0 &&
                header.frames == m_frames && header.sampleRate == sampleRate &&
                header.chunkFrames == kChunkFrames;
    }
    if (!valid) {
        // New (or stale) cache: header, all chunks not ready
        if (!m_file.resize(0) || !m_file.resize(size)) {
            kLogger.warning() << "Cannot allocate" << size << "bytes for" << m_file.fileName();
            return false;
        }
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, kMagic, sizeof(kMagic));
        header.frames = m_frames;
        header.sampleRate = sampleRate;
        header.chunkFrames = kChunkFrames;
        m_file.seek(0);
        m_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        m_file.flush();
    }
    uchar* pMap = m_file.map(0, size);
    if (!pMap) {
        kLogger.warning() << "Cannot map" << m_file.fileName();
        return false;
    }
    m_pReady = pMap + kHeaderSize;
    m_pData = reinterpret_cast<int16_t*>(m_pReady + chunkCount());
    m_chunkFill = std::make_unique<std::atomic<int32_t>[]>(chunkCount());
    for (long c = 0; c < chunkCount(); ++c) {
        if (!isChunkReady(c)) {
            // A chunk only half written by an earlier session is redone
            m_pReady[c] = 0;
        }
        m_chunkFill[c].store(0, std::memory_order_relaxed);
    }
    kLogger.info() << (valid ? "Reopened" : "Created") << m_file.fileName()
                   << readyChunkCount() << "/" << chunkCount() << "chunks ready";
    return true;
}

bool StemCache::isChunkReady(long chunk) const {
    if (chunk < 0 || chunk >= chunkCount()) {
        return false;
    }
    return __atomic_load_n(&m_pReady[chunk], __ATOMIC_ACQUIRE) != 0;
}

long StemCache::readyChunkCount() const {
    long count = 0;
    for (long c = 0; c < chunkCount(); ++c) {
        count += isChunkReady(c) ? 1 : 0;
    }
    return count;
}

void StemCache::beginPass() {
    for (long c = 0; c < chunkCount(); ++c) {
        m_chunkFill[c].store(0, std::memory_order_relaxed);
    }
}

StemCache::Level StemCache::chunkLevel(long chunk) const {
    if (chunk < 0 || chunk >= chunkCount()) {
        return kNotReady;
    }
    return static_cast<Level>(__atomic_load_n(&m_pReady[chunk], __ATOMIC_ACQUIRE));
}

StemCache::Level StemCache::minLevel(long start, long end) const {
    start = std::max(0L, start);
    end = std::min(end, m_frames);
    if (start >= end) {
        return kBalanced;
    }
    Level level = kBalanced;
    for (long c = start / kChunkFrames; c <= (end - 1) / kChunkFrames; ++c) {
        level = std::min(level, chunkLevel(c));
    }
    return level;
}

void StemCache::write(long start, long count, const float* stems, Level level) {
    start = std::max(0L, start);
    count = std::min(count, m_frames - start);
    if (count <= 0) {
        return;
    }
    int16_t* dst = m_pData + start * kSlots;
    for (long i = 0; i < count * kSlots; ++i) {
        const float v = std::clamp(stems[i] * kScale, -32768.0f, 32767.0f);
        dst[i] = static_cast<int16_t>(v);
    }
    bool changed = false;
    for (long frame = start; frame < start + count;) {
        const long chunk = frame / kChunkFrames;
        const long chunkStart = chunk * kChunkFrames;
        const long chunkLength = std::min(kChunkFrames, m_frames - chunkStart);
        const long n = std::min(start + count, chunkStart + chunkLength) - frame;
        const int32_t filled = m_chunkFill[chunk].fetch_add(
                static_cast<int32_t>(n), std::memory_order_acq_rel) + static_cast<int32_t>(n);
        if (filled >= chunkLength && chunkLevel(chunk) < level) {
            // The data above is visible before the flag (release)
            __atomic_store_n(&m_pReady[chunk], static_cast<uint8_t>(level), __ATOMIC_RELEASE);
            changed = true;
        }
        frame += n;
    }
    if (changed) {
        m_generation.fetch_add(1, std::memory_order_acq_rel);
    }
}

void StemCache::readSlots(long start, long count, const float* mixStereo, float* out8) const {
    for (long i = 0; i < count; ++i) {
        const long frame = start + i;
        float* o = out8 + i * 8;
        if (isFrameReady(frame)) {
            const int16_t* s = m_pData + frame * kSlots;
            o[0] = s[0] / kScale; // drums
            o[1] = s[1] / kScale;
            o[2] = s[2] / kScale; // bass + melody
            o[3] = s[3] / kScale;
            o[4] = 0.0f;          // silent third slot
            o[5] = 0.0f;
            o[6] = s[4] / kScale; // vocals
            o[7] = s[5] / kScale;
        } else {
            o[0] = o[1] = 0.0f;
            o[2] = mixStereo[2 * i];
            o[3] = mixStereo[2 * i + 1];
            o[4] = o[5] = o[6] = o[7] = 0.0f;
        }
    }
}

} // namespace stems
} // namespace mixxx
