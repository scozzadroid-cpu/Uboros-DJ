#pragma once

#include <qobject.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QTimer>
#include <QVariant>
#include <memory>

#include "library/browse/browsefeature.h"
#include "library/libraryfeature.h"
#include "library/sidebarmodel.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/treeitem.h"
#include "qmlconfigproxy.h"
#include "qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class LibraryTableModel;
class TreeItemModel;
class TreeItem;

/// Library tracks grouped by artist or by folder: the sidebar lists the
/// groups, selecting one shows its tracks (a library search on that group)
class GroupedTrackFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    enum class Mode { Artist, Folder };
    GroupedTrackFeature(Library* pLibrary, UserSettingsPointer pConfig, Mode mode);
    ~GroupedTrackFeature() override = default;

    QVariant title() override {
        return m_mode == Mode::Artist ? tr("Artists") : tr("Folders");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pSidebarModel;
    }
    bool hasTrackTable() override {
        return true;
    }

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;

  private:
    void reloadGroups();
    /// Reads the groups a moment after the library changed, so that the
    /// sidebar knows whether the entry has anything in it without it being
    /// opened first (an empty entry is hidden)
    void scheduleReload();

    QTimer m_reloadTimer;
    const Mode m_mode;
    LibraryTableModel* m_pTableModel;
    parented_ptr<TreeItemModel> m_pSidebarModel;
    bool m_loaded = false;
    /// What the sidebar shows now (labels, data, nesting)
    QString m_treeSignature;
};
class AllTrackLibraryFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    AllTrackLibraryFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    ~AllTrackLibraryFeature() override = default;

    QVariant title() override {
        return tr("All...");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pSidebarModel;
    }

    bool hasTrackTable() override {
        return true;
    }

    LibraryTableModel* trackTableModel() const {
        return m_pLibraryTableModel;
    }

    void searchAndActivate(const QString& query);

  public slots:
    void activate() override;

  private:
    LibraryTableModel* m_pLibraryTableModel;

    parented_ptr<TreeItemModel> m_pSidebarModel;
};

namespace mixxx {
namespace qml {

class QmlLibraryTrackListColumn;

class QmlLibrarySource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label MEMBER m_label)
    Q_PROPERTY(QString icon MEMBER m_icon)
    Q_PROPERTY(QQmlListProperty<QmlLibraryTrackListColumn> columns READ columnsQml)
    Q_CLASSINFO("DefaultProperty", "columns")
    QML_NAMED_ELEMENT(LibrarySource)
    QML_UNCREATABLE("Only accessible via its specialization")
  public:
    explicit QmlLibrarySource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    QQmlListProperty<QmlLibraryTrackListColumn> columnsQml() {
        return {this, &m_columns};
    }

    const QList<QmlLibraryTrackListColumn*>& columns() const {
        return m_columns;
    }
    virtual LibraryFeature* internal() = 0;
  public slots:
    void slotShowTrackModel(QAbstractItemModel* pModel);

  signals:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    void requestTrackModel(std::shared_ptr<QmlLibraryTrackListModel> pModel);
#else
    void requestTrackModel(std::shared_ptr<mixxx::qml::QmlLibraryTrackListModel> pModel);
#endif

  protected:
    QString m_label;
    QString m_icon;
    QList<QmlLibraryTrackListColumn*> m_columns;
    /// Whether the track objects of the rows may be created while the list is
    /// displayed. Must be false for sources outside of the Mixxx library
    /// (files, external libraries), where creating a track object imports the
    /// file into the library.
    bool m_tracksOnDisplay{true};
};

class QmlLibraryAllTrackSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryAllTrackSource)
  public:
    explicit QmlLibraryAllTrackSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<AllTrackLibraryFeature> m_pLibraryFeature;
};

/// Exposes the Rekordbox feature owned by the library (prepared USB drives)
class QmlLibraryRekordboxSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryRekordboxSource)
  public:
    explicit QmlLibraryRekordboxSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pLibraryFeature;
    }

  private:
    // Owned by the library, may be null if the feature is disabled
    LibraryFeature* m_pLibraryFeature;
};

/// Exposes the Browse ("Computer") feature owned by the library
class QmlLibraryBrowseSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryBrowseSource)
  public:
    explicit QmlLibraryBrowseSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pLibraryFeature;
    }

  private:
    // Owned by the library
    LibraryFeature* m_pLibraryFeature;
};

/// Playlists and crates of the library (owned by the library)
class QmlLibraryPlaylistSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryPlaylistSource)
  public:
    explicit QmlLibraryPlaylistSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature;
    }

  private:
    LibraryFeature* m_pLibraryFeature;
};

class QmlLibraryCrateSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryCrateSource)
  public:
    explicit QmlLibraryCrateSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature;
    }

  private:
    LibraryFeature* m_pLibraryFeature;
};

class QmlLibraryArtistSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryArtistSource)
  public:
    explicit QmlLibraryArtistSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<GroupedTrackFeature> m_pLibraryFeature;
};

class QmlLibraryFolderSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryFolderSource)
  public:
    explicit QmlLibraryFolderSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<GroupedTrackFeature> m_pLibraryFeature;
};

} // namespace qml
} // namespace mixxx
