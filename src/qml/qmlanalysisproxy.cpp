#include "qml/qmlanalysisproxy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QStorageInfo>

#include "library/analysis/analysisfeature.h"
#include "library/dao/analysisdao.h"
#include "library/library.h"
#include "library/rekordbox/deviceanalysisstore.h"
#include "library/rekordbox/rekordboxfeature.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playerinfo.h"
#include "moc_qmlanalysisproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

namespace {

struct Option {
    const char* name;
    ConfigKey key;
    bool defaultValue;
    // Stored as this integer when on (0 when off), e.g. the stem strategy
    // enums (Enforced = 2); 0 means a plain bool
    int intValueWhenOn;
};

const QList<Option>& optionTable() {
    static const QList<Option> kOptions = {
            {"bpmDetection",
                    ConfigKey(QStringLiteral("[BPM]"), QStringLiteral("BPMDetectionEnabled")),
                    true,
                    0},
            {"bpmFixedTempo",
                    ConfigKey(QStringLiteral("[BPM]"),
                            QStringLiteral("BeatDetectionFixedTempoAssumption")),
                    true,
                    0},
            {"bpmFast",
                    ConfigKey(QStringLiteral("[BPM]"), QStringLiteral("FastAnalysisEnabled")),
                    false,
                    0},
            {"bpmReanalyzeOnChange",
                    ConfigKey(QStringLiteral("[BPM]"),
                            QStringLiteral("ReanalyzeWhenSettingsChange")),
                    false,
                    0},
            {"bpmReanalyzeImported",
                    ConfigKey(QStringLiteral("[BPM]"), QStringLiteral("ReanalyzeImported")),
                    false,
                    0},
            {"bpmFromDrums",
                    ConfigKey(QStringLiteral("[BPM]"), QStringLiteral("stem_strategy")),
                    false,
                    2},
            {"keyDetection",
                    ConfigKey(QStringLiteral("[Key]"), QStringLiteral("KeyDetectionEnabled")),
                    true,
                    0},
            {"keyFast",
                    ConfigKey(QStringLiteral("[Key]"), QStringLiteral("FastAnalysisEnabled")),
                    false,
                    0},
            {"keyReanalyzeOnChange",
                    ConfigKey(QStringLiteral("[Key]"),
                            QStringLiteral("ReanalyzeWhenSettingsChange")),
                    false,
                    0},
            {"keyWithoutDrums",
                    ConfigKey(QStringLiteral("[Key]"), QStringLiteral("stem_strategy")),
                    false,
                    2},
            {"devicePreanalysis", mixxx::deviceanalysis::preanalysisConfigKey(), true, 0},
            {"deviceStore", mixxx::deviceanalysis::storeOnDeviceConfigKey(), true, 0},
            {"stemAutoReset",
                    ConfigKey(QStringLiteral("[Mixer Profile]"), QStringLiteral("stem_auto_reset")),
                    true,
                    0},
    };
    return kOptions;
}

bool readOption(const UserSettingsPointer& pConfig, const Option& option) {
    if (option.intValueWhenOn != 0) {
        return pConfig->getValue<int>(option.key, option.defaultValue ? option.intValueWhenOn : 0) ==
                option.intValueWhenOn;
    }
    return pConfig->getValue<bool>(option.key, option.defaultValue);
}

/// Removable volumes (USB drives, SD cards): /storage/<name> except the
/// internal storage
QStringList removableVolumes() {
    // An app may not be allowed to list /storage: also ask for the mounted
    // volumes (same as RekordboxFeature)
    QStringList candidates;
    const QFileInfoList dirs = QDir(QStringLiteral("/storage"))
                                       .entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& dir : dirs) {
        candidates.append(dir.absoluteFilePath());
    }
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& volume : volumes) {
        candidates.append(volume.rootPath());
    }
    QStringList paths;
    for (const QString& path : std::as_const(candidates)) {
        const QStringList parts = path.split(QChar('/'), Qt::SkipEmptyParts);
        if (parts.size() == 2 && parts[0] == QStringLiteral("storage") &&
                parts[1] != QStringLiteral("emulated") && parts[1] != QStringLiteral("self") &&
                !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

QSqlDatabase libraryDatabase() {
    return QmlLibraryProxy::get()->trackCollectionManager()->internalCollection()->database();
}

int countQuery(const QString& sql) {
    QSqlQuery query(libraryDatabase());
    if (!query.exec(sql) || !query.next()) {
        qWarning() << "Analysis dashboard: query failed" << sql;
        return 0;
    }
    return query.value(0).toInt();
}

const QString kNotDeleted = QStringLiteral("library.mixxx_deleted=0");
const QString kHasWaveform = QStringLiteral(
        "library.id IN (SELECT track_id FROM track_analysis WHERE type=1)");
const QString kOnDrive = QStringLiteral(
        "track_locations.location LIKE '/storage/%' AND "
        "track_locations.location NOT LIKE '/storage/emulated/%'");
const QString kIsStem = QStringLiteral(
        "(track_locations.location LIKE '%.stem.mp4' OR "
        "track_locations.location LIKE '%.stem.m4a')");
const QString kFrom = QStringLiteral(
        " FROM library JOIN track_locations ON library.location=track_locations.id WHERE ");

} // namespace

QmlAnalysisProxy::QmlAnalysisProxy(QObject* parent)
        : QObject(parent) {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary && pLibrary->analysisFeature()) {
        return;
    }
    connect(pLibrary->analysisFeature(),
            &AnalysisFeature::analysisActive,
            this,
            [this](bool active) {
                m_active = active;
                if (!active) {
                    m_currentTrack = 0;
                    m_totalTracks = 0;
                    m_trackProgress = 0;
                    emit cacheChanged();
                }
                emit progressChanged();
            });
    connect(pLibrary->analysisFeature(),
            &AnalysisFeature::batchProgress,
            this,
            &QmlAnalysisProxy::onBatchProgress);
    if (RekordboxFeature* pRekordbox = RekordboxFeature::instance()) {
        m_deviceBusy = pRekordbox->busyText();
        connect(pRekordbox,
                &RekordboxFeature::busyTextChanged,
                this,
                [this](const QString& text) {
                    m_deviceBusy = text;
                    emit deviceBusyChanged();
                });
    }
}

