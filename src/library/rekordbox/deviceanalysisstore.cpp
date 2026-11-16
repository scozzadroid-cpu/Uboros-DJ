#include "library/rekordbox/deviceanalysisstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtEndian>
#include <QtDebug>

#include "library/dao/analysisdao.h"
#include "track/beats.h"
#include "track/keyfactory.h"
#include "track/track.h"

namespace mixxx::deviceanalysis {

namespace {

const QByteArray kMagic = QByteArrayLiteral("MXA1");
constexpr int kFormatVersion = 1;
constexpr int kMaxRootSearchDepth = 12;
// FAT stores modification times with a 2 s resolution
constexpr qint64 kMTimeToleranceSecs = 2;
// Sanity limit when reading a sidecar
constexpr qint64 kMaxSidecarSize = 64 * 1024 * 1024;

const QString kStoreDir = QStringLiteral("MIXXX/ANLZ");

QString relativePath(const QString& root, const QString& location) {
    return QDir(root).relativeFilePath(location);
}

QString sidecarPath(const QString& root, const QString& relPath) {
    const QByteArray hash = QCryptographicHash::hash(
            relPath.toUtf8(), QCryptographicHash::Sha1)
                                    .toHex()
                                    .left(24);
    return root + QChar('/') + kStoreDir + QChar('/') +
            QString::fromLatin1(hash) + QStringLiteral(".mxa");
}

struct Sidecar {
    QJsonObject header;
    QByteArray waveform;        // compressed
    QByteArray waveformSummary; // compressed
};

bool readSidecar(const QString& path, Sidecar* pSidecar) {
    QFile file(path);
    if (!file.exists() || file.size() > kMaxSidecarSize ||
            !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 8 || !data.startsWith(kMagic)) {
        qWarning() << "Device analysis: not a sidecar" << path;
        return false;
    }
    const quint32 headerLength = qFromLittleEndian<quint32>(data.constData() + 4);
    if (8 + static_cast<qint64>(headerLength) > data.size()) {
        qWarning() << "Device analysis: truncated sidecar" << path;
        return false;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data.mid(8, headerLength), &error);
    if (!doc.isObject()) {
        qWarning() << "Device analysis: invalid sidecar header" << path << error.errorString();
        return false;
    }
    pSidecar->header = doc.object();
    if (pSidecar->header.value(QStringLiteral("format")).toInt() != kFormatVersion) {
        return false;
    }
    const qint64 blobsStart = 8 + headerLength;
    const auto blobs = pSidecar->header.value(QStringLiteral("blobs")).toObject();
    for (auto it = blobs.constBegin(); it != blobs.constEnd(); ++it) {
        const QJsonObject blob = it.value().toObject();
        const qint64 offset = blobsStart + blob.value(QStringLiteral("offset")).toInteger();
        const qint64 length = blob.value(QStringLiteral("length")).toInteger();
        if (offset < blobsStart || length < 0 || offset + length > data.size()) {
            qWarning() << "Device analysis: invalid blob in" << path;
            return false;
        }
        if (it.key() == QStringLiteral("waveform")) {
            pSidecar->waveform = data.mid(offset, length);
        } else if (it.key() == QStringLiteral("waveformSummary")) {
            pSidecar->waveformSummary = data.mid(offset, length);
        }
    }
    return true;
}

/// The sidecar describes this very file (same relative path, size and
/// modification time)
bool matchesFile(const QJsonObject& header, const QString& relPath, const QFileInfo& fileInfo) {
    if (header.value(QStringLiteral("path")).toString() != relPath) {
        return false;
    }
    if (header.value(QStringLiteral("size")).toInteger() != fileInfo.size()) {
        return false;
    }
    const qint64 mtime = header.value(QStringLiteral("mtime")).toInteger();
    return qAbs(mtime - fileInfo.lastModified().toSecsSinceEpoch()) <= kMTimeToleranceSecs;
}

bool hasCachedWaveform(TrackId trackId, AnalysisDao& analysisDao) {
    return !analysisDao.getAnalysesForTrackByType(trackId, AnalysisDao::TYPE_WAVEFORM)
                    .isEmpty();
}

/// Builds the sidecar for the track, or returns false when there is nothing
/// worth storing (no beats and no waveform)
bool buildSidecar(const TrackPointer& pTrack,
        AnalysisDao& analysisDao,
        const QString& relPath,
        const QFileInfo& fileInfo,
        QByteArray* pOut) {
    QJsonObject header;
    header.insert(QStringLiteral("format"), kFormatVersion);
    header.insert(QStringLiteral("generator"), QStringLiteral("Mixxx"));
    header.insert(QStringLiteral("path"), relPath);
    header.insert(QStringLiteral("size"), fileInfo.size());
    header.insert(QStringLiteral("mtime"), fileInfo.lastModified().toSecsSinceEpoch());

    bool hasData = false;
    const BeatsPointer pBeats = pTrack->getBeats();
    if (pBeats) {
        QJsonObject beats;
        beats.insert(QStringLiteral("version"), pBeats->getVersion());
        beats.insert(QStringLiteral("subVersion"), pBeats->getSubVersion());
        beats.insert(QStringLiteral("data"), QString::fromLatin1(pBeats->toByteArray().toBase64()));
        header.insert(QStringLiteral("beats"), beats);
        header.insert(QStringLiteral("bpmLocked"), pTrack->isBpmLocked());
        hasData = true;
    }
    const Keys keys = pTrack->getKeys();
    if (keys.getGlobalKey() != mixxx::track::io::key::INVALID) {
        QJsonObject keysObject;
        keysObject.insert(QStringLiteral("version"), keys.getVersion());
        keysObject.insert(QStringLiteral("subVersion"), keys.getSubVersion());
        keysObject.insert(QStringLiteral("data"), QString::fromLatin1(keys.toByteArray().toBase64()));
        header.insert(QStringLiteral("keys"), keysObject);
    }
    QJsonArray cues;
    for (const CuePointer& pCue : pTrack->getCuePoints()) {
        const auto type = pCue->getType();
        if (type == CueType::Invalid || type == CueType::N60dBSound) {
            continue; // recomputed by the analysis
        }
        QJsonObject cue;
        cue.insert(QStringLiteral("type"), static_cast<int>(type));
        cue.insert(QStringLiteral("hotcue"), pCue->getHotCue());
        const auto position = pCue->getPosition();
        const auto endPosition = pCue->getEndPosition();
        cue.insert(QStringLiteral("position"), position.isValid() ? position.value() : -1.0);
        cue.insert(QStringLiteral("end"), endPosition.isValid() ? endPosition.value() : -1.0);
        cue.insert(QStringLiteral("color"), static_cast<qint64>(static_cast<RgbColor::code_t>(pCue->getColor())));
        cue.insert(QStringLiteral("label"), pCue->getLabel());
        cues.append(cue);
    }
    header.insert(QStringLiteral("cues"), cues);

    QByteArray blobs;
    QJsonObject blobIndex;
    const auto addBlob = [&](const QString& name, AnalysisDao::AnalysisType type) {
        const auto analyses = analysisDao.getAnalysesForTrackByType(pTrack->getId(), type);
        if (analyses.isEmpty() || analyses.first().data.isEmpty()) {
            return;
        }
        const auto& analysis = analyses.first();
        const QByteArray compressed = qCompress(analysis.data, 6);
        QJsonObject blob;
        blob.insert(QStringLiteral("version"), analysis.version);
        blob.insert(QStringLiteral("description"), analysis.description);
        blob.insert(QStringLiteral("offset"), blobs.size());
        blob.insert(QStringLiteral("length"), compressed.size());
        blobIndex.insert(name, blob);
        blobs.append(compressed);
        hasData = true;
    };
    addBlob(QStringLiteral("waveform"), AnalysisDao::TYPE_WAVEFORM);
    addBlob(QStringLiteral("waveformSummary"), AnalysisDao::TYPE_WAVESUMMARY);
    header.insert(QStringLiteral("blobs"), blobIndex);

    if (!hasData) {
        return false;
    }
    const QByteArray headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
    pOut->clear();
    pOut->append(kMagic);
    char length[4];
    qToLittleEndian<quint32>(static_cast<quint32>(headerJson.size()), length);
    pOut->append(length, 4);
    pOut->append(headerJson);
    pOut->append(blobs);
    return true;
}

} // namespace

QString deviceRootForFile(const QString& location) {
    QDir dir = QFileInfo(location).absoluteDir();
    for (int depth = 0; depth < kMaxRootSearchDepth; ++depth) {
        if (QFileInfo(dir.filePath(QStringLiteral("PIONEER"))).isDir()) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

bool importIfAvailable(const TrackPointer& pTrack, AnalysisDao& analysisDao) {
    if (!pTrack || !pTrack->getId().isValid()) {
        return false;
    }
    const QString location = pTrack->getLocation();
    const QString root = deviceRootForFile(location);
    if (root.isEmpty() || hasCachedWaveform(pTrack->getId(), analysisDao)) {
        return false;
    }
    const QString relPath = relativePath(root, location);
    Sidecar sidecar;
    if (!readSidecar(sidecarPath(root, relPath), &sidecar)) {
        return false;
    }
    if (!matchesFile(sidecar.header, relPath, QFileInfo(location))) {
        qInfo() << "Device analysis: outdated sidecar ignored for" << relPath;
        return false;
    }

    const QJsonObject beats = sidecar.header.value(QStringLiteral("beats")).toObject();
    if (!beats.isEmpty() && !pTrack->isBpmLocked() && pTrack->getSampleRate().isValid()) {
        const auto pBeats = Beats::fromByteArray(pTrack->getSampleRate(),
                beats.value(QStringLiteral("version")).toString(),
                beats.value(QStringLiteral("subVersion")).toString(),
                QByteArray::fromBase64(beats.value(QStringLiteral("data")).toString().toLatin1()));
        if (pBeats) {
            pTrack->trySetBeats(pBeats);
            if (sidecar.header.value(QStringLiteral("bpmLocked")).toBool()) {
                pTrack->setBpmLocked(true);
            }
        }
    }
    const QJsonObject keys = sidecar.header.value(QStringLiteral("keys")).toObject();
    if (!keys.isEmpty()) {
        QByteArray keysData = QByteArray::fromBase64(
                keys.value(QStringLiteral("data")).toString().toLatin1());
        const Keys loadedKeys = KeyFactory::loadKeysFromByteArray(
                keys.value(QStringLiteral("version")).toString(),
                keys.value(QStringLiteral("subVersion")).toString(),
                &keysData);
        if (loadedKeys.getGlobalKey() != mixxx::track::io::key::INVALID) {
            pTrack->setKeys(loadedKeys);
        }
    }
    // Cues: add those the track does not have yet (the ones imported from
    // rekordbox are usually the same)
    const QList<CuePointer> existingCues = pTrack->getCuePoints();
    for (const auto& value : sidecar.header.value(QStringLiteral("cues")).toArray()) {
        const QJsonObject cue = value.toObject();
        const auto type = static_cast<CueType>(cue.value(QStringLiteral("type")).toInt());
        const int hotcue = cue.value(QStringLiteral("hotcue")).toInt(-1);
        const double position = cue.value(QStringLiteral("position")).toDouble(-1);
        const double end = cue.value(QStringLiteral("end")).toDouble(-1);
        const bool exists = std::any_of(existingCues.cbegin(),
                existingCues.cend(),
                [&](const CuePointer& pCue) {
                    if (pCue->getType() != type) {
                        return false;
                    }
                    if (type == CueType::HotCue) {
                        return pCue->getHotCue() == hotcue;
                    }
                    // A track has a single main cue, intro and outro
                    return type == CueType::MainCue || type == CueType::Intro ||
                            type == CueType::Outro ||
                            (pCue->getPosition().isValid() &&
                                    qAbs(pCue->getPosition().value() - position) < 1);
                });
        if (exists || type == CueType::Invalid) {
            continue;
        }
        const auto color = RgbColor::optional(static_cast<RgbColor::code_t>(
                cue.value(QStringLiteral("color")).toInteger()));
        CuePointer pCue = pTrack->createAndAddCue(type,
                hotcue,
                position >= 0 ? audio::FramePos(position) : audio::FramePos(),
                end >= 0 ? audio::FramePos(end) : audio::FramePos(),
                color.value_or(PredefinedColorPalettes::kDefaultCueColor));
        if (pCue) {
            pCue->setLabel(cue.value(QStringLiteral("label")).toString());
        }
    }

    const auto blobs = sidecar.header.value(QStringLiteral("blobs")).toObject();
    const auto saveBlob = [&](const QString& name,
                                  const QByteArray& compressed,
                                  AnalysisDao::AnalysisType type) {
        if (compressed.isEmpty()) {
            return;
        }
        const QJsonObject blob = blobs.value(name).toObject();
        AnalysisDao::AnalysisInfo info;
        info.trackId = pTrack->getId();
        info.type = type;
        info.version = blob.value(QStringLiteral("version")).toString();
        info.description = blob.value(QStringLiteral("description")).toString();
        info.data = qUncompress(compressed);
        if (!info.data.isEmpty()) {
            analysisDao.saveAnalysis(&info);
        }
    };
    saveBlob(QStringLiteral("waveform"), sidecar.waveform, AnalysisDao::TYPE_WAVEFORM);
    saveBlob(QStringLiteral("waveformSummary"),
            sidecar.waveformSummary,
            AnalysisDao::TYPE_WAVESUMMARY);
    qInfo() << "Device analysis: imported" << relPath;
    return true;
}

bool needsExport(const TrackPointer& pTrack, AnalysisDao& analysisDao) {
    if (!pTrack || !pTrack->getId().isValid()) {
        return false;
    }
    const QString location = pTrack->getLocation();
    const QString root = deviceRootForFile(location);
    if (root.isEmpty() || !hasCachedWaveform(pTrack->getId(), analysisDao)) {
        return false;
    }
    const QString relPath = relativePath(root, location);
    QByteArray content;
    if (!buildSidecar(pTrack, analysisDao, relPath, QFileInfo(location), &content)) {
        return false;
    }
    QFile existing(sidecarPath(root, relPath));
    if (!existing.open(QIODevice::ReadOnly)) {
        return true;
    }
    return existing.readAll() != content;
}

ExportResult exportAnalysis(const TrackPointer& pTrack, AnalysisDao& analysisDao) {
    if (!pTrack || !pTrack->getId().isValid()) {
        return ExportResult::NothingToExport;
    }
    const QString location = pTrack->getLocation();
    const QString root = deviceRootForFile(location);
    if (root.isEmpty()) {
        return ExportResult::NotOnDevice;
    }
    const QString relPath = relativePath(root, location);
    QByteArray content;
    if (!buildSidecar(pTrack, analysisDao, relPath, QFileInfo(location), &content)) {
        return ExportResult::NothingToExport;
    }
    const QString path = sidecarPath(root, relPath);
    {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == content) {
            return ExportResult::Unchanged;
        }
    }
    QDir rootDir(root);
    if (!QFileInfo(rootDir.filePath(kStoreDir)).isDir() && !rootDir.mkpath(kStoreDir)) {
        // Mounted read-only (or no permission): not an error
        return ExportResult::ReadOnly;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return ExportResult::ReadOnly;
    }
    if (file.write(content) != content.size() || !file.commit()) {
        qWarning() << "Device analysis: writing failed for" << relPath << file.errorString();
        return ExportResult::Failed;
    }
    qInfo() << "Device analysis: stored" << relPath << content.size() << "bytes";
    return ExportResult::Written;
}

} // namespace mixxx::deviceanalysis
