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

/// One phrase of the song structure of a track analyzed by rekordbox (PSSI
/// tag of the .EXT analysis file): Intro, Verse, Bridge, Chorus, Outro and,
/// in "high mood" tracks, Up and Down.
struct RekordboxPhrase {
    enum class Kind {
        Intro,
        Verse,
        Bridge,
        Chorus,
        Outro,
        Up,
        Down,
    };
    Kind kind = Kind::Intro;
    /// Verse number 1..6 (0 for the other kinds; verse "1b"/"1c" are Verse 1)
    int number = 0;
    /// Mood of the whole track: 1 high, 2 mid, 3 low
    int mood = 0;
    /// Position in seconds (rekordbox time, at 100% pitch)
    double startSec = 0.0;
    /// 0 when unknown (the phrase then runs to the end of the track)
    double endSec = 0.0;
};

/// Reads the phrases of a track from the analysis files of the drive:
/// `anlzPath` is the ANLZ*.DAT of the track (the beat grid, to turn beat
/// numbers into time) and the PSSI tag is in the .EXT next to it. Reads two
/// small files and parses them: call it from a worker thread. Returns an
/// empty list when the track has no phrases or the files cannot be read (a
/// drive that went away, a damaged file): it never throws.
QList<RekordboxPhrase> readRekordboxPhrases(const QString& anlzPath);

class RekordboxPlaylistModel : public BaseExternalPlaylistModel {
    Q_OBJECT
  public:
    RekordboxPlaylistModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            QSharedPointer<BaseTrackCache> trackSource);
    TrackPointer getTrack(const QModelIndex& index) const override;
    bool isColumnHiddenByDefault(int column) override;
    bool isColumnInternal(int column) override;
    /// What the import of the track at `index` read from the drive, as a short
    /// text: the audio file, the two analysis files and the key and color of the
    /// export, with their sizes and times. The same text = nothing changed since
    /// the track was prepared, no need to read the drive again. Empty when the
    /// audio file is not there.
    QString preparedStamp(const QModelIndex& index) const;

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
    /// True from the start until the first look for USB drives is over and every
    /// drive found has been read and prepared (the start screen waits for it:
    /// reading a drive makes the interface stutter). Never true again after that.
    bool startupBusy() const {
        return m_startupBusy;
    }
    QString busyText() const {
        return m_busyText;
    }
    /// The ANLZ*.DAT path of the rekordbox track with this file location, or
    /// "" when it is not on a drive read by this feature (yet). GUI thread.
    QString analyzePathForLocation(const QString& location) const;

  signals:
    void busyTextChanged(const QString& text);
    void startupBusyChanged(bool busy);

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
    void preanalyzeNextBatchImpl();
    // The device vanished or cannot be written: stop the background work
    void abortDeviceWork(const QString& why);
    bool deviceStillThere() const;
    // Analysis stored ON the device (MIXXX/ANLZ, see deviceanalysisstore.h):
    // queued after an analysis or a change, written while no deck plays
    void onTrackAnalyzerProgress(TrackId trackId, AnalyzerProgress progress);
    void onTracksChanged(const QSet<TrackId>& trackIds);
    void exportNextAnalyses();
    void exportNextAnalysesImpl();
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
    bool m_startupBusy = true;
    QStringList m_volumes; // the mounted volumes at the last look (autoScanDevices)
    int m_scansDone = 0;
    // A slot that changes several things one after the other: the start is not
    // over in between
    bool m_startupHold = false;
    void updateStartupBusy();
    bool hasUnloadedDevice() const;
    static inline RekordboxFeature* s_pInstance = nullptr;
    QList<AnalyzerScheduledTrack> m_preanalysisTracks;
    QTimer m_exportTimer;
    QList<TrackId> m_exportQueue;
    bool m_exportReadOnlyLogged = false;
    // An I/O error while writing: no more writes to the devices this session
    bool m_exportDisabled = false;
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
