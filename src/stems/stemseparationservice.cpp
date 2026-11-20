#include "stems/stemseparationservice.h"

#include <utility>
#ifdef __linux__
#include <pthread.h>
#endif
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "moc_stemseparationservice.cpp"
#include "sources/soundsourceproxy.h"
#include "stems/htdemucs.h"
#include "stems/scnet.h"
#include "stems/spleeter.h"
#include "stems/stemcache.h"
#include "track/track.h"
#include "util/logger.h"
#include "soundio/audiocallbackstats.h"
#include "util/threadpriority.h"

namespace mixxx {
namespace stems {

namespace {

const Logger kLogger("StemSeparation");

const QString kStemsGroup = QStringLiteral("[Stems]");
const ConfigKey kInstantStemsKey(kStemsGroup, QStringLiteral("InstantStems"));
const ConfigKey kQualityKey(kStemsGroup, QStringLiteral("Quality"));
const ConfigKey kBestWhenIdleKey(kStemsGroup, QStringLiteral("BestWhenIdle"));
const ConfigKey kLiveStemsKey(kStemsGroup, QStringLiteral("LiveStems"));
// Speed test (multiple of real time, 0 = not measured) and the revision of the
// test they come from: raised when models or kernels change, the test then runs again
const ConfigKey kSpeedLightKey(kStemsGroup, QStringLiteral("SpeedLight"));
const ConfigKey kSpeedBalancedKey(kStemsGroup, QStringLiteral("SpeedBalanced"));
const ConfigKey kSpeedBestKey(kStemsGroup, QStringLiteral("SpeedBest"));
const ConfigKey kSpeedRevisionKey(kStemsGroup, QStringLiteral("SpeedRevision"));
constexpr int kSpeedRevision = 1;
// The test of a new revision starts this long after the start, if no deck plays
constexpr double kSpeedTestDelaySeconds = 45.0;
// The automatic quality is the balanced one (htdemucs) unless the test measured
// less than this: a phone below it cannot keep ahead of two playing decks
// (each needs kMinSpeed). Measured on the POCO F4 GT: 2.0x without kernel
// tuning, ~3x tuned
constexpr double kAutoMinBalancedSpeed = 1.6;
// The best quality (SCNet, only while no deck plays) is skipped below this: a
// track would take more than 2.5 times its length. POCO F4 GT: ~0.65x
constexpr double kBestMinSpeed = 0.4;

// The separation of a playing deck must stay ahead of the play position.
// Below this multiple of real time, measured over a few segments while the
// deck plays, the phone is too slow for the current quality.
constexpr double kMinSpeed = 1.2;
constexpr int kSlowSegmentsToReport = 3;
// The playing deck reached audio whose stems were requested and are still
// missing, for this long in total
constexpr double kStarvedSecondsToReport = 6.0;
// Audio callbacks over their budget while the separation runs, within one
// 10 s window
constexpr quint32 kOverBudgetToReport = 4;
constexpr int kOverBudgetWindowTicks = 50; // x 200 ms
// demucs overlap: 0.1 as the stems prepared on the PC (stems/stemify_usb.py),
// 0.25 would cost a third more segments
constexpr double kOverlap = 0.1;

std::atomic<bool> s_instantStems{true};
QPointer<StemSeparationService> s_pInstance;

QString stemsDataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
            QStringLiteral("/stems");
}

QString speedTestMarkerPath() {
    return stemsDataDir() + QStringLiteral("/speedtest_running_rev%1").arg(kSpeedRevision);
}

} // namespace

bool instantStemsEnabled() {
    return s_instantStems.load(std::memory_order_relaxed);
}

QList<StemInfo> instantStemInfos() {
    return {StemInfo(QObject::tr("Drums"), QColor(0xFF, 0x40, 0x40)),
            StemInfo(QObject::tr("Bass + Melody"), QColor(0x00, 0xC8, 0xFF)),
            StemInfo(QString(), QColor(Qt::darkGray)),
            StemInfo(QObject::tr("Vocals"), QColor(0x40, 0xFF, 0x60))};
}

struct StemSeparationService::Job {
    QString group;
    TrackPointer pTrack;
    std::shared_ptr<StemCache> pCache;
    ControlObject* pPlayPosition = nullptr;
    ControlObject* pPlay = nullptr;
    std::atomic<bool> cancelled{false};
    /// Done or failed: never queued again
    std::atomic<bool> finished{false};
    /// SMART CFX asked for its stems (main thread)
    bool requested = false;
    // Worker thread only
    bool prepared = false;
    bool failed = false;
    std::unique_ptr<Separator> pSeparator;
    /// Quality of this separation
    StemCache::Level level = StemCache::kBalanced;
    /// The pass at the best quality while no deck plays
    bool idle = false;
};

StemSeparationService::StemSeparationService(
        UserSettingsPointer pConfig, int deckCount, QObject* pParent)
        : QObject(pParent),
          m_pConfig(pConfig) {
    s_instantStems.store(m_pConfig->getValue(kInstantStemsKey, true));
    m_pQuality = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("quality")));
    m_pQuality->setParent(this);
    m_pQuality->set(std::clamp(m_pConfig->getValue(kQualityKey, 0), 0, 2));
    connect(m_pQuality, &ControlObject::valueChanged, this, [this](double value) {
        const int quality = std::clamp(static_cast<int>(value), 0, 2);
        m_pConfig->setValue(kQualityKey, quality);
        kLogger.info() << "Quality set to" << quality;
        restartLiveJobs();
    });
    m_pSlowDetected = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("slow_detected")));
    m_pSlowDetected->setParent(this);
    m_pBestWhenIdle = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("best_when_idle")));
    m_pBestWhenIdle->setParent(this);
    m_pBestWhenIdle->set(m_pConfig->getValue(kBestWhenIdleKey, true) ? 1 : 0);
    connect(m_pBestWhenIdle, &ControlObject::valueChanged, this, [this](double value) {
        m_pConfig->setValue(kBestWhenIdleKey, value > 0 ? 1 : 0);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_wake.notify_all();
    });
    m_pLiveStems = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("live_stems")));
    m_pLiveStems->setParent(this);
    m_pLiveStems->set(m_pConfig->getValue(kLiveStemsKey, false) ? 1 : 0);
    connect(m_pLiveStems, &ControlObject::valueChanged, this, [this](double value) {
        m_pConfig->setValue(kLiveStemsKey, value > 0 ? 1 : 0);
        kLogger.info() << "Live stems" << (value > 0 ? "on" : "off");
        if (value > 0) {
            // The tracks already loaded too
            for (const auto& pDeck : m_decks) {
                if (!pDeck->realStems) {
                    requestSeparation(pDeck.get());
                }
            }
        }
    });
    m_pSpeed = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("speed")));
    m_pSpeed->setParent(this);
    m_pSpeed->setReadOnly();
    // The speed test: results of the last one (kept in the settings)
    const bool currentRevision = m_pConfig->getValue(kSpeedRevisionKey, 0) == kSpeedRevision;
    const auto makeSpeedControl = [this, currentRevision](const char* name, const ConfigKey& key) {
        auto* pControl = new ControlObject(ConfigKey(kStemsGroup, QString::fromLatin1(name)));
        pControl->setParent(this);
        pControl->setReadOnly();
        pControl->forceSet(currentRevision ? m_pConfig->getValue(key, 0.0) : 0.0);
        return pControl;
    };
    m_pSpeedLight = makeSpeedControl("speed_light", kSpeedLightKey);
    m_pSpeedBalanced = makeSpeedControl("speed_balanced", kSpeedBalancedKey);
    m_pSpeedBest = makeSpeedControl("speed_best", kSpeedBestKey);
    m_measuredBalanced.store(m_pSpeedBalanced->get(), std::memory_order_relaxed);
    m_measuredBest.store(m_pSpeedBest->get(), std::memory_order_relaxed);
    m_pSpeedTest = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("speed_test")));
    m_pSpeedTest->setParent(this);
    connect(m_pSpeedTest, &ControlObject::valueChanged, this, [this](double value) {
        if (value > 0) {
            kLogger.info() << "Speed test requested";
            m_speedTestRequested.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_wake.notify_all();
        }
    });
    // Never measured with this revision: the test starts by itself (see slotUpdateControls)
    m_autoTestChecked = currentRevision;
    for (int i = 1; i <= deckCount; ++i) {
        auto pDeck = std::make_unique<Deck>();
        pDeck->group = QStringLiteral("[Channel%1]").arg(i);
        pDeck->pSeparate = new ControlPushButton(
                ConfigKey(pDeck->group, QStringLiteral("stem_separate")));
        pDeck->pSeparate->setParent(this);
        pDeck->pProgress = new ControlObject(
                ConfigKey(pDeck->group, QStringLiteral("stem_progress")));
        pDeck->pProgress->setParent(this);
        pDeck->pProgress->setReadOnly();
        pDeck->pProgress->forceSet(-1);
        pDeck->pReady = new ControlObject(
                ConfigKey(pDeck->group, QStringLiteral("stems_ready")));
        pDeck->pReady->setParent(this);
        pDeck->pReady->setReadOnly();
        pDeck->pPlayPosition = ControlObject::getControl(
                ConfigKey(pDeck->group, QStringLiteral("playposition")));
        pDeck->pPlay = ControlObject::getControl(
                ConfigKey(pDeck->group, QStringLiteral("play")));
        Deck* pRaw = pDeck.get();
        connect(pDeck->pSeparate,
                &ControlObject::valueChanged,
                this,
                [this, pRaw](double value) {
                    if (value > 0) {
                        requestSeparation(pRaw);
                    }
                });
        m_decks.push_back(std::move(pDeck));
    }
    connect(&m_timer, &QTimer::timeout, this, &StemSeparationService::slotUpdateControls);
    m_timer.start(200);
    s_pInstance = this;
    m_thread = std::thread([this] { workerLoop(); });
}

