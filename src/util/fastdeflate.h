#pragma once

#include <QByteArray>

namespace mixxx {

/// Drop-in replacements of qCompress() / qUncompress(): the same format (4 byte
/// big endian length of the original data, then a zlib stream), so the data
/// already stored (analysis files, sidecars on the drives) reads and writes
/// both ways. On Android they use libdeflate (src/lib/libdeflate): on the
/// POCO F4 GT, with real analysis files, compression at level 6 is 4.2x
/// faster, at level 1 2.25x, decompression 1.9x. Elsewhere, and for any input
/// that libdeflate does not accept, they call Qt, so the result is never worse.
///
/// level: 0-9 like zlib, -1 = default (6).
QByteArray fastCompress(const QByteArray& data, int level = -1);
QByteArray fastUncompress(const QByteArray& data);

} // namespace mixxx
