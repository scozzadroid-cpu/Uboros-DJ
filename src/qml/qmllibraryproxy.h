#pragma once
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <Qt>
#include <memory>

#include "qml_owned_ptr.h"
#include "qmllibrarytracklistmodel.h"

class Library;

namespace mixxx {
namespace qml {

class QmlTrackProxy;

class QmlLibraryProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlLibraryTrackListModel* model READ model CONSTANT)
    QML_NAMED_ELEMENT(Library)
    QML_SINGLETON

  public:
    explicit QmlLibraryProxy(QObject* parent = nullptr);

    static QmlLibraryProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerLibrary(std::shared_ptr<Library> pLibrary) {
        s_pLibrary = std::move(pLibrary);
    }

    static Library* get() {
        return s_pLibrary.get();
    }

    QmlLibraryTrackListModel* model() const;
    Q_INVOKABLE void analyze(const mixxx::qml::QmlTrackProxy* track) const;
    /// Android: copy the Rekordbox export of the connected USB drives to the
    /// internal storage (no root needed). Returns false where unsupported.
    Q_INVOKABLE bool importUsbDrives() const;
    /// Queues the analysis (BPM, key, beat grid...) of the library tracks
    /// that have no BPM yet. Returns the number of queued tracks.
    Q_INVOKABLE int analyzeTracksWithoutBpm() const;

    /// True when the file (a file:// URL or a path) is on a removable drive
    /// ("/storage/<name>/...") that is not mounted now: the library shows such
    /// tracks dimmed. Cached per drive for a few seconds (the rows ask often).
    /// `epoch` is not used: a QML binding passes a counter to ask again.
    Q_INVOKABLE bool isDriveMissing(const QString& fileUrl, int epoch = 0) const;

  private:
    struct DriveState {
        qint64 checkedAtMs = 0;
        bool missing = false;
    };
    mutable QHash<QString, DriveState> m_driveStates;
    QElapsedTimer m_driveClock;

    static inline std::shared_ptr<Library> s_pLibrary;
};

} // namespace qml
} // namespace mixxx