void QmlAnalysisProxy::onBatchProgress(
        double currentTrackProgress, int currentTrackNumber, int totalTracksCount) {
    m_currentTrack = currentTrackNumber;
    m_totalTracks = totalTracksCount;
    m_trackProgress = currentTrackProgress > 0 ? currentTrackProgress : 0;
    m_active = totalTracksCount > 0;
    emit progressChanged();
}

QVariantMap QmlAnalysisProxy::options() const {
    QVariantMap result;
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig) {
        return result;
    }
    for (const Option& option : optionTable()) {
        result.insert(QString::fromLatin1(option.name), readOption(pConfig, option));
    }
    return result;
}

void QmlAnalysisProxy::setOption(const QString& name, bool value) {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig) {
        return;
    }
    for (const Option& option : optionTable()) {
        if (name == QLatin1String(option.name)) {
            if (option.intValueWhenOn != 0) {
                pConfig->setValue<int>(option.key, value ? option.intValueWhenOn : 0);
            } else {
                pConfig->setValue<bool>(option.key, value);
            }
            // Android may kill the app without a clean exit: store now
            pConfig->save();
            emit optionsChanged();
            return;
        }
    }
    qWarning() << "Analysis dashboard: unknown option" << name;
}

int QmlAnalysisProxy::analyze(const QString& scope) {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return 0;
    }
    QString where = kNotDeleted;
    if (scope == QStringLiteral("missing")) {
        where += QStringLiteral(" AND (library.bpm IS NULL OR library.bpm<=0 OR NOT ") +
                kHasWaveform + QStringLiteral(")");
    } else if (scope == QStringLiteral("drives")) {
        where += QStringLiteral(" AND ") + kOnDrive;
    } else if (scope != QStringLiteral("all")) {
        qWarning() << "Analysis dashboard: unknown scope" << scope;
        return 0;
    }
    QSqlQuery query(libraryDatabase());
    if (!query.exec(QStringLiteral("SELECT library.id") + kFrom + where)) {
        qWarning() << "Analysis dashboard: query failed" << query.lastQuery();
        return 0;
    }
    QList<AnalyzerScheduledTrack> tracks;
    while (query.next()) {
        tracks.append(AnalyzerScheduledTrack(TrackId(query.value(0))));
    }
    qInfo() << "Analysis dashboard: analyzing" << tracks.size() << "track(s), scope" << scope;
    if (!tracks.isEmpty()) {
        emit pLibrary->analyzeTracks(tracks);
    }
    return static_cast<int>(tracks.size());
}

void QmlAnalysisProxy::stop() {
    Library* pLibrary = QmlLibraryProxy::get();
    if (pLibrary && pLibrary->analysisFeature()) {
        pLibrary->analysisFeature()->stopAnalysis();
    }
}

