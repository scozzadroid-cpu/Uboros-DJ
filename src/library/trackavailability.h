#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>

#include "util/db/dbconnectionpool.h"

/// Keeps `track_locations.fs_deleted` right for every track of the library:
/// 1 when the file is not there now (deleted, a drive that is not connected, a
/// folder that was removed), 0 when it is. The track lists leave out the tracks
/// with fs_deleted = 1, so a track that cannot be loaded is not shown (it used
/// to be listed and then refused with "file not found" when tapped).
/// The library scanner does the same for the folders it scans; this covers the
/// rest (tracks of USB drives and of copies that no longer exist) and comes
/// back to 0 when a drive is plugged again. The files are checked in a worker
/// thread, with the database of its own: `verify()` returns at once, calls made
/// while it runs are merged into one more pass, `finished()` tells whether any
/// flag changed (the lists then read their rows again).
class TrackAvailability : public QObject {
    Q_OBJECT
  public:
    TrackAvailability(mixxx::DbConnectionPoolPtr pDbConnectionPool, QObject* pParent);
    ~TrackAvailability() override;

    /// The one of the running library (null before it exists)
    static TrackAvailability* instance() {
        return s_pInstance;
    }

    void verify();
    /// True once the first pass is over (the lists are right from then on)
    bool hasRun() const {
        return m_hasRun;
    }

  signals:
    void finished(bool changed);

  private:
    void onFinished();

    const mixxx::DbConnectionPoolPtr m_pDbConnectionPool;
    QFuture<int> m_future;
    QFutureWatcher<int> m_watcher;
    bool m_again = false;
    bool m_hasRun = false;

    static inline TrackAvailability* s_pInstance = nullptr;
};
