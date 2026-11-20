#pragma once

#include <functional>
#include <memory>

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

#include "control/controlproxy.h"
#include "preferences/usersettings.h"
#include "stems/separator.h"
#include "stems/stemcache.h"
#include "track/steminfo.h"
#include "track/track_decl.h"

class ControlObject;
class ControlPushButton;

namespace mixxx {
namespace stems {

class StemCache;
class HtdemucsNet;
class SpleeterNet;
class ScnetNet;
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
        // Separates the track again at the best quality while no deck plays
        std::shared_ptr<Job> pIdleJob;
    };

    void requestSeparation(Deck* pDeck);
    /// Quality of the stems separated while a deck may play
    StemCache::Level liveLevel() const;
    /// Quality of the separation while no deck plays
    StemCache::Level bestLevel() const;
    StemCache::Level liveLevelForWorker() const;
    bool hasRunnableJob() const;
    /// The quality changed: the separations in progress go on with it
    void restartLiveJobs();
    void startIdleJobs();
    /// Main thread: the separation does not keep up on this phone
    void checkPerformance();
    void reportSlow(const char* reason);
    void workerLoop();
    bool ensureNet(StemCache::Level level);
    std::unique_ptr<Separator> makeSeparator(StemCache::Level level);
    /// Worker thread: seconds of audio separated per second by this phone
    /// (multiple of real time, 0 = cannot be measured, -1 = stopped because a deck
    /// plays or the app quits), on a synthetic signal: one warm-up segment (not for
    /// the CPU network, whose segment lasts ~17 s), then the timed one
    double measureSpeed(const std::function<std::unique_ptr<Separator>()>& make, bool warmUp);
    /// Worker thread: the speed test of every quality; tunes the GPU kernels
    /// once if that was never done
    void runSpeedTest();
    /// Worker thread: true if the GPU kernels were tuned now (next start is faster)
    bool tuneKernels();
    bool speedTestDue() const;
    /// Main thread: stores the result, shows it, applies it to the live jobs
    void publishSpeeds(double light, double balanced, double best, bool retest);
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
    // [Stems],live_stems: separate the tracks without stems as soon as they
    // are loaded (toolbar button "Live Stems")
    ControlObject* m_pLiveStems = nullptr;
    // [Stems],speed: separation speed as a multiple of real time (0 = unknown)
    ControlObject* m_pSpeed = nullptr;
    // [Stems],speed_light / speed_balanced / speed_best: speed measured by the
    // speed test, multiple of real time (0 = not measured); the automatic
    // quality follows them
    ControlObject* m_pSpeedLight = nullptr;
    ControlObject* m_pSpeedBalanced = nullptr;
    ControlObject* m_pSpeedBest = nullptr;
    // [Stems],speed_test: set to 1 to measure (the test waits for the decks to
    // stop), back to 0 when it is done
    ControlObject* m_pSpeedTest = nullptr;
    std::atomic<bool> m_speedTestRequested{false};
    // Set by the destructor: the speed test does not hold the exit up
    std::atomic<bool> m_quitting{false};
    // Measured speeds read by liveLevel() / bestLevel() (any thread)
    std::atomic<double> m_measuredBalanced{0};
    std::atomic<double> m_measuredBest{0};
    bool m_autoTestChecked = false;
    double m_secondsSinceStart = 0;
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
    // true while a deck plays or the performance mode is on: the idle
    // (best quality) passes wait
    std::atomic<bool> m_anyPlaying{false};
    std::unique_ptr<ControlProxy> m_pPerformanceMode;
    // Set by the worker when a network cannot be loaded on this phone
    std::atomic<bool> m_netFailed{false};
    std::atomic<bool> m_lightNetFailed{false};
    std::atomic<bool> m_bestNetFailed{false};
    double m_idleSeconds = 0; // main thread

    // Worker thread only
    std::unique_ptr<HtdemucsNet> m_pNet;
    std::unique_ptr<SpleeterNet> m_pLightNet;
    std::unique_ptr<ScnetNet> m_pBestNet;
};

} // namespace stems
} // namespace mixxx
