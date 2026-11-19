#include "qml/qmllibrarysource.h"

#include <qalgorithms.h>
#include <qlist.h>
#include <qqmlengine.h>

#include <QAbstractListModel>
#include <QMap>
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

namespace {

/// One folder of the tree shown by "Folders": like the folder view of
/// rekordbox, the real hierarchy of the disk with a count of the tracks
/// inside (subfolders included)
struct DirNode {
    QString name;
    QString path;
    int direct = 0; // tracks directly in this folder
    int total = 0;  // tracks in this folder and below
    QMap<QString, std::shared_ptr<DirNode>> kids;
};

int computeTotals(DirNode& node) {
    node.total = node.direct;
    for (const auto& kid : std::as_const(node.kids)) {
        node.total += computeTotals(*kid);
    }
    return node.total;
}

/// Entry for a folder. A folder with no tracks of its own and a single
/// subfolder (that has more inside) is merged with it ("emulated/0/Music"),
/// a folder whose only child is a folder with just tracks is kept as is
/// (Artist > Title, like on the USB drive)
std::unique_ptr<TreeItem> makeFolderItem(const DirNode& start) {
    QStringList names{start.name};
    const DirNode* pNode = &start;
    while (pNode->direct == 0 && pNode->kids.size() == 1) {
        const DirNode& next = *pNode->kids.first();
        if (next.kids.isEmpty()) {
            break;
        }
        names << next.name;
        pNode = &next;
    }
    QString label = names.size() > 3
            ? QStringLiteral("\u2026/") + names.mid(names.size() - 3).join('/')
            : names.join('/');
    auto pItem = std::make_unique<TreeItem>(
            QStringLiteral("%1 (%2)").arg(label).arg(pNode->total),
            pNode->path);
    for (const auto& kid : std::as_const(pNode->kids)) {
        pItem->insertChild(pItem->childRows(), makeFolderItem(*kid));
    }
    return pItem;
}

/// Everything the entries say (labels, data, nesting): to know whether the
/// sidebar has to be rebuilt
void appendSignature(const TreeItem* pItem, QString* pOut) {
    *pOut += pItem->getLabel();
    *pOut += QChar(0x1f);
    *pOut += pItem->getData().toString();
    *pOut += QChar('(');
    for (const TreeItem* pChild : pItem->children()) {
        appendSignature(pChild, pOut);
    }
    *pOut += QChar(')');
}

} // namespace

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
    if (m_mode == Mode::Folder) {
        DirNode top;
        while (query.next()) {
            const QString value = query.value(0).toString();
            const int count = query.value(1).toInt();
            const QStringList parts = value.split('/', Qt::SkipEmptyParts);
            if (parts.isEmpty()) {
                continue;
            }
            const QString prefix = value.startsWith('/') ? QStringLiteral("/") : QString();
            DirNode* pNode = &top;
            for (int i = 0; i < parts.size(); ++i) {
                std::shared_ptr<DirNode>& pKid =
                        pNode->kids[parts.at(i).toLower() + QChar(0) + parts.at(i)];
                if (!pKid) {
                    pKid = std::make_shared<DirNode>();
                    pKid->name = parts.at(i);
                    pKid->path = prefix + parts.mid(0, i + 1).join('/');
                }
                pNode = pKid.get();
            }
            pNode->direct += count;
        }
        computeTotals(top);
        // The part of the path that all the tracks share (/storage/emulated/0...)
        // says nothing: the entries start where the folders split
        std::vector<const DirNode*> tops;
        for (const auto& kid : std::as_const(top.kids)) {
            tops.push_back(kid.get());
        }
        while (tops.size() == 1 && tops[0]->direct == 0 && !tops[0]->kids.isEmpty()) {
            const DirNode* pOnly = tops[0];
            tops.clear();
            for (const auto& kid : std::as_const(pOnly->kids)) {
                tops.push_back(kid.get());
            }
        }
        for (const DirNode* pNode : tops) {
            children.push_back(makeFolderItem(*pNode));
        }
    } else {
        while (query.next()) {
            children.push_back(std::make_unique<TreeItem>(
                    QStringLiteral("%1 (%2)")
                            .arg(query.value(0).toString())
                            .arg(query.value(1).toInt()),
                    query.value(0).toString()));
        }
    }
    // Same groups and counts as now: leave the model alone, so that the
    // selection of the user in the sidebar is not lost
    QString signature;
    for (const auto& pChild : children) {
        appendSignature(pChild.get(), &signature);
    }
    if (signature == m_treeSignature) {
        return;
    }
    m_treeSignature = signature;
    if (root->childRows() > 0) {
        m_pSidebarModel->removeRows(0, root->childRows());
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
