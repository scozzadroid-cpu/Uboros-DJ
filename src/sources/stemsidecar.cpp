#include "sources/stemsidecar.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

namespace mixxx {

namespace {
const QString kStemDir = QStringLiteral("MIXXX/STEMS");
const QString kStemSuffix = QStringLiteral(".mxstem");
constexpr int kMaxDepth = 8;
} // namespace

QString stemSidecarForTrack(const QString& trackLocation) {
    if (trackLocation.isEmpty()) {
        return {};
    }
    QDir dir = QFileInfo(trackLocation).absoluteDir();
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        const QString stemDir = dir.filePath(kStemDir);
        if (QFileInfo(stemDir).isDir()) {
            const QString relPath = dir.relativeFilePath(trackLocation);
            const QByteArray hash = QCryptographicHash::hash(
                    relPath.toUtf8(), QCryptographicHash::Sha1)
                                            .toHex()
                                            .left(24);
            const QString path = stemDir + QChar('/') +
                    QString::fromLatin1(hash) + kStemSuffix;
            return QFileInfo::exists(path) ? path : QString();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

} // namespace mixxx
