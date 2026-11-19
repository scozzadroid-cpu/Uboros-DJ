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
#include <list>
#include <memory>

#include "library/browse/browsefeature.h"
#include "library/libraryfeature.h"
#include "library/sidebarmodel.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/treeitem.h"
#include "track/trackid.h"
#include "track/track_decl.h"
#include "qmlconfigproxy.h"
#include "qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class LibraryTableModel;
class PlaylistTableModel;
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

/// History: the sessions played (the set logs of Mixxx, hidden playlists of
/// type PLHT_SET_LOG), newest first, each with its tracks in the order they
/// were played. The legacy SetlogFeature of the library does the logging only
/// when it is bound to a library widget, which the QML interface never does:
/// this feature logs the tracks that start playing itself (without touching
/// the play counter or the "played" flag of the track, the list keeps its own
/// session based "played" mark). A session without tracks is not listed, so
/// that the entry stays hidden until something has been played.
class HistoryFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    HistoryFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~HistoryFeature() override = default;

    QVariant title() override {
        return tr("History");
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

  private slots:
    void slotPlayingTrackChanged(TrackPointer pTrack);

  private:
    void reloadSessions();
    /// The session the played tracks are added to: the newest set log (the
    /// one the library created at startup), or a new one
    int currentSessionId();

    QTimer m_reloadTimer;
    PlaylistTableModel* m_pTableModel;
    parented_ptr<TreeItemModel> m_pSidebarModel;
    int m_currentSessionId;
    /// The last tracks played: a track that is played again within this
    /// window is not logged twice (same rule as the legacy history)
    std::list<TrackId> m_recentTracks;
};

/// Matching (Club profile): the library tracks that fit the track of a deck:
/// BPM within 6% (also half and double tempo) and a compatible key. The
/// interface builds the search (Harmony.js) once, when the track of the
/// reference deck changes, and gives it with setQuery().
class MatchingFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    MatchingFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~MatchingFeature() override = default;

    QVariant title() override {
        return tr("Matching");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pSidebarModel;
    }
    bool hasTrackTable() override {
        return true;
    }
    /// Sets the search of the list; the rows are read again only if the list
    /// has been shown
    void setQuery(const QString& query);

  public slots:
    void activate() override;

  private:
    LibraryTableModel* m_pTableModel;
    parented_ptr<TreeItemModel> m_pSidebarModel;
    QString m_query;
    bool m_activated = false;
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
    /// Whether the list can be filtered by the text of the search field (see
    /// QmlLibraryTrackListModel::searchText). False for a source that already
    /// is a search of its own (Matching).
    bool m_searchable{true};
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

/// Sessions played, see HistoryFeature
class QmlLibraryHistorySource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryHistorySource)
  public:
    explicit QmlLibraryHistorySource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<HistoryFeature> m_pLibraryFeature;
};

/// Tracks that fit a deck (BPM and key), see MatchingFeature
class QmlLibraryMatchingSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryMatchingSource)
  public:
    explicit QmlLibraryMatchingSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});
    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }
    /// The search that selects the matching tracks (built in Harmony.js)
    Q_INVOKABLE void setQuery(const QString& query) {
        m_pLibraryFeature->setQuery(query);
    }

  private:
    std::unique_ptr<MatchingFeature> m_pLibraryFeature;
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