QVariantMap QmlAnalysisProxy::cacheInfo() const {
    QVariantMap info;
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return info;
    }
    info.insert(QStringLiteral("libraryTracks"),
            countQuery(QStringLiteral("SELECT COUNT(*)") + kFrom + kNotDeleted));
    info.insert(QStringLiteral("tracksWithoutBpm"),
            countQuery(QStringLiteral("SELECT COUNT(*)") + kFrom + kNotDeleted +
                    QStringLiteral(" AND (library.bpm IS NULL OR library.bpm<=0)")));
    info.insert(QStringLiteral("tracksWithoutWaveform"),
            countQuery(QStringLiteral("SELECT COUNT(*)") + kFrom + kNotDeleted +
                    QStringLiteral(" AND NOT ") + kHasWaveform));

    TrackCollection* pCollection = pLibrary->trackCollectionManager()->internalCollection();
    AnalysisDao& analysisDao = pCollection->getAnalysisDAO();
    info.insert(QStringLiteral("waveformBytes"),
            static_cast<qint64>(analysisDao.getDiskUsageInBytes(
                    pCollection->database(), AnalysisDao::TYPE_WAVEFORM)));
    info.insert(QStringLiteral("summaryBytes"),
            static_cast<qint64>(analysisDao.getDiskUsageInBytes(
                    pCollection->database(), AnalysisDao::TYPE_WAVESUMMARY)));

    // Stem files of the library
    qint64 stemBytes = 0;
    int stemTracks = 0;
    QSqlQuery query(pCollection->database());
    if (query.exec(QStringLiteral("SELECT track_locations.location") + kFrom + kNotDeleted +
                QStringLiteral(" AND ") + kIsStem)) {
        while (query.next()) {
            ++stemTracks;
            stemBytes += QFileInfo(query.value(0).toString()).size();
        }
    }
    info.insert(QStringLiteral("stemTracks"), stemTracks);
    info.insert(QStringLiteral("stemBytes"), stemBytes);

    // Analyses stored on the drives
    QVariantList drives;
    for (const QString& path : removableVolumes()) {
        const QDir storeDir(path + QStringLiteral("/MIXXX/ANLZ"));
        const QFileInfoList sidecars = storeDir.entryInfoList(
                {QStringLiteral("*.mxa")}, QDir::Files);
        qint64 bytes = 0;
        for (const QFileInfo& sidecar : sidecars) {
            bytes += sidecar.size();
        }
        const bool rekordbox = QDir(path + QStringLiteral("/PIONEER")).exists();
        if (sidecars.isEmpty() && !rekordbox) {
            continue;
        }
        const QStorageInfo storage(path);
        QVariantMap drive;
        drive.insert(QStringLiteral("label"),
                storage.name().isEmpty() ? QFileInfo(path).fileName() : storage.name());
        drive.insert(QStringLiteral("path"), path);
        drive.insert(QStringLiteral("rekordbox"), rekordbox);
        drive.insert(QStringLiteral("sidecars"), static_cast<int>(sidecars.size()));
        drive.insert(QStringLiteral("bytes"), bytes);
        drive.insert(QStringLiteral("freeBytes"), storage.bytesAvailable());
        drive.insert(QStringLiteral("writable"), !storage.isReadOnly());
        drives.append(drive);
    }
    info.insert(QStringLiteral("drives"), drives);
    return info;
}

bool QmlAnalysisProxy::clearWaveformCache() {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return false;
    }
    TrackCollection* pCollection = pLibrary->trackCollectionManager()->internalCollection();
    AnalysisDao& analysisDao = pCollection->getAnalysisDAO();
    const bool ok = analysisDao.deleteAnalysesByType(
                            pCollection->database(), AnalysisDao::TYPE_WAVEFORM) &&
            analysisDao.deleteAnalysesByType(
                    pCollection->database(), AnalysisDao::TYPE_WAVESUMMARY);
    qInfo() << "Analysis dashboard: waveform cache cleared" << ok;
    emit cacheChanged();
    return ok;
}

int QmlAnalysisProxy::deleteDriveAnalyses(const QString& drivePath) {
    if (!removableVolumes().contains(drivePath)) {
        qWarning() << "Analysis dashboard: not a removable drive" << drivePath;
        return -1;
    }
    // Same rule as the writer: never touch the drive while a deck plays
    if (PlayerInfo::instance().getCurrentPlayingDeck() >= 0) {
        return -1;
    }
    // Only Mixxx's own folder, only its own file types
    QDir storeDir(drivePath + QStringLiteral("/MIXXX/ANLZ"));
    int deleted = 0;
    const QFileInfoList files = storeDir.entryInfoList(
            {QStringLiteral("*.mxa"), QStringLiteral("*.mxa.*")}, QDir::Files);
    for (const QFileInfo& file : files) {
        if (QFile::remove(file.absoluteFilePath())) {
            ++deleted;
        }
    }
    qInfo() << "Analysis dashboard: deleted" << deleted << "analyses from" << drivePath;
    emit cacheChanged();
    return deleted;
}

// static
QmlAnalysisProxy* QmlAnalysisProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    Q_UNUSED(pJsEngine);
    VERIFY_OR_DEBUG_ASSERT(QmlLibraryProxy::get()) {
        qWarning() << "Library hasn't been registered yet";
        return nullptr;
    }
    return new QmlAnalysisProxy(pQmlEngine);
}

} // namespace qml
} // namespace mixxx
