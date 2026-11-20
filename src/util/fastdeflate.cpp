#include "util/fastdeflate.h"

#include <QtEndian>

// MIXXX_FASTDEFLATE: forces the libdeflate path on the PC (the test of
// tools/fastdeflate-test); the app uses it on Android only.
#if defined(__ANDROID__) || defined(MIXXX_FASTDEFLATE)
#include <libdeflate.h>

namespace mixxx {

namespace {

// More than this is a damaged length field (the analyses are a few MB): let Qt decide (256 MiB)
constexpr quint32 kMaxExpectedSize = 1u << 28;

struct CompressorCache {
    libdeflate_compressor* pCompressor = nullptr;
    int level = -1;
    ~CompressorCache() {
        if (pCompressor) {
            libdeflate_free_compressor(pCompressor);
        }
    }
};

struct DecompressorCache {
    libdeflate_decompressor* pDecompressor = nullptr;
    ~DecompressorCache() {
        if (pDecompressor) {
            libdeflate_free_decompressor(pDecompressor);
        }
    }
};

} // namespace

QByteArray fastCompress(const QByteArray& data, int level) {
    if (data.isEmpty() || data.size() > static_cast<qsizetype>(kMaxExpectedSize)) {
        return qCompress(data, level);
    }
    if (level < 0) {
        level = 6; // what Qt/zlib call the default
    }
    if (level > 9) {
        level = 9;
    }
    // One compressor per thread: allocating one costs more than compressing a small blob
    thread_local CompressorCache cache;
    if (!cache.pCompressor || cache.level != level) {
        if (cache.pCompressor) {
            libdeflate_free_compressor(cache.pCompressor);
        }
        cache.pCompressor = libdeflate_alloc_compressor(level);
        cache.level = level;
    }
    if (!cache.pCompressor) {
        return qCompress(data, level);
    }
    const size_t bound = libdeflate_zlib_compress_bound(cache.pCompressor, static_cast<size_t>(data.size()));
    QByteArray out(static_cast<qsizetype>(bound + 4), Qt::Uninitialized);
    qToBigEndian<quint32>(static_cast<quint32>(data.size()), out.data());
    const size_t written = libdeflate_zlib_compress(cache.pCompressor,
            data.constData(),
            static_cast<size_t>(data.size()),
            out.data() + 4,
            bound);
    if (written == 0) {
        return qCompress(data, level);
    }
    out.resize(static_cast<qsizetype>(written + 4));
    return out;
}

QByteArray fastUncompress(const QByteArray& data) {
    if (data.size() <= 4) {
        return qUncompress(data);
    }
    const quint32 expected = qFromBigEndian<quint32>(data.constData());
    if (expected == 0 || expected > kMaxExpectedSize) {
        return qUncompress(data);
    }
    thread_local DecompressorCache cache;
    if (!cache.pDecompressor) {
        cache.pDecompressor = libdeflate_alloc_decompressor();
    }
    if (!cache.pDecompressor) {
        return qUncompress(data);
    }
    QByteArray out(static_cast<qsizetype>(expected), Qt::Uninitialized);
    size_t actual = 0;
    const libdeflate_result result = libdeflate_zlib_decompress(cache.pDecompressor,
            data.constData() + 4,
            static_cast<size_t>(data.size() - 4),
            out.data(),
            static_cast<size_t>(expected),
            &actual);
    if (result == LIBDEFLATE_BAD_DATA) {
        // Damaged data: qUncompress's answer (empty), without a second allocation
        return QByteArray();
    }
    if (result != LIBDEFLATE_SUCCESS || actual != expected) {
        // Wrong length field, damaged or truncated data: Qt's answer (it also
        // grows the buffer if the field is too small and logs the failure)
        return qUncompress(data);
    }
    return out;
}

} // namespace mixxx

#else // not Android: zlib through Qt

namespace mixxx {

QByteArray fastCompress(const QByteArray& data, int level) {
    return qCompress(data, level);
}

QByteArray fastUncompress(const QByteArray& data) {
    return qUncompress(data);
}

} // namespace mixxx

#endif