StemSeparationService::~StemSeparationService() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
        for (const auto& pJob : m_jobs) {
            pJob->cancelled = true;
        }
    }
    m_wake.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

// static
void StemSeparationService::deckLoaded(const QString& group,
        TrackPointer pTrack,
        std::shared_ptr<StemCache> pCache,
        bool realStems) {
    QPointer<StemSeparationService> pService = s_pInstance;
    if (!pService) {
        return;
    }
    QMetaObject::invokeMethod(
            pService,
            [pService, group, pTrack, pCache, realStems]() {
                if (pService) {
                    pService->slotDeckLoaded(group, pTrack, pCache, realStems);
                }
            },
            Qt::QueuedConnection);
}

void StemSeparationService::slotDeckLoaded(const QString& group,
        TrackPointer pTrack,
        std::shared_ptr<StemCache> pCache,
        bool realStems) {
    for (const auto& pDeck : m_decks) {
        if (pDeck->group != group) {
            continue;
        }
        if (pDeck->pJob) {
            // The deck changed track: stop separating the previous one
            pDeck->pJob->cancelled = true;
            pDeck->pJob.reset();
        }
        if (pDeck->pIdleJob) {
            pDeck->pIdleJob->cancelled = true;
            pDeck->pIdleJob.reset();
        }
        pDeck->realStems = realStems;
        pDeck->starvedSeconds = 0;
        if (pCache && pTrack) {
            auto pJob = std::make_shared<Job>();
            pJob->group = group;
            pJob->pTrack = pTrack;
            pJob->pCache = pCache;
            pJob->pPlayPosition = pDeck->pPlayPosition;
            pJob->pPlay = pDeck->pPlay;
            pDeck->pJob = pJob;
            // Load the network now, while the track is being looked at, so
            // the first SMART CFX does not wait for it (~4.5 s)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_preloadNet = true;
                m_wake.notify_all();
            }
            // Live stems: separate every track without stems as soon as it
            // is loaded, without waiting for SMART CFX
            if (!realStems && m_pLiveStems->get() > 0) {
                requestSeparation(pDeck.get());
            }
        }
        slotUpdateControls();
        return;
    }
}

