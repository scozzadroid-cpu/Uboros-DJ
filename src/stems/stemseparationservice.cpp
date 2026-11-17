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

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "moc_stemseparationservice.cpp"
#include "sources/soundsourceproxy.h"
#include "stems/htdemucs.h"
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
    m_pSpeed = new ControlObject(ConfigKey(kStemsGroup, QStringLiteral("speed")));
    m_pSpeed->setParent(this);
    m_pSpeed->setReadOnly();
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
            std::lock_guard<std::mutex> lock(m_mutex);
            m_preloadNet = true;
            m_wake.notify_all();
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
    if (static_cast<int>(m_pQuality->get()) == 1 ||
            m_netFailed.load(std::memory_order_relaxed)) {
        return StemCache::kLight;
    }
    return StemCache::kBalanced;
}

StemCache::Level StemSeparationService::liveLevelForWorker() const {
    // ControlObject::get() is thread safe
    return liveLevel();
}

StemCache::Level StemSeparationService::bestLevel() const {
    return m_netFailed.load(std::memory_order_relaxed) ? StemCache::kLight
                                                        : StemCache::kBalanced;
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
    if (anyPlaying != m_anyPlaying.load(std::memory_order_relaxed)) {
        m_anyPlaying.store(anyPlaying, std::memory_order_relaxed);
        if (!anyPlaying) {
            // Idle jobs waiting for the decks to stop
            std::lock_guard<std::mutex> lock(m_mutex);
            m_wake.notify_all();
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

bool StemSeparationService::ensureNet(StemCache::Level level) {
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
        if (!pNet->init(model.toStdString(), 4)) {
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
    HtdemucsNet::Config config;
    config.modelPath = model.toStdString();
    config.cacheDir = dir.toStdString();
    config.backend = HtdemucsNet::Backend::OpenCl;
    config.lowPrecision = true;
    // Use the best kernel tuning already done; tuning now would take minutes
    config.tuning = HtdemucsNet::Config::Tuning::Wide;
    if (!QFile::exists(QString::fromStdString(HtdemucsNet::cacheFile(config)))) {
        config.tuning = HtdemucsNet::Config::Tuning::Fast;
        if (!QFile::exists(QString::fromStdString(HtdemucsNet::cacheFile(config)))) {
            config.tuning = HtdemucsNet::Config::Tuning::None;
        }
    }
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
    if (pJob->level == StemCache::kLight) {
        pJob->pSeparator = std::make_unique<SpleeterSeparator>(m_pLightNet.get());
    } else {
        pJob->pSeparator = std::make_unique<TrackSeparator>(m_pNet.get(), kOverlap);
    }
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
            mixxx::kThreadNiceBackground, "StemSeparation");
    while (true) {
        std::shared_ptr<Job> pJob;
        int segment = -1;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this] { return m_quit || m_preloadNet || hasRunnableJob(); });
            if (m_quit) {
                return;
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
            if (pJob->level == StemCache::kBalanced && ensureNet(StemCache::kLight)) {
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
                                                     : m_pNet->error());
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
