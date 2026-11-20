#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QSqlQuery>
#include <QUrl>

#include "analyzer/analyzerscheduledtrack.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#ifdef __ANDROID__
#include <QCoreApplication>
#include <QJniObject>
#endif

#include "library/library.h"
#include "library/librarytablemodel.h"
#include "moc_qmllibraryproxy.cpp"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "qmltrackproxy.h"
#include "track/track.h"
#include "util/storagevolume.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlLibraryProxy::QmlLibraryProxy(QObject* parent)
        : QObject(parent) {
    m_driveClock.start();
}

QmlLibraryTrackListModel* QmlLibraryProxy::model() const {
    return make_qml_owned<QmlLibraryTrackListModel>(
            QList<QmlLibraryTrackListColumn*>{}, s_pLibrary->trackTableModel())
            .get();
}

void QmlLibraryProxy::analyze(const QmlTrackProxy* track) const {
    VERIFY_OR_DEBUG_ASSERT(track && track->internal()) {
        return;
    }
    emit s_pLibrary->analyzeTracks({track->internal()->getId()});
}

bool QmlLibraryProxy::isDriveMissing(const QString& fileUrl, int epoch) const {
    Q_UNUSED(epoch);
    const QString path = fileUrl.startsWith(QLatin1String("file:"))
            ? QUrl(fileUrl).toLocalFile()
            : fileUrl;
    const QLatin1String prefix("/storage/");
    if (!path.startsWith(prefix)) {
        return false;
    }
    const QString volume = path.mid(prefix.size()).section(QLatin1Char('/'), 0, 0);
    if (volume.isEmpty()) {
        return false;
    }
    constexpr qint64 kValidMs = 3000;
    const qint64 now = m_driveClock.isValid() ? m_driveClock.elapsed() : 0;
    const auto it = m_driveStates.constFind(volume);
    if (it != m_driveStates.constEnd() && m_driveClock.isValid() &&
            now - it->checkedAtMs < kValidMs) {
        return it->missing;
    }
    DriveState state;
    state.missing = !mixxx::missingStorageVolume(prefix + volume + QLatin1Char('/')).isEmpty();
    state.checkedAtMs = now;
    m_driveStates.insert(volume, state);
    return state.missing;
}

int QmlLibraryProxy::analyzeTracksWithoutBpm() const {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return 0;
    }
    QSqlQuery query(s_pLibrary->trackCollectionManager()->internalCollection()->database());
    query.prepare(QStringLiteral(
            "SELECT id FROM library WHERE mixxx_deleted=0 AND (bpm IS NULL OR bpm<=0)"));
    if (!query.exec()) {
        qWarning() << "analyzeTracksWithoutBpm: query failed" << query.lastQuery();
        return 0;
    }
    QList<AnalyzerScheduledTrack> tracks;
    while (query.next()) {
        tracks.append(AnalyzerScheduledTrack(TrackId(query.value(0))));
    }
    qInfo() << "Analyzing" << tracks.size() << "track(s) without BPM";
    if (!tracks.isEmpty()) {
        emit s_pLibrary->analyzeTracks(tracks);
    }
    return static_cast<int>(tracks.size());
}

bool QmlLibraryProxy::importUsbDrives() const {
#ifdef __ANDROID__
    // Same folder as the one searched by RekordboxFeature
    const QString destRoot = QStringLiteral("/storage/emulated/0/RekordboxUSB");
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("org/mixxx/UsbImporter",
            "start",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            context.object<jobject>(),
            QJniObject::fromString(destRoot).object<jstring>());
    return true;
#else
    return false;
#endif
}

// static
QmlLibraryProxy* QmlLibraryProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet";
        return nullptr;
    }
    return new QmlLibraryProxy(pQmlEngine);
}

} // namespace qml
} // namespace mixxx
