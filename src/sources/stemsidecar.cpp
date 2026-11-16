#include "sources/stemsidecar.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QSet>
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

namespace {

QString hashedName(const QString& root, const QString& trackLocation) {
    const QString relPath = QDir(root).relativeFilePath(trackLocation);
    return QString::fromLatin1(
                   QCryptographicHash::hash(relPath.toUtf8(), QCryptographicHash::Sha1)
                           .toHex()
                           .left(24)) +
            kStemSuffix;
}

// Directory of a track -> drive root with MIXXX/STEMS ("" = none)
QString stemRootForDir(const QString& trackDir) {
    static QHash<QString, QString> s_rootByDir;
    const auto it = s_rootByDir.constFind(trackDir);
    if (it != s_rootByDir.constEnd()) {
        return it.value();
    }
    QString root;
    QDir dir(trackDir);
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        if (QFileInfo(dir.filePath(kStemDir)).isDir()) {
            root = dir.absolutePath();
            break;
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    s_rootByDir.insert(trackDir, root);
    return root;
}

} // namespace

bool trackHasStems(const QString& trackLocation) {
    if (trackLocation.endsWith(QStringLiteral(".stem.mp4"), Qt::CaseInsensitive) ||
            trackLocation.endsWith(QStringLiteral(".stem.m4a"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString root = stemRootForDir(QFileInfo(trackLocation).absolutePath());
    if (root.isEmpty()) {
        return false;
    }
    struct Listing {
        qint64 listedAtMs = 0;
        QSet<QString> names;
    };
    static QHash<QString, Listing> s_listings;
    Listing& listing = s_listings[root];
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - listing.listedAtMs > 10000) {
        const QStringList names = QDir(root + QChar('/') + kStemDir)
                                          .entryList({QStringLiteral("*") + kStemSuffix},
                                                  QDir::Files);
        listing.names = QSet<QString>(names.cbegin(), names.cend());
        listing.listedAtMs = now;
    }
    return listing.names.contains(hashedName(root, trackLocation));
}

} // namespace mixxx