void StemSeparationService::requestSeparation(Deck* pDeck) {
    const auto& pJob = pDeck->pJob;
    if (!pJob || pJob->finished ||
            pJob->pCache->readyChunkCount() == pJob->pCache->chunkCount()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_jobs.begin(), m_jobs.end(), pJob) == m_jobs.end()) {
        kLogger.info() << pDeck->group << "Separating the stems of"
                       << pJob->pTrack->getLocation();
        pJob->requested = true;
        pJob->level = liveLevel();
        m_jobs.push_back(pJob);
        m_wake.notify_all();
    }
}

StemCache::Level StemSeparationService::liveLevel() const {
    const int quality = static_cast<int>(m_pQuality->get());
    if (quality == 1 || m_netFailed.load(std::memory_order_relaxed)) {
        return StemCache::kLight;
    }
    if (quality == 0) {
        // Automatic: what the speed test measured on this phone
        const double balanced = m_measuredBalanced.load(std::memory_order_relaxed);
        if (balanced > 0 && balanced < kAutoMinBalancedSpeed) {
            return StemCache::kLight;
        }
    }
    return StemCache::kBalanced;
}

StemCache::Level StemSeparationService::liveLevelForWorker() const {
    // ControlObject::get() is thread safe
    return liveLevel();
}

StemCache::Level StemSeparationService::bestLevel() const {
    const double best = m_measuredBest.load(std::memory_order_relaxed);
    const bool bestTooSlow = best > 0 && best < kBestMinSpeed;
    if (!m_bestNetFailed.load(std::memory_order_relaxed) && !bestTooSlow) {
        return StemCache::kBest;
    }
    return liveLevel();
}

void StemSeparationService::restartLiveJobs() {
    const StemCache::Level level = liveLevel();
    for (const auto& pDeck : m_decks) {
        const auto pOld = pDeck->pJob;
        if (!pOld || !pOld->requested || pOld->finished || pOld->level == level) {
            continue;
        }
        // Same track and cache, separated from now on at the new quality
        // (what is already separated is kept, see prepareJob())
        pOld->cancelled = true;
        auto pJob = std::make_shared<Job>();
        pJob->group = pOld->group;
        pJob->pTrack = pOld->pTrack;
        pJob->pCache = pOld->pCache;
        pJob->pPlayPosition = pOld->pPlayPosition;
        pJob->pPlay = pOld->pPlay;
        pDeck->pJob = pJob;
        kLogger.info() << pDeck->group << "Separation goes on at level" << level;
        requestSeparation(pDeck.get());
    }
}

void StemSeparationService::startIdleJobs() {
    const StemCache::Level level = bestLevel();
    for (const auto& pDeck : m_decks) {
        if (pDeck->realStems || !pDeck->pJob || pDeck->pIdleJob) {
            continue;
        }
        const auto& pCache = pDeck->pJob->pCache;
        if (pCache->minLevel(0, pCache->frames()) >= level) {
            continue;
        }
        auto pJob = std::make_shared<Job>();
        pJob->group = pDeck->group;
        pJob->pTrack = pDeck->pJob->pTrack;
        pJob->pCache = pCache;
        pJob->pPlayPosition = pDeck->pPlayPosition;
        pJob->pPlay = pDeck->pPlay;
        pJob->level = level;
        pJob->idle = true;
        pDeck->pIdleJob = pJob;
        kLogger.info() << pDeck->group << "No deck playing: separating at level" << level
                       << pJob->pTrack->getLocation();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.push_back(pJob);
        m_wake.notify_all();
    }
}

