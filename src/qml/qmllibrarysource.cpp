#include "qml/qmllibrarysource.h"

#include <qalgorithms.h>
#include <qlist.h>
#include <qqmlengine.h>

#include <QAbstractListModel>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>
#include <memory>

#include "library/browse/browsefeature.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/rekordbox/rekordboxfeature.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/crate/cratesummary.h"
#include "library/trackset/playlistfeature.h"
#include "library/treeitemmodel.h"
#include "moc_qmllibrarysource.cpp"
#include "qmllibraryproxy.h"
#include "track/track.h"

AllTrackLibraryFeature::AllTrackLibraryFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QString()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pLibraryTableModel(pLibrary->trackTableModel()) {
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
}

void AllTrackLibraryFeature::activate() {
    emit showTrackModel(m_pLibraryTableModel);
}

GroupedTrackFeature::GroupedTrackFeature(
        Library* pLibrary, UserSettingsPointer pConfig, Mode mode)
        : LibraryFeature(pLibrary, pConfig, QString()),
          m_mode(mode),
          m_pTableModel(new LibraryTableModel(this,
                  pLibrary->trackCollectionManager(),
                  mode == Mode::Artist ? "mixxx.db.model.group_artist"
                                       : "mixxx.db.model.group_folder")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(1500);
    connect(&m_reloadTimer, &QTimer::timeout, this, &GroupedTrackFeature::reloadGroups);
    // Tracks added or removed (library scan, USB drive imported)
    TrackCollectionManager* pManager = pLibrary->trackCollectionManager();
    connect(pManager->internalCollection(),
            &TrackCollection::tracksAdded,
            this,
            &GroupedTrackFeature::scheduleReload);
    connect(pManager->internalCollection(),
            &TrackCollection::tracksRemoved,
            this,
            &GroupedTrackFeature::scheduleReload);
    connect(pManager,
            &TrackCollectionManager::libraryScanFinished,
            this,
            &GroupedTrackFeature::scheduleReload);
    // First reading once the library is up
    scheduleReload();
}

void GroupedTrackFeature::scheduleReload() {
    m_reloadTimer.start();
}

void GroupedTrackFeature::activate() {
    // Groups are read each time the entry is opened (new tracks, new drives)
    reloadGroups();
    m_pTableModel->search(QString());
    m_pTableModel->select();
    emit showTrackModel(m_pTableModel);
}

void GroupedTrackFeature::reloadGroups() {
    TreeItem* root = m_pSidebarModel->getRootItem();
    if (root->childRows() > 0) {
        m_pSidebarModel->removeRows(0, root->childRows());
    }
    QSqlDatabase database = m_pLibrary->trackCollectionManager()
                                    ->internalCollection()
                                    ->database();
    QSqlQuery query(database);
    if (m_mode == Mode::Artist) {
        query.prepare(QStringLiteral(
                "SELECT artist, COUNT(*) FROM library "
                "WHERE mixxx_deleted=0 AND artist<>'' "
                "GROUP BY artist COLLATE NOCASE ORDER BY artist COLLATE NOCASE"));
    } else {
        query.prepare(QStringLiteral(
                "SELECT tl.directory, COUNT(*) FROM library l "
                "JOIN track_locations tl ON l.location=tl.id "
                "WHERE l.mixxx_deleted=0 AND tl.directory<>'' "
                "GROUP BY tl.directory ORDER BY tl.directory COLLATE NOCASE"));
    }
    if (!query.exec()) {
        qWarning() << "GroupedTrackFeature: query failed" << query.lastError();
        return;
    }
    std::vector<std::unique_ptr<TreeItem>> children;
    while (query.next()) {
        const QString value = query.value(0).toString();
        QString label = value;
        if (m_mode == Mode::Folder) {
            // Parent and name: "Music/House" is clearer than "House" alone
            const QStringList parts = value.split('/', Qt::SkipEmptyParts);
            label = parts.size() > 1 ? parts.mid(parts.size() - 2).join('/')
                                     : value;
        }
        children.push_back(std::make_unique<TreeItem>(
                QStringLiteral("%1 (%2)").arg(label).arg(query.value(1).toInt()),
                value));
    }
    if (!children.empty()) {
        m_pSidebarModel->insertTreeItemRows(std::move(children), 0);
    }
}

void GroupedTrackFeature::activateChild(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    if (!item || !item->getData().isValid()) {
        return;
    }
    QString value = item->getData().toString();
    value.replace('"', ' ');
    const QString field = m_mode == Mode::Artist ? QStringLiteral("artist")
                                                 : QStringLiteral("location");
    m_pTableModel->search(QStringLiteral("%1:\"%2\"").arg(
            field, m_mode == Mode::Folder ? value + '/' : value));
    m_pTableModel->select();
    emit showTrackModel(m_pTableModel);
}

namespace mixxx {
namespace qml {

QmlLibrarySource::QmlLibrarySource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QObject(parent),
          m_columns(columns) {
}

void QmlLibrarySource::slotShowTrackModel(QAbstractItemModel* pModel) {
    emit requestTrackModel(std::make_shared<QmlLibraryTrackListModel>(
            columns(), pModel, nullptr, m_tracksOnDisplay));
}

QmlLibraryAllTrackSource::QmlLibraryAllTrackSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(std::make_unique<AllTrackLibraryFeature>(
                  QmlLibraryProxy::get(), QmlConfigProxy::get())) {
    connect(m_pLibraryFeature.get(),
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryRekordboxSource::QmlLibraryRekordboxSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(QmlLibraryProxy::get()->findFeature<RekordboxFeature>()) {
    m_tracksOnDisplay = false;
    if (!m_pLibraryFeature) {
        qWarning() << "QmlLibraryRekordboxSource: the Rekordbox feature is disabled";
        return;
    }
    connect(m_pLibraryFeature,
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryBrowseSource::QmlLibraryBrowseSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(QmlLibraryProxy::get()->findFeature<BrowseFeature>()) {
    m_tracksOnDisplay = false;
    if (!m_pLibraryFeature) {
        qWarning() << "QmlLibraryBrowseSource: the Browse feature is not available";
        return;
    }
    connect(m_pLibraryFeature,
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryPlaylistSource::QmlLibraryPlaylistSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(QmlLibraryProxy::get()->findFeature<PlaylistFeature>()) {
    if (!m_pLibraryFeature) {
        return;
    }
    connect(m_pLibraryFeature,
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryCrateSource::QmlLibraryCrateSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(QmlLibraryProxy::get()->findFeature<CrateFeature>()) {
    if (!m_pLibraryFeature) {
        return;
    }
    connect(m_pLibraryFeature,
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryArtistSource::QmlLibraryArtistSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(std::make_unique<GroupedTrackFeature>(
                  QmlLibraryProxy::get(),
                  QmlConfigProxy::get(),
                  GroupedTrackFeature::Mode::Artist)) {
    connect(m_pLibraryFeature.get(),
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

QmlLibraryFolderSource::QmlLibraryFolderSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(std::make_unique<GroupedTrackFeature>(
                  QmlLibraryProxy::get(),
                  QmlConfigProxy::get(),
                  GroupedTrackFeature::Mode::Folder)) {
    connect(m_pLibraryFeature.get(),
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

} // namespace qml
} // namespace mixxx
