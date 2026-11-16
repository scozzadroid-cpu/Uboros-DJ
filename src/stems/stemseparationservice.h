#pragma once

// Instant stems: separation on the device of the tracks loaded in the decks,
// started by the user (SMART CFX of the DDJ-FLX4, or the "stem_separate"
// control) and done from the play position onwards, so the stems near the
// play head are ready in a few seconds. Design: ~/mixxx-android/stems/ISTANTANEI.md
//
// Decks play such tracks through AudioSourceProgressiveStemProxy (opened by
// CachingReaderWorker) which reads the stems from a StemCache as soon as its
// chunks are ready; this service fills the caches with htdemucs.
//
// Controls per deck ([ChannelN]):
//   stem_separate   button: start separating the track of the deck
//   stem_progress   -1 no instant stems, 0..1 share of the track separated
//   stems_ready     1 if the stems at the play position are usable (real
//                   stem files and sidecars: always 1)

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "preferences/usersettings.h"
#include "track/steminfo.h"
#include "track/track_decl.h"

class ControlObject;
class ControlPushButton;

namespace mixxx {
namespace stems {

class StemCache;
class HtdemucsNet;
class TrackSeparator;

/// Whether decks open tracks without stems as progressive stem tracks
/// ([Stems],InstantStems, default on). Any thread.
bool instantStemsEnabled();

/// Stem infos of a progressive track (4 slots, the third one silent)
QList<StemInfo> instantStemInfos();

class StemSeparationService : public QObject {
    Q_OBJECT
  public:
    StemSeparationService(UserSettingsPointer pConfig, int deckCount, QObject* pParent = nullptr);
    ~StemSeparationService() override;

    /// Any thread (CachingReaderWorker): the deck loaded a track; pCache is
    /// null if the track is not played as a progressive stem track, realStems
    /// tells whether it has real stems (stem file or sidecar)
    static void deckLoaded(const QString& group,
            TrackPointer pTrack,
            std::shared_ptr<StemCache> pCache,
            bool realStems);

  private slots:
    void slotDeckLoaded(const QString& group,
            TrackPointer pTrack,
            std::shared_ptr<StemCache> pCache,
            bool realStems);
    void slotUpdateControls();

  private:
    struct Job;
    struct Deck {
        QString group;
        ControlPushButton* pSeparate = nullptr;
        ControlObject* pProgress = nullptr;
        ControlObject* pReady = nullptr;
        ControlObject* pPlayPosition = nullptr;
        ControlObject* pPlay = nullptr;
        bool realStems = false;
        // Seconds the playing deck spent on audio whose stems were requested
        // but not ready yet
        double starvedSeconds = 0;
        std::shared_ptr<Job> pJob; // null: nothing to separate
    };

    void requestSeparation(Deck* pDeck);
    /// Main thread: the separation does not keep up on this phone
    void checkPerformance();
    void reportSlow(const char* reason);
    void workerLoop();
    bool ensureNet();
    bool prepareJob(Job* pJob);
    std::shared_ptr<Job> pickJob(int* pSegment);

    UserSettingsPointer m_pConfig;
    // [Stems],quality: 0 automatic, 1 light (weak phones), 2 balanced
    ControlObject* m_pQuality = nullptr;
    // [Stems],slow_detected: 1 = the GUI proposes the light quality; the GUI
    // sets it back to 0 when the user answers
    ControlObject* m_pSlowDetected = nullptr;
    // [Stems],best_when_idle: separate again at the best quality while no
    // deck plays
    ControlObject* m_pBestWhenIdle = nullptr;
    // [Stems],speed: separation speed as a multiple of real time (0 = unknown)
    ControlObject* m_pSpeed = nullptr;
    bool m_slowReported = false; // once per session
    quint32 m_overBudgetMark = 0;
    int m_overBudgetTicks = 0;
    double m_busySeconds = 0;
    std::vector<std::unique_ptr<Deck>> m_decks;
    QTimer m_timer;

    // Worker thread state, guarded by m_mutex
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::vector<std::shared_ptr<Job>> m_jobs; // requested, not finished
    size_t m_nextJob = 0;
    bool m_quit = false;
    bool m_preloadNet = false; // a deck loaded a track that can be separated
    std::thread m_thread;

    // Worker thread: speed of the last segments (multiple of real time),
    // published for checkPerformance()
    std::atomic<double> m_speed{0};
    std::atomic<int> m_slowSegments{0}; // consecutive, while the deck plays
    std::atomic<bool> m_busy{false};

    // Worker thread only
    std::unique_ptr<HtdemucsNet> m_pNet;
    bool m_netFailed = false;
};

} // namespace stems
} // namespace mixxx