void StemSeparationService::slotUpdateControls() {
    bool anyPlaying = false;
    for (const auto& pDeck : m_decks) {
        anyPlaying = anyPlaying || (pDeck->pPlay && pDeck->pPlay->get() > 0);
    }
    // Performance mode ([Skin],performance_mode): the passes at the best
    // quality wait as if a deck were playing, so the CPU is left to audio
    // and rendering (jobs requested explicitly keep running)
    if (!m_pPerformanceMode) {
        m_pPerformanceMode = std::make_unique<ControlProxy>(
                ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("performance_mode")));
    }
    anyPlaying = anyPlaying || m_pPerformanceMode->toBool();
    if (anyPlaying != m_anyPlaying.load(std::memory_order_relaxed)) {
        m_anyPlaying.store(anyPlaying, std::memory_order_relaxed);
        if (!anyPlaying) {
            // Idle jobs waiting for the decks to stop
            std::lock_guard<std::mutex> lock(m_mutex);
            m_wake.notify_all();
        }
    }
    // The speed test of this phone: once per revision, a while after the start,
    // with no deck playing
    if (!m_autoTestChecked) {
        m_secondsSinceStart += 0.2;
        if (m_secondsSinceStart >= kSpeedTestDelaySeconds && !anyPlaying) {
            m_autoTestChecked = true;
            // A marker written before the test and removed when it ends: if the
            // test ever crashed the app, it is not started by itself again
            QFile marker(speedTestMarkerPath());
            if (marker.exists()) {
                kLogger.warning() << "The last automatic speed test did not finish: "
                                     "not started again (the Dashboard button does it)";
            } else {
                QDir().mkpath(QFileInfo(marker).absolutePath());
                if (marker.open(QIODevice::WriteOnly)) {
                    marker.close();
                }
                m_pSpeedTest->set(1);
            }
        }
    }
    m_idleSeconds = anyPlaying ? 0.0 : m_idleSeconds + 0.2;
    constexpr double kIdleSecondsBeforeBest = 10.0;
    if (m_idleSeconds >= kIdleSecondsBeforeBest && m_pBestWhenIdle->get() > 0) {
        startIdleJobs();
    }

    for (const auto& pDeck : m_decks) {
        if (pDeck->realStems) {
            pDeck->pProgress->forceSet(-1);
            pDeck->pReady->forceSet(1);
            continue;
        }
        if (!pDeck->pJob) {
            pDeck->pProgress->forceSet(-1);
            pDeck->pReady->forceSet(0);
            continue;
        }
        const auto& pCache = pDeck->pJob->pCache;
        pDeck->pProgress->forceSet(
                double(pCache->readyChunkCount()) / std::max(1L, pCache->chunkCount()));
        const double position = pDeck->pPlayPosition ? pDeck->pPlayPosition->get() : 0.0;
        const long frame = static_cast<long>(std::max(0.0, position) * pCache->frames());
        const bool ready = pCache->isFrameReady(frame);
        pDeck->pReady->forceSet(ready ? 1.0 : 0.0);
        // Stems requested, the first ones already there (the start-up of the
        // separation does not count), and the deck plays where they are not
        if (!ready && pDeck->pJob->requested && !pDeck->pJob->finished &&
                pCache->readyChunkCount() > 0 && pDeck->pPlay &&
                pDeck->pPlay->get() > 0) {
            pDeck->starvedSeconds += 0.2;
        }
    }
    checkPerformance();
}

void StemSeparationService::checkPerformance() {
    const double speed = m_speed.load(std::memory_order_relaxed);
    m_pSpeed->forceSet(speed);
    if (m_slowReported) {
        return;
    }
    if (m_slowSegments.load(std::memory_order_relaxed) >= kSlowSegmentsToReport) {
        reportSlow("separation slower than playback");
        return;
    }
    for (const auto& pDeck : m_decks) {
        if (pDeck->starvedSeconds >= kStarvedSecondsToReport) {
            reportSlow("the play position caught up with the separation");
            return;
        }
    }
    // Audio dropouts that only happen while separating
    const quint32 overBudget = AudioCallbackStats::overBudgetTotal();
    if (!m_busy.load(std::memory_order_relaxed)) {
        m_overBudgetMark = overBudget;
        m_overBudgetTicks = 0;
        return;
    }
    if (++m_overBudgetTicks > kOverBudgetWindowTicks) {
        m_overBudgetMark = overBudget;
        m_overBudgetTicks = 0;
    } else if (overBudget - m_overBudgetMark >= kOverBudgetToReport) {
        reportSlow("audio callbacks over budget while separating");
    }
}

void StemSeparationService::reportSlow(const char* reason) {
    m_slowReported = true;
    kLogger.warning() << "This phone is too slow for the stem quality"
                      << m_pQuality->get() << ":" << reason << "- speed"
                      << m_speed.load(std::memory_order_relaxed) << "x real time";
    // Already at the lightest quality: nothing to propose
    if (static_cast<int>(m_pQuality->get()) != 1) {
        m_pSlowDetected->forceSet(1);
    }
}

// ------------------------------------------------------------ worker thread

