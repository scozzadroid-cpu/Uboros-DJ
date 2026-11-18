// This feature reads tracks, playlists and folders from removable Recordbox
// prepared devices (USB drives, etc), by parsing the binary *.PDB files
// stored on each removable device. It does not read the locally stored
// Rekordbox database (Collection).

// It draws heavily from the hard work completed here:

//      https://github.com/Deep-Symmetry/crate-digger

// And uses the C++ Kaitai Struct binary parsing libraries:

//      http://kaitai.io
//      https://github.com/kaitai-io/kaitai_struct
//      https://github.com/kaitai-io/kaitai_struct_cpp_stl_runtime

// The *.PDB C++ files:

//      rekordbox_pdb.h
//      rekordbox_pdb.cpp

// Were generated from the following structure definition file:

//      https://github.com/Deep-Symmetry/crate-digger/blob/master/src/main/kaitai/rekordbox_pdb.ksy

#pragma once

#include "analyzer/analyzerprogress.h"
#include <QTimer>

#include "analyzer/analyzerscheduledtrack.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QStringListModel>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <fstream>

#include "library/baseexternallibraryfeature.h"
#include "library/baseexternalplaylistmodel.h"
#include "library/baseexternaltrackmodel.h"
#include "library/rekordbox/deviceanalysisstore.h"
#include "library/treeitemmodel.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class BaseExternalPlaylistModel;

class RekordboxPlaylistModel : public BaseExternalPlaylistModel {
    Q_OBJECT
  public:
    RekordboxPlaylistModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            QSharedPointer<BaseTrackCache> trackSource);
    TrackPointer getTrack(const QModelIndex& index) const override;
    bool isColumnHiddenByDefault(int column) override;
    bool isColumnInternal(int column) override;

  protected:
    void initSortColumnMapping() override;
};

class RekordboxFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    RekordboxFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~RekordboxFeature() override;

    QVariant title() override;
    static bool isSupported();
    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;

    TreeItemModel* sidebarModel() const override;

    /// The feature of the running library (one per application), for the
    /// QML interface's "Loading USB drive" notice
    static RekordboxFeature* instance() {
        return s_pInstance;
    }
    /// What the feature is doing with a USB drive ("" when idle)
    QString busyText() const {
        return m_busyText;
    }

  signals:
    void busyTextChanged(const QString& text);

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void refreshLibraryModels();
    void onRekordboxDevicesFound();
    void onTracksFound();

  private slots:
    void htmlLinkClicked(const QUrl& link);
    // Pre-analysis: imports the device's tracks into the library (beatgrid,
    // cues, key, color from rekordbox, as when loading one) a few at a time,
    // then queues their analysis (waveforms), so that loading a track later
    // does not wait for "Analyzing". Writes only to Mixxx's own database.
    void preanalyzeNextBatch();
    // Analysis stored ON the device (MIXXX/ANLZ, see deviceanalysisstore.h):
    // queued after an analysis or a change, written while no deck plays
    void onTrackAnalyzerProgress(TrackId trackId, AnalyzerProgress progress);
    void onTracksChanged(const QSet<TrackId>& trackIds);
    void exportNextAnalyses();
    void autoScanDevices();

  private:
    QString formatRootViewHtml() const;
    std::unique_ptr<BaseSqlTableModel> createPlaylistModelForPlaylist(
            const QVariant& data) override;

    parented_ptr<TreeItemModel> m_pSidebarModel;
    parented_ptr<RekordboxPlaylistModel> m_pRekordboxPlaylistModel;
    parented_ptr<RekordboxPlaylistModel> m_pPreanalysisModel;
    QTimer m_preanalysisTimer;
    QString m_preanalysisPlaylist;
    int m_preanalysisRow = 0;
    QString m_busyText;
    void setBusyText(const QString& text);
    static inline RekordboxFeature* s_pInstance = nullptr;
    QList<AnalyzerScheduledTrack> m_preanalysisTracks;
    QTimer m_exportTimer;
    QList<TrackId> m_exportQueue;
    bool m_exportReadOnlyLogged = false;
    // Comparing with the drive and writing run in a worker thread, one at a time
    QFuture<mixxx::deviceanalysis::ExportResult> m_exportWrite;
    TrackPointer m_exportWriteTrack;
    // Own pool: its only thread is lowered to background priority (the global
    // pool is shared and a thread cannot get its priority back)
    QThreadPool m_exportPool;

    QFutureWatcher<QList<TreeItem*>> m_devicesFutureWatcher;
    QFuture<QList<TreeItem*>> m_devicesFuture;
    QFutureWatcher<QString> m_tracksFutureWatcher;
    QFuture<QString> m_tracksFuture;
    QString m_title;
    // Android: look for drives by itself and analyze them in the background
    QTimer m_autoScanTimer;
    bool m_autoScan = false;
    bool m_backgroundLoad = false;
    void loadDevice(TreeItem* item, const QString& label, const QString& path);

    QSharedPointer<BaseTrackCache> m_trackSource;
};
