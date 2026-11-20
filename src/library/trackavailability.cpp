#include "library/trackavailability.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QVector>
#include <QtConcurrentRun>
#include <QtDebug>

#include "library/queryutil.h"
#include "moc_trackavailability.cpp"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("TrackAvailability");

/// Returns the number of flags changed, -1 when the database could not be used
int verifyFiles(mixxx::DbConnectionPoolPtr pDbConnectionPool) {
    const mixxx::DbConnectionPooler dbConnectionPooler(pDbConnectionPool);
    QSqlDatabase database = mixxx::DbConnectionPooled(pDbConnectionPool);
    if (!database.isOpen()) {
        return -1;
    }
    QThread::currentThread()->setPriority(QThread::LowPriority);

    struct Change {
        int id;
        int fsDeleted;
    };
    QVector<Change> changes;
    {
        QSqlQuery query(database);
        query.setForwardOnly(true);
        if (!query.exec(QStringLiteral("SELECT id, location, fs_deleted FROM track_locations"))) {
            LOG_FAILED_QUERY(query);
            return -1;
        }
        while (query.next()) {
            const QString location = query.value(1).toString();
            if (location.isEmpty()) {
                continue;
            }
            const int want = QFileInfo::exists(location) ? 0 : 1;
            if (want != query.value(2).toInt()) {
                changes.append({query.value(0).toInt(), want});
            }
        }
    }
    if (changes.isEmpty()) {
        return 0;
    }
    ScopedTransaction transaction(database);
    QSqlQuery update(database);
    update.prepare(QStringLiteral("UPDATE track_locations SET fs_deleted=:v WHERE id=:id"));
    for (const Change& change : std::as_const(changes)) {
        update.bindValue(QStringLiteral(":v"), change.fsDeleted);
        update.bindValue(QStringLiteral(":id"), change.id);
        if (!update.exec()) {
            LOG_FAILED_QUERY(update);
        }
    }
    transaction.commit();
    return static_cast<int>(changes.size());
}

} // namespace

TrackAvailability::TrackAvailability(
        mixxx::DbConnectionPoolPtr pDbConnectionPool, QObject* pParent)
        : QObject(pParent),
          m_pDbConnectionPool(std::move(pDbConnectionPool)) {
    s_pInstance = this;
    connect(&m_watcher, &QFutureWatcher<int>::finished, this, &TrackAvailability::onFinished);
}

TrackAvailability::~TrackAvailability() {
    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }
    m_future.waitForFinished();
}

void TrackAvailability::verify() {
    if (m_future.isRunning()) {
        m_again = true;
        return;
    }
    m_again = false;
    m_future = QtConcurrent::run(verifyFiles, m_pDbConnectionPool);
    m_watcher.setFuture(m_future);
}

void TrackAvailability::onFinished() {
    const int changed = m_future.result();
    m_hasRun = true;
    if (changed > 0) {
        kLogger.info() << changed << "track(s) changed between present and missing";
    }
    emit finished(changed > 0);
    if (m_again) {
        verify();
    }
}