/// htdemucs configuration with the best OpenCL kernel tuning already done
static HtdemucsNet::Config htdemucsConfig() {
    const QString dir = stemsDataDir();
    HtdemucsNet::Config config;
    config.modelPath = QString(dir + QStringLiteral("/htdemucs3.mnn")).toStdString();
    config.cacheDir = dir.toStdString();
    config.backend = HtdemucsNet::Backend::OpenCl;
    config.lowPrecision = true;
    // Use the best kernel tuning already done; tuning now would take minutes
    // (see tuneKernels(), which does it once while no deck plays)
    config.tuning = HtdemucsNet::Config::Tuning::Wide;
    if (!QFile::exists(QString::fromStdString(HtdemucsNet::cacheFile(config)))) {
        config.tuning = HtdemucsNet::Config::Tuning::Fast;
        if (!QFile::exists(QString::fromStdString(HtdemucsNet::cacheFile(config)))) {
            config.tuning = HtdemucsNet::Config::Tuning::None;
        }
    }
    return config;
}

bool StemSeparationService::ensureNet(StemCache::Level level) {
    if (level == StemCache::kBest) {
        if (m_pBestNet) {
            return true;
        }
        if (m_bestNetFailed.load(std::memory_order_relaxed)) {
            return false;
        }
        const QString dir = stemsDataDir();
        QDir().mkpath(dir);
        const QString model = dir + QStringLiteral("/scnet_small.mnn");
#ifdef __ANDROID__
        const QString asset = QStringLiteral("assets:/stems/scnet_small.mnn");
        if (QFileInfo(model).size() != QFileInfo(asset).size()) {
            QFile::remove(model);
            if (!QFile::copy(asset, model)) {
                kLogger.warning() << "Cannot copy the best model to" << model;
                m_bestNetFailed = true;
                return false;
            }
        }
#endif
        auto pNet = std::make_unique<ScnetNet>();
        // 2 threads: it only runs while no deck plays, but a segment takes
        // seconds and cannot be interrupted when a deck starts
        if (!pNet->init(model.toStdString(), 2)) {
            kLogger.warning() << "Cannot load the best stem model:"
                              << QString::fromStdString(pNet->error());
            m_bestNetFailed = true;
            return false;
        }
        m_pBestNet = std::move(pNet);
        return true;
    }
    if (level == StemCache::kLight) {
        if (m_pLightNet) {
            return true;
        }
        if (m_lightNetFailed.load(std::memory_order_relaxed)) {
            return false;
        }
        const QString dir = stemsDataDir();
        QDir().mkpath(dir);
        const QString model = dir + QStringLiteral("/spleeter4.mnn");
#ifdef __ANDROID__
        const QString asset = QStringLiteral("assets:/stems/spleeter4.mnn");
        if (QFileInfo(model).size() != QFileInfo(asset).size()) {
            QFile::remove(model);
            if (!QFile::copy(asset, model)) {
                kLogger.warning() << "Cannot copy the light model to" << model;
                m_lightNetFailed = true;
                return false;
            }
        }
#endif
        const auto start = std::chrono::steady_clock::now();
        auto pNet = std::make_unique<SpleeterNet>();
        // 4 threads: ~12x real time on a Snapdragon 8 Gen 1, still above
        // real time on phones a few times slower (the thread runs below
        // the audio, see workerLoop())
        if (!pNet->init(model.toStdString(), 3)) {
            kLogger.warning() << "Cannot load the light stem model:"
                              << QString::fromStdString(pNet->error());
            m_lightNetFailed = true;
            return false;
        }
        kLogger.info() << "Light stem model loaded in"
                       << std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - start)
                                  .count()
                       << "s";
        m_pLightNet = std::move(pNet);
        return true;
    }
    if (m_pNet) {
        return true;
    }
    if (m_netFailed.load(std::memory_order_relaxed)) {
        return false;
    }
    const QString dir = stemsDataDir();
    QDir().mkpath(dir);
    const QString model = dir + QStringLiteral("/htdemucs3.mnn");
#ifdef __ANDROID__
    // The model is an asset of the APK: MNN needs a real file
    const QString asset = QStringLiteral("assets:/stems/htdemucs3.mnn");
    if (QFileInfo(model).size() != QFileInfo(asset).size()) {
        QFile::remove(model);
        if (!QFile::copy(asset, model)) {
            kLogger.warning() << "Cannot copy the model to" << model;
            m_netFailed.store(true);
            return false;
        }
    }
#endif
    HtdemucsNet::Config config = htdemucsConfig();
    const auto start = std::chrono::steady_clock::now();
    auto pNet = std::make_unique<HtdemucsNet>();
    if (!pNet->init(config)) {
        kLogger.warning() << "OpenCL:" << QString::fromStdString(pNet->error())
                          << "- using the CPU";
        config.backend = HtdemucsNet::Backend::Cpu;
        pNet = std::make_unique<HtdemucsNet>();
        if (!pNet->init(config)) {
            kLogger.warning() << "Cannot load the stem model:"
                              << QString::fromStdString(pNet->error());
            m_netFailed.store(true);
            return false;
        }
    }
    kLogger.info() << "Stem model loaded in"
                   << std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - start)
                              .count()
                   << "s, cache" << QString::fromStdString(HtdemucsNet::cacheFile(config));
    m_pNet = std::move(pNet);
    return true;
}

std::unique_ptr<Separator> StemSeparationService::makeSeparator(StemCache::Level level) {
    if (level == StemCache::kLight) {
        return std::make_unique<SpleeterSeparator>(m_pLightNet.get());
    }
    if (level == StemCache::kBest) {
        return std::make_unique<TrackSeparator>(
                static_cast<SegmentNet*>(m_pBestNet.get()), kOverlap);
    }
    return std::make_unique<TrackSeparator>(m_pNet.get(), kOverlap);
}

namespace {

/// Pseudo music for the speed test: tones and noise. The speed of the networks
/// does not depend on what is played, but the audio must not be constant (the
/// normalization of a track divides by its standard deviation)
std::vector<float> syntheticAudio(long frames) {
    std::vector<float> audio(static_cast<size_t>(frames) * 2);
    uint32_t state = 12345u;
    for (long i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        state = state * 1664525u + 1013904223u;
        const float noise = (static_cast<int>(state >> 16) - 32768) / 32768.0f;
        const float tone = 0.25f * static_cast<float>(std::sin(6.2831853 * 110.0 * t)) +
                0.15f * static_cast<float>(std::sin(6.2831853 * 440.0 * t)) +
                0.08f * static_cast<float>(std::sin(6.2831853 * 3520.0 * t * (1.0 + 0.1 * std::sin(t))));
        audio[2 * i] = tone + 0.05f * noise;
        audio[2 * i + 1] = 0.9f * tone - 0.05f * noise;
    }
    return audio;
}

} // namespace

double StemSeparationService::measureSpeed(
        const std::function<std::unique_ptr<Separator>()>& make) {
    // 12 s: a whole segment of every model (7.8 s, 11 s, 11.9 s)
    constexpr long kFrames = 12L * kSampleRate;
    const std::vector<float> audio = syntheticAudio(kFrames);
    double speed = 0;
    // The first pass only warms up (GPU kernels, memory); the second is timed
    for (int pass = 0; pass < 2; ++pass) {
        const std::unique_ptr<Separator> pSeparator = make();
        if (!pSeparator) {
            return 0;
        }
        pSeparator->setTrack(audio.data(), kFrames);
        const auto start = std::chrono::steady_clock::now();
        if (!pSeparator->processSegment(0, [](long, long, const float*) {})) {
            return 0;
        }
        const double seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start)
                                       .count();
        speed = seconds > 0 ? pSeparator->segmentSeconds() / seconds : 0;
    }
    return speed;
}

bool StemSeparationService::tuneKernels() {
    // Kernel tuning of the GPU, once per phone: the first run of the network
    // with it takes ~25 s (FAST) and leaves a cache that the next starts use:
    // 3.3 -> 2.2 s per segment on the POCO F4 GT
    if (!m_pNet || m_pNet->backend() != HtdemucsNet::Backend::OpenCl) {
        return false;
    }
    HtdemucsNet::Config config = htdemucsConfig();
    if (config.tuning != HtdemucsNet::Config::Tuning::None) {
        return false; // already tuned
    }
    config.tuning = HtdemucsNet::Config::Tuning::Fast;
    const auto start = std::chrono::steady_clock::now();
    HtdemucsNet net;
    if (!net.init(config) || net.backend() != HtdemucsNet::Backend::OpenCl) {
        return false;
    }
    const std::vector<float> audio = syntheticAudio(kSegmentFrames);
    TrackSeparator separator(&net, kOverlap);
    separator.setTrack(audio.data(), kSegmentFrames);
    // The cache file is written after the first run
    if (!separator.processSegment(0, [](long, long, const float*) {})) {
        return false;
    }
    const bool saved = QFile::exists(QString::fromStdString(HtdemucsNet::cacheFile(config)));
    kLogger.info() << "GPU kernels tuned in"
                   << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                   << "s, cache saved:" << saved;
    return saved;
}

bool StemSeparationService::speedTestDue() const {
    return m_speedTestRequested.load(std::memory_order_relaxed) &&
            !m_anyPlaying.load(std::memory_order_relaxed);
}

void StemSeparationService::runSpeedTest() {
    m_speedTestRequested.store(false, std::memory_order_relaxed);
    const auto start = std::chrono::steady_clock::now();
    kLogger.info() << "Speed test started";
    const auto interrupted = [this]() {
        // A deck started: the test goes on when they stop again
        if (m_anyPlaying.load(std::memory_order_relaxed)) {
            kLogger.info() << "Speed test interrupted: a deck plays";
            m_speedTestRequested.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    };
    double light = 0;
    double balanced = 0;
    double best = 0;
    if (ensureNet(StemCache::kLight)) {
        light = measureSpeed([this]() { return makeSeparator(StemCache::kLight); });
    }
    if (interrupted()) {
        return;
    }
    if (ensureNet(StemCache::kBalanced)) {
        balanced = measureSpeed([this]() { return makeSeparator(StemCache::kBalanced); });
    }
    if (interrupted()) {
        return;
    }
    const bool tuned = tuneKernels();
    if (interrupted()) {
        return;
    }
    if (ensureNet(StemCache::kBest)) {
        best = measureSpeed([this]() { return makeSeparator(StemCache::kBest); });
    }
    kLogger.info() << "Speed test: light" << light << "x, balanced" << balanced << "x, best"
                   << best << "x real time, in"
                   << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                   << "s" << (tuned ? "(GPU kernels tuned: faster from the next start)" : "");
    QMetaObject::invokeMethod(
            this,
            [this, light, balanced, best, tuned]() { publishSpeeds(light, balanced, best, tuned); },
            Qt::QueuedConnection);
}

void StemSeparationService::publishSpeeds(double light, double balanced, double best, bool retest) {
    m_pConfig->setValue(kSpeedLightKey, light);
    m_pConfig->setValue(kSpeedBalancedKey, balanced);
    m_pConfig->setValue(kSpeedBestKey, best);
    // With the GPU kernels tuned now the next start is faster than what was
    // measured: it measures again
    m_pConfig->setValue(kSpeedRevisionKey, retest ? 0 : kSpeedRevision);
    m_pSpeedLight->forceSet(light);
    m_pSpeedBalanced->forceSet(balanced);
    m_pSpeedBest->forceSet(best);
    m_measuredBalanced.store(balanced, std::memory_order_relaxed);
    m_measuredBest.store(best, std::memory_order_relaxed);
    m_pSpeedTest->set(0);
    QFile::remove(speedTestMarkerPath());
    kLogger.info() << "Automatic quality: live" << static_cast<int>(liveLevel()) << ", best"
                   << static_cast<int>(bestLevel());
    // The automatic level may have changed
    restartLiveJobs();
}

bool StemSeparationService::prepareJob(Job* pJob) {
    pJob->prepared = true;
    const auto start = std::chrono::steady_clock::now();
    AudioSource::OpenParams params;
    params.setChannelCount(audio::ChannelCount::stereo());
    auto pSource = SoundSourceProxy(pJob->pTrack).openAudioSource(params);
    if (!pSource || pSource->getSignalInfo().getChannelCount() != audio::ChannelCount::stereo()) {
        kLogger.warning() << pJob->group << "Cannot decode" << pJob->pTrack->getLocation();
        return false;
    }
    if (pSource->getSignalInfo().getSampleRate().value() != kSampleRate) {
        // TODO: resample (htdemucs works at 44.1 kHz)
        kLogger.warning() << pJob->group << "Not 44.1 kHz, no instant stems:"
                          << pSource->getSignalInfo().getSampleRate().value();
        return false;
    }
    const IndexRange range = pSource->frameIndexRange();
    const long frames = pJob->pCache->frames();
    std::vector<float> audio(size_t(frames) * 2, 0.0f);
    constexpr SINT kBlock = 65536;
    for (SINT pos = range.start(); pos < range.end() && !pJob->cancelled;) {
        const SINT n = std::min(kBlock, range.end() - pos);
        const long offset = pos - range.start();
        if (offset + n > frames) {
            break;
        }
        pSource->readSampleFrames(WritableSampleFrames(IndexRange::forward(pos, n),
                SampleBuffer::WritableSlice(audio.data() + offset * 2, n * 2)));
        pos += n;
    }
    if (pJob->cancelled) {
        return false;
    }
    pJob->pSeparator = makeSeparator(pJob->level);
    pJob->pSeparator->setTrack(audio.data(), frames);
    if (pJob->idle) {
        // Everything is separated again; the chunks keep their current
        // stems until the new ones are complete
        pJob->pCache->beginPass();
    } else if (pJob->pSeparator->canSkipSegments()) {
        // The parts already separated (at this quality or a better one, e.g.
        // before switching to the light quality) are kept
        int skipped = 0;
        for (int segment = 0; segment < pJob->pSeparator->segmentCount(); ++segment) {
            long first = 0;
            long end = 0;
            pJob->pSeparator->segmentRange(segment, &first, &end);
            if (pJob->pCache->minLevel(first, end) >= pJob->level) {
                pJob->pSeparator->skipSegment(segment);
                ++skipped;
            }
        }
        if (skipped > 0) {
            kLogger.info() << pJob->group << skipped << "segments already separated";
        }
        if (skipped == pJob->pSeparator->segmentCount()) {
            return false;
        }
    }
    kLogger.info() << pJob->group << "Decoded" << frames << "frames in"
                   << std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - start)
                              .count()
                   << "s," << pJob->pSeparator->segmentCount() << "segments";
    return true;
}

std::shared_ptr<StemSeparationService::Job> StemSeparationService::pickJob(int* pSegment) {
    // Caller holds m_mutex. Round robin over the decks; in each, the first
    // segment not done from the play position, then from the beginning.
    m_jobs.erase(std::remove_if(m_jobs.begin(),
                         m_jobs.end(),
                         [](const auto& pJob) { return pJob->cancelled.load(); }),
            m_jobs.end());
    // The decks that may play first; the pass at the best quality only
    // while no deck plays
    const bool anyPlaying = m_anyPlaying.load(std::memory_order_relaxed);
    for (const bool idle : {false, true}) {
    if (idle && anyPlaying) {
        break;
    }
    for (size_t k = 0; k < m_jobs.size(); ++k) {
        const size_t index = (m_nextJob + k) % m_jobs.size();
        const auto& pJob = m_jobs[index];
        if (pJob->idle != idle) {
            continue;
        }
        if (!pJob->prepared) {
            m_nextJob = index + 1;
            *pSegment = -1;
            return pJob;
        }
        if (!pJob->pSeparator) {
            continue;
        }
        const Separator& sep = *pJob->pSeparator;
        const double position = pJob->pPlayPosition ? pJob->pPlayPosition->get() : 0.0;
        const int from = sep.segmentAt(static_cast<long>(std::max(0.0, position) * sep.frames()));
        for (int n = 0; n < sep.segmentCount(); ++n) {
            const int segment = (from + n) % sep.segmentCount();
            if (!sep.isSegmentDone(segment)) {
                m_nextJob = index + 1;
                *pSegment = segment;
                return pJob;
            }
        }
    }
    }
    return nullptr;
}

bool StemSeparationService::hasRunnableJob() const {
    // Caller holds m_mutex
    const bool anyPlaying = m_anyPlaying.load(std::memory_order_relaxed);
    return std::any_of(m_jobs.begin(), m_jobs.end(), [anyPlaying](const auto& pJob) {
        return pJob->cancelled.load() || !pJob->idle || !anyPlaying;
    });
}

void StemSeparationService::workerLoop() {
#ifdef __linux__
    // Otherwise it inherits the name of the thread that started it
    pthread_setname_np(pthread_self(), "StemSeparation");
#endif
    // Below the audio and the GUI (the GPU has no priorities). Done before
    // the network is created so that the CPU thread pool of the inference
    // runtime inherits it. Not the lowest level: the stems of a playing deck
    // must keep up with the play position.
    mixxx::demoteCurrentThreadToBackground(
            mixxx::kThreadNiceLowest, "StemSeparation");
    while (true) {
        std::shared_ptr<Job> pJob;
        int segment = -1;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            // The speed test waits while a track is being separated for a deck
            // that may play (it runs between the segments of the idle passes)
            const auto speedTestNow = [this]() {
                return speedTestDue() &&
                        std::all_of(m_jobs.begin(), m_jobs.end(), [](const auto& pJob) {
                            return pJob->idle || pJob->cancelled.load();
                        });
            };
            m_wake.wait(lock, [this, &speedTestNow] {
                return m_quit || m_preloadNet || hasRunnableJob() || speedTestNow();
            });
            if (m_quit) {
                return;
            }
            if (speedTestNow()) {
                lock.unlock();
                runSpeedTest();
                continue;
            }
            const bool preload = std::exchange(m_preloadNet, false);
            pJob = pickJob(&segment);
            if (!pJob) {
                if (preload) {
                    lock.unlock();
                    ensureNet(liveLevelForWorker());
                }
                continue;
            }
        }
        if (!ensureNet(pJob->level)) {
            if (pJob->level == StemCache::kBest && ensureNet(StemCache::kBalanced)) {
                pJob->level = StemCache::kBalanced;
            } else if (pJob->level == StemCache::kBalanced && ensureNet(StemCache::kLight)) {
                // No htdemucs on this phone: the light quality instead
                kLogger.warning() << "Using the light stem quality";
                pJob->level = StemCache::kLight;
            } else {
                pJob->finished = true;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_jobs.erase(std::remove(m_jobs.begin(), m_jobs.end(), pJob), m_jobs.end());
                continue;
            }
        }
        if (segment < 0) {
            if (!prepareJob(pJob.get())) {
                pJob->finished = true;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_jobs.erase(std::remove(m_jobs.begin(), m_jobs.end(), pJob), m_jobs.end());
            }
            continue;
        }
        const auto start = std::chrono::steady_clock::now();
        StemCache* pCache = pJob->pCache.get();
        const StemCache::Level level = pJob->level;
        // The audio dropout check is only about the separations that run
        // while decks may play
        m_busy.store(!pJob->idle, std::memory_order_relaxed);
        const bool ok = pJob->pSeparator->processSegment(segment,
                [pCache, level](long first, long count, const float* stems) {
                    pCache->write(first, count, stems, level);
                });
        m_busy.store(false, std::memory_order_relaxed);
        const double seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start)
                                       .count();
        kLogger.debug() << pJob->group << "segment" << segment << "in" << seconds << "s";
        if (ok && seconds > 0 && !pJob->idle) {
            const double speed = pJob->pSeparator->segmentSeconds() / seconds;
            const double previous = m_speed.load(std::memory_order_relaxed);
            m_speed.store(previous > 0 ? 0.6 * previous + 0.4 * speed : speed,
                    std::memory_order_relaxed);
            // Only while the deck plays: then the separation must keep up
            const bool playing = pJob->pPlay && pJob->pPlay->get() > 0;
            if (!playing) {
                // keep the count: a pause does not make the phone faster
            } else if (speed < kMinSpeed) {
                m_slowSegments.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_slowSegments.store(0, std::memory_order_relaxed);
            }
        }
        const bool finished = pJob->pSeparator->doneCount() == pJob->pSeparator->segmentCount();
        if (!ok || finished) {
            if (!ok) {
                kLogger.warning() << pJob->group << "Separation failed at level" << level
                                  << QString::fromStdString(level == StemCache::kLight
                                                     ? m_pLightNet->error()
                                                     : (level == StemCache::kBest
                                                                       ? m_pBestNet->error()
                                                                       : m_pNet->error()));
            } else {
                kLogger.info() << pJob->group << "Stems complete at level" << level
                               << (pJob->idle ? "(while no deck played)" : "")
                               << pJob->pTrack->getLocation();
            }
            // The decoded track and the segments are not needed any more
            pJob->finished = true;
            pJob->pSeparator.reset();
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.erase(std::remove(m_jobs.begin(), m_jobs.end(), pJob), m_jobs.end());
        }
    }
}

} // namespace stems
} // namespace mixxx
