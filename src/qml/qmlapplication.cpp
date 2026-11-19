#include "qmlapplication.h"

#include <qtextdocument.h>

#include <QQmlEngineExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#ifdef __ANDROID__
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QPointer>
#include <QTimer>
#include <chrono>
#include <memory>
#include <mutex>
#ifdef __ANDROID__
#include <android/log.h>
#include <sys/system_properties.h>
#endif
#endif

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "controllers/controllermanager.h"
#include "controllers/keyboard/keyboardeventfilter.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/effectchain.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/visibleeffectslist.h"
#include "mixer/playermanager.h"
#include "moc_qmlapplication.cpp"
#include "preferences/configobject.h"
#include "qml/asyncimageprovider.h"
#include "qml/qmldlgpreferencesproxy.h"
#include "soundio/soundmanager.h"
#include "qml/qmlwaveformdisplay.h"
#include "util/threadpriority.h"
#include "util/versionstore.h"
#include "waveform/visualsmanager.h"
#include "waveform/waveformwidgetfactory.h"
Q_IMPORT_QML_PLUGIN(MixxxPlugin)
Q_IMPORT_QML_PLUGIN(Mixxx_ControlsPlugin)

namespace {
const QString kMainQmlFileName = QStringLiteral("qml/main.qml");

// Converts a (capturing) lambda into a function pointer that can be passed to
// qmlRegisterSingletonType.
template<class F>
auto lambda_to_singleton_type_factory_ptr(F&& f) {
    static F fn = std::forward<F>(f);
    return [](QQmlEngine* pEngine, QJSEngine* pScriptEngine) -> QObject* {
        return fn(pEngine, pScriptEngine);
    };
}
} // namespace

namespace mixxx {
namespace qml {

#ifdef __ANDROID__
namespace {
// Preferred refresh rate of the main window (WindowManager.LayoutParams)
void setWindowRefreshRate(float hz) {
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([hz]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            return;
        }
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) {
            return;
        }
        QJniObject params = window.callObjectMethod(
                "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
        if (!params.isValid()) {
            return;
        }
        params.setField<jfloat>("preferredRefreshRate", hz);
        window.callMethod<void>("setAttributes",
                "(Landroid/view/WindowManager$LayoutParams;)V",
                params.object());
    });
}
} // namespace
#endif

namespace {
qint64 nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
}
} // namespace

struct QmlApplication::PerfHint {
#ifdef __ANDROID__
    QJniObject session;
    QMetaObject::Connection beginConnection;
    QMetaObject::Connection endConnection;
    QTimer* pCheckTimer = nullptr;
    // Guards session/sessionOk: the frame slots run on the render thread,
    // closeSession()/createSession() on the GUI thread.
    std::mutex sessionMutex;
    std::atomic<qint64> frameStartNs{nowNs()};
    std::vector<int> tids;
    qint64 targetNs = 0;
    int audioTid = 0;
    bool sessionOk = false;
    QPointer<QQuickWindow> window;

    static std::vector<int> collectTids() {
        std::vector<int> tids;
        tids.push_back(static_cast<int>(gettid())); // Qt GUI thread
        const int audio = mixxx::g_audioCallbackTid.load(std::memory_order_relaxed);
        if (audio > 0) {
            tids.push_back(audio);
        }
        // Qt Quick render thread, when the threaded render loop is used
        if (QDir dir(QStringLiteral("/proc/self/task")); dir.exists()) {
            const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& tid : entries) {
                QFile comm(dir.filePath(tid + QStringLiteral("/comm")));
                if (comm.open(QIODevice::ReadOnly) &&
                        comm.readAll().startsWith("QSGRenderThread")) {
                    tids.push_back(tid.toInt());
                }
            }
        }
        return tids;
    }

    bool createSession() {
        std::lock_guard<std::mutex> lock(sessionMutex);
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            return false;
        }
        QJniObject name = QJniObject::fromString(QStringLiteral("performance_hint"));
        QJniObject manager = activity.callObjectMethod("getSystemService",
                "(Ljava/lang/String;)Ljava/lang/Object;",
                name.object<jstring>());
        if (!manager.isValid()) {
            qWarning() << "Performance hint: no PerformanceHintManager on this device";
            return false;
        }
        {
            const jlong rate = manager.callMethod<jlong>("getPreferredUpdateRateNanos", "()J");
            qInfo() << "Performance hint: preferred update rate" << rate / 1e6 << "ms";
        }
        tids = collectTids();
        audioTid = mixxx::g_audioCallbackTid.load(std::memory_order_relaxed);
        QJniEnvironment env;
        jintArray array = env->NewIntArray(static_cast<jsize>(tids.size()));
        env->SetIntArrayRegion(array, 0, static_cast<jsize>(tids.size()), tids.data());
        session = manager.callObjectMethod("createHintSession",
                "([IJ)Landroid/os/PerformanceHintManager$Session;",
                array,
                static_cast<jlong>(targetNs));
        env->DeleteLocalRef(array);
        if (env.checkAndClearExceptions()) {
            qWarning() << "Performance hint: createHintSession threw an exception, tids" << tids;
        }
        sessionOk = session.isValid();
        if (!sessionOk) {
            qWarning() << "Performance hint: createHintSession returned null, tids" << tids;
        }
        return sessionOk;
    }

    void closeSession() {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (session.isValid()) {
            session.callMethod<void>("close", "()V");
        }
        session = QJniObject();
        sessionOk = false;
    }
#endif
};

namespace {
// Measurement tool (tests/perf/play_hitch.sh): with [Config],perf_trace = 1 the
// frames drawn in the 600 ms after a PLAY press of deck 1 are logged.
// Off by default, no cost when off.
struct PerfTrace {
    qint64 playNs = 0;
    std::vector<double> frames; // ms since the press
    std::atomic<bool> armed{false};
};
} // namespace

void QmlApplication::setupPerfTrace() {
    // Called by a singleShot 4 s after the start: if the app quits before, the
    // processEvents() of CoreServices::finalize() runs it with m_pCoreServices
    // (reset first) and m_pAppEngine already null
    if (!m_pCoreServices || !m_pAppEngine) {
        return;
    }
    if (!m_pCoreServices->getSettings()->getValue(
                ConfigKey(QStringLiteral("[Config]"), QStringLiteral("perf_trace")), false)) {
        return;
    }
    QQuickWindow* pWindow = nullptr;
    for (QObject* pRoot : m_pAppEngine->rootObjects()) {
        pWindow = qobject_cast<QQuickWindow*>(pRoot);
        if (pWindow) {
            break;
        }
    }
    if (!pWindow) {
        return;
    }
    auto* pTrace = new PerfTrace();
    auto* pPlay = new ControlProxy(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("play")), this);
    pPlay->connectValueChanged(
            this,
            [pTrace](double value) {
                if (value > 0 && !pTrace->armed.load()) {
                    pTrace->frames.clear();
                    pTrace->playNs = nowNs();
                    pTrace->armed.store(true);
                }
            },
            Qt::DirectConnection);
    connect(pWindow, &QQuickWindow::afterFrameEnd, this, [pTrace, this]() {
        if (!pTrace->armed.load()) {
            return;
        }
        const double ms = (nowNs() - pTrace->playNs) / 1e6;
        if (ms < 600) {
            pTrace->frames.push_back(ms);
            return;
        }
        pTrace->armed.store(false);
        QStringList list;
        double maxGap = 0, prev = 0;
        int slow = 0;
        for (double f : pTrace->frames) {
            maxGap = std::max(maxGap, f - prev);
            slow += (f - prev) > 20.0;
            prev = f;
            list << QString::number(f, 'f', 0);
        }
        qInfo().noquote() << "PERFTRACE perf=" << m_pPerformanceMode->get()
                          << "frames=" << pTrace->frames.size()
                          << "first_ms=" << (pTrace->frames.empty() ? -1 : pTrace->frames.front())
                          << "max_gap_ms=" << maxGap << "gaps_over_20ms=" << slow
                          << "at:" << list.join(',');
    }, Qt::DirectConnection);
    qInfo() << "perf_trace enabled";
}

void QmlApplication::applyPerformanceMode(bool on) {
    const auto pConfig = m_pCoreServices->getSettings();
    // Parameters (not saved as "on": only these are remembered)
    const ConfigKey refreshKey(QStringLiteral("[Config]"), QStringLiteral("performance_refresh_hz"));
    const ConfigKey hintKey(QStringLiteral("[Config]"), QStringLiteral("performance_hint_ms"));
    const double refreshHz = pConfig->getValue(refreshKey, 60.0);
    const double hintMs = pConfig->getValue(hintKey, 12.0);
    QmlWaveformDisplay::setPerformanceMode(on);
#ifdef __ANDROID__
    setWindowRefreshRate(on ? static_cast<float>(refreshHz) : 60.0f);
    if (on) {
        if (!m_pPerfHint) {
            m_pPerfHint = std::make_shared<PerfHint>();
        }
        // Shared: the render-thread lambdas keep it alive while a frame is in flight
        const std::shared_ptr<PerfHint> h = m_pPerfHint;
        h->targetNs = static_cast<qint64>(hintMs * 1e6);
        h->closeSession();
        if (hintMs > 0 && h->createSession()) {
            QQuickWindow* pWindow = nullptr;
            if (m_pAppEngine) {
                for (QObject* pRoot : m_pAppEngine->rootObjects()) {
                    pWindow = qobject_cast<QQuickWindow*>(pRoot);
                    if (pWindow) {
                        break;
                    }
                }
            }
            h->window = pWindow;
            if (pWindow) {
                h->beginConnection = connect(
                        pWindow, &QQuickWindow::beforeFrameBegin, this, [h]() {
                            h->frameStartNs.store(
                                    nowNs(),
                                    std::memory_order_relaxed);
                        }, Qt::DirectConnection);
                h->endConnection = connect(
                        pWindow, &QQuickWindow::afterFrameEnd, this, [h]() {
                            // Render thread: skip the frame if the GUI thread
                            // is opening/closing the session
                            std::unique_lock<std::mutex> lock(h->sessionMutex, std::try_to_lock);
                            if (!lock.owns_lock() || !h->sessionOk) {
                                return;
                            }
                            const qint64 start = h->frameStartNs.load(std::memory_order_relaxed);
                            const qint64 dur = nowNs() - start;
                            h->session.callMethod<void>("reportActualWorkDuration",
                                    "(J)V",
                                    static_cast<jlong>(std::max<qint64>(dur, 1000000)));
                        }, Qt::DirectConnection);
            }
            if (!h->pCheckTimer) {
                h->pCheckTimer = new QTimer(this);
                h->pCheckTimer->setInterval(3000);
                connect(h->pCheckTimer, &QTimer::timeout, this, [this, h]() {
                    // The audio thread appears later (or changes when the
                    // sound device is restarted): new session with its tid
                    const int audio = mixxx::g_audioCallbackTid.load(std::memory_order_relaxed);
                    if (audio > 0 && audio != h->audioTid) {
                        h->closeSession();
                        h->createSession();
                        qInfo() << "Performance mode: hint session updated, threads" << h->tids.size();
                    }
                });
            }
            h->pCheckTimer->start();
        }
        qInfo() << "Performance mode ON: waveforms redrawn every frame, refresh"
                << refreshHz << "Hz, hint session"
                << (h->sessionOk ? "active" : "not available") << "target" << hintMs
                << "ms, threads" << h->tids.size() << ", analysis and best-quality stems paused, screen stays on";
    } else if (m_pPerfHint) {
        const std::shared_ptr<PerfHint> h = m_pPerfHint;
        if (h->pCheckTimer) {
            h->pCheckTimer->stop();
        }
        disconnect(h->beginConnection);
        disconnect(h->endConnection);
        h->closeSession();
        qInfo() << "Performance mode OFF: normal redraw, 60 Hz, analysis and stems resumed";
    }
#else
    qInfo() << "Performance mode" << (on ? "ON" : "OFF") << "refresh" << refreshHz << "Hz";
#endif
}


namespace {

#ifdef __ANDROID__
/// Diagnostics: `setprop debug.mixxx.framemon 1` (root) makes the render
/// thread log, in logcat (tag mixxx-frames), every gap between two frames of
/// a continuous stream longer than 20 ms, and a summary every 3 s of
/// activity. Idle gaps (nothing to draw) are ignored. Cost when off: one
/// property read every 60 frames.
void installFrameMonitor(QQuickWindow* pWindow) {
    struct State {
        using Clock = std::chrono::steady_clock;
        Clock::time_point last;
        Clock::time_point frameBegin;
        Clock::time_point summaryStart;
        bool enabled = false;
        int sinceCheck = 0;
        int frames = 0;
        int over20 = 0;
        int over33 = 0;
        int over50 = 0;
        double worstMs = 0;
    };
    auto state = std::make_shared<State>();
    // Duration of each frame on the render thread (sync with the GUI thread
    // included): long ones are what the eye sees as a hitch
    QObject::connect(
            pWindow,
            &QQuickWindow::beforeFrameBegin,
            pWindow,
            [state]() { state->frameBegin = State::Clock::now(); },
            Qt::DirectConnection);
    // Direct: runs on the render thread, a queued call would measure the GUI thread
    QObject::connect(
            pWindow,
            &QQuickWindow::frameSwapped,
            pWindow,
            [state]() {
                const auto now = State::Clock::now();
                if (state->sinceCheck-- <= 0) {
                    state->sinceCheck = 60;
                    char value[PROP_VALUE_MAX] = {};
                    __system_property_get("debug.mixxx.framemon", value);
                    state->enabled = value[0] == '1';
                }
                if (!state->enabled) {
                    state->last = {};
                    return;
                }
                if (state->frameBegin.time_since_epoch().count() != 0) {
                    const double frameMs = std::chrono::duration<double, std::milli>(
                            now - state->frameBegin)
                                                   .count();
                    if (frameMs > 16.0) {
                        __android_log_print(ANDROID_LOG_WARN, "mixxx-frames", "frame took %.1f ms", frameMs);
                    }
                }
                if (state->last.time_since_epoch().count() != 0) {
                    const double gapMs =
                            std::chrono::duration<double, std::milli>(now - state->last).count();
                    if (gapMs < 250.0) {
                        if (state->frames == 0) {
                            state->summaryStart = now;
                        }
                        state->frames++;
                        if (gapMs > 20.0) {
                            state->over20++;
                            __android_log_print(ANDROID_LOG_WARN, "mixxx-frames", "gap %.1f ms", gapMs);
                        }
                        if (gapMs > 33.0) {
                            state->over33++;
                        }
                        if (gapMs > 50.0) {
                            state->over50++;
                        }
                        state->worstMs = std::max(state->worstMs, gapMs);
                    }
                }
                state->last = now;
                if (state->frames > 0 &&
                        std::chrono::duration<double>(now - state->summaryStart).count() >= 3.0) {
                    __android_log_print(ANDROID_LOG_WARN,
                            "mixxx-frames",
                            "SUMMARY frames=%d >20ms=%d >33ms=%d >50ms=%d worst=%.1f ms",
                            state->frames,
                            state->over20,
                            state->over33,
                            state->over50,
                            state->worstMs);
                    state->frames = state->over20 = state->over33 = state->over50 = 0;
                    state->worstMs = 0;
                }
            },
            Qt::DirectConnection);

    // GUI thread stalls: a 5 ms timer, running only while the property is on
    // (checked once a second), whose lateness is the time the thread was blocked
    auto* pStall = new QTimer(pWindow);
    pStall->setInterval(5);
    pStall->setTimerType(Qt::PreciseTimer);
    auto lastTick = std::make_shared<State::Clock::time_point>();
    QObject::connect(pStall, &QTimer::timeout, pWindow, [lastTick]() {
        const auto now = State::Clock::now();
        if (lastTick->time_since_epoch().count() != 0) {
            const double lateMs =
                    std::chrono::duration<double, std::milli>(now - *lastTick).count() - 5.0;
            if (lateMs > 12.0) {
                __android_log_print(ANDROID_LOG_WARN, "mixxx-frames", "GUI thread blocked %.1f ms", lateMs);
            }
        }
        *lastTick = now;
    });
    auto* pPoll = new QTimer(pWindow);
    pPoll->setInterval(1000);
    QObject::connect(pPoll, &QTimer::timeout, pWindow, [pStall, lastTick]() {
        char value[PROP_VALUE_MAX] = {};
        __system_property_get("debug.mixxx.framemon", value);
        const bool on = value[0] == '1';
        if (on && !pStall->isActive()) {
            *lastTick = {};
            pStall->start();
        } else if (!on && pStall->isActive()) {
            pStall->stop();
        }
    });
    pPoll->start();
}
#endif

} // namespace

QmlApplication::QmlApplication(
        QApplication* app,
        const CmdlineArgs& args)
        : m_pCoreServices(std::make_unique<mixxx::CoreServices>(args, app)),
          m_visualsManager(std::make_unique<VisualsManager>()),
          m_mainFilePath(m_pCoreServices->getSettings()->getResourcePath() + kMainQmlFileName),
          m_pAppEngine(nullptr),
          m_autoReload() {
    QQuickStyle::setStyle("Basic");

    m_pCoreServices->initialize(app);

    QString configVersion = m_pCoreServices->getSettings()->getValue(
            ConfigKey("[Config]", "Version"), "");
    if (configVersion == VersionStore::FUTURE_UNSTABLE) {
        qDebug() << "Generating a new user profile for safe testing with unstable code";
    } else if (CmdlineArgs::Instance().isAwareOfRisk()) {
        qCritical() << "Existing user profile detected from" << configVersion
                    << "but you said you wanted to play with fire!";
        m_pCoreServices->getSettings()->setValue(
                ConfigKey("[Config]", "did_run_with_unstable"), true);
    } else {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle(tr("Existing user profile detected"));
        msgBox.setText(
                tr("Trying to run Mixxx 3.0 with an existing %0 user profile! "
                   "<br><br>There is <b>serious risks</b> of data loss and "
                   "corruption.<br>We recommend using a test profile folder "
                   "with the '--settings-path' argument. <br><br>If you want "
                   "to continue at your own risk, run Mixxx with the argument "
                   "'--allow-dangerous-data-corruption-risk'.")
                        .arg(configVersion));

        QPushButton* continueButton =
                msgBox.addButton(tr("Ok"), QMessageBox::ActionRole);
        msgBox.exec();
        m_pCoreServices.reset();
        exit(-1);
    }

    auto pSoundManager = m_pCoreServices->getSoundManager();
    SoundDeviceStatus result = pSoundManager->setupDevices();
#ifdef __ANDROID__
    if (result != SoundDeviceStatus::Ok) {
        // Typical case: the saved config uses a USB controller (e.g. the
        // DDJ-FLX4) that is not plugged in. Instead of quitting, fall back to
        // the default devices: a 4 channel USB device if present, otherwise the
        // phone output. The fallback is not written to disk, so the saved
        // config is used again on the next start with the controller attached.
        qWarning() << "Configured sound devices unavailable:"
                   << pSoundManager->getLastErrorMessage(result)
                   << "- falling back to the default devices";
        m_pSavedSoundConfig = std::make_unique<SoundManagerConfig>(pSoundManager->getConfig());
        m_soundFallback = true;
        SoundManagerConfig fallbackConfig = *m_pSavedSoundConfig;
        fallbackConfig.loadDefaults(pSoundManager.get(), SoundManagerConfig::DEVICES);
        const bool persist = false;
        result = pSoundManager->setConfig(fallbackConfig, persist);
        if (result != SoundDeviceStatus::Ok) {
            // Keep running without audio rather than closing the app: the
            // devices can still be chosen in the sound preferences.
            qCritical() << "Error setting up the default sound devices:"
                        << pSoundManager->getLastErrorMessage(result);
        }
    }
#else
    if (result != SoundDeviceStatus::Ok) {
        const int reInt = static_cast<int>(result);
        qCritical() << "Error setting up sound devices:" << reInt;
        exit(reInt);
    }
#endif

    // FIXME: DlgPreferences has some initialization logic that must be executed
    // before the GUI is shown, at least for the effects system.
    std::shared_ptr<QDialog> pDlgPreferences = m_pCoreServices->makeDlgPreferences();
    // Without this, QApplication will quit when the last QWidget QWindow is
    // closed because it does not take into account the window created by
    // the QQmlApplicationEngine.
    pDlgPreferences->setAttribute(Qt::WA_QuitOnClose, false);

    // Since DlgPreferences is only meant to be used in the main QML engine, it
    // follows a strict singleton pattern design
    QmlDlgPreferencesProxy::s_pInstance =
            std::make_unique<QmlDlgPreferencesProxy>(pDlgPreferences, this);

    // The QML skin always shows 4 decks and 8 samplers. Create them before
    // the QML is loaded: components look up their player only once, so a
    // deck created later would stay empty ("-100.00", null player errors).
    // Controller mappings are set up later and may use more samplers: the
    // DDJ-FLX4 one drives the LEDs of [Sampler1]-[Sampler16]. Without them a
    // fresh install (no samplers.xml yet) shows a modal "invalid
    // MixxxControls" warning at every start, which also takes the keyboard.
    {
        auto pPlayerManager = m_pCoreServices->getPlayerManager();
        while (PlayerManager::numDecks() < 4) {
            pPlayerManager->addDeck();
        }
        while (PlayerManager::numSamplers() < 16) {
            pPlayerManager->addSampler();
        }
    }

    // A fresh install starts with empty effect units: the DDJ-FLX4 BEAT FX
    // (unit 1) and PAD FX (unit 2 = deck 1, unit 3 = deck 2) did nothing.
    // Fill only units that are completely empty, so a choice made by the
    // user is kept.
    {
        auto pEffectsManager = m_pCoreServices->getEffectsManager();
        const auto fillUnit = [&pEffectsManager](int unitIndex, const QStringList& effectIds) {
            const EffectChainPointer pChain = pEffectsManager->getStandardEffectChain(unitIndex);
            if (!pChain) {
                return;
            }
            const QList<EffectSlotPointer>& effectSlots = pChain->getEffectSlots();
            for (const EffectSlotPointer& pSlot : effectSlots) {
                if (pSlot->isLoaded()) {
                    return;
                }
            }
            for (int i = 0; i < effectIds.size() && i < effectSlots.size(); ++i) {
                const EffectManifestPointer pManifest =
                        pEffectsManager->getBackendManager()->getManifestFromUniqueId(
                                effectIds.at(i));
                if (pManifest) {
                    effectSlots.at(i)->loadEffectWithDefaults(pManifest);
                    // The manifests have a meta default of 0, where Echo,
                    // Reverb and Flanger are silent
                    ControlObject::set(ConfigKey(effectSlots.at(i)->getGroup(),
                                               QStringLiteral("meta")),
                            0.5);
                }
            }
            qInfo() << "Effect unit" << unitIndex + 1 << "was empty, loaded" << effectIds;
        };
        const QString echo = QStringLiteral("org.mixxx.effects.echo");
        const QString reverb = QStringLiteral("org.mixxx.effects.reverb");
        const QString flanger = QStringLiteral("org.mixxx.effects.flanger");
        const QString bitcrusher = QStringLiteral("org.mixxx.effects.bitcrusher");
        // Some built-in effects are too gentle for BEAT FX with their
        // defaults: whenever a slot of effect units 1-3 loads one of them,
        // set DJ-style values (knob parameters in manifest order).
        //  - Flanger (speed, width, manual, regen, mix): a sweep every 4
        //    beats, full width, feedback 0.9 (jet)
        //  - Compressor (threshold, ratio, knee, attack, release, level; not
        //    linked to the meta knob): pumping -35 dB 20:1, 10 ms attack to
        //    let the kicks through, 150 ms release, soft knee, +6 dB
        for (int unitIndex = 0; unitIndex < 3; ++unitIndex) {
            const EffectChainPointer pChain = pEffectsManager->getStandardEffectChain(unitIndex);
            if (!pChain) {
                continue;
            }
            for (const EffectSlotPointer& pSlot : pChain->getEffectSlots()) {
                EffectSlot* pRawSlot = pSlot.data();
                const auto tune = [pRawSlot]() {
                    const EffectManifestPointer pManifest = pRawSlot->getManifest();
                    if (!pManifest) {
                        return;
                    }
                    const QString& group = pRawSlot->getGroup();
                    const auto setValue = [&group](int knob, double value) {
                        ControlObject::set(ConfigKey(group,
                                                   QStringLiteral("parameter") +
                                                           QString::number(knob)),
                                value);
                    };
                    const auto setNormalized = [&group](int knob, double parameter) {
                        ControlObject* pControl = ControlObject::getControl(ConfigKey(
                                group, QStringLiteral("parameter") + QString::number(knob)));
                        if (pControl) {
                            pControl->setParameter(parameter);
                        }
                    };
                    if (pManifest->id() == QStringLiteral("org.mixxx.effects.flanger")) {
                        setValue(1, 4.0);
                        setNormalized(2, 1.0);
                        setValue(4, 0.9);
                    } else if (pManifest->id() == QStringLiteral("org.mixxx.effects.compressor")) {
                        setValue(1, -35.0);
                        setValue(2, 20.0);
                        setValue(3, 12.0);
                        setValue(4, 10.0);
                        setValue(5, 150.0);
                        setValue(6, 6.0);
                    }
                };
                connect(pRawSlot, &EffectSlot::effectChanged, this, tune);
                tune();
            }
        }
        // BEAT FX SELECT (and the effect menus) cycle through the visible
        // effects: only the four that work as DJ effects on the FLX4
        {
            const VisibleEffectsListPointer pVisible = pEffectsManager->getVisibleEffectsList();
            const auto pBackends = pEffectsManager->getBackendManager();
            if (pVisible && pBackends) {
                QList<EffectManifestPointer> list;
                for (const QString& id : {echo, reverb, flanger, bitcrusher}) {
                    const EffectManifestPointer pManifest =
                            pBackends->getManifestFromUniqueId(id);
                    if (pManifest) {
                        list.append(pManifest);
                    }
                }
                if (list.size() == 4 && list != pVisible->getList()) {
                    pVisible->setList(list);
                    qInfo() << "Visible effects: Echo, Reverb, Flanger, Bitcrusher";
                }
            }
        }
        fillUnit(0, {echo, reverb, flanger});
        fillUnit(1, {echo, reverb, flanger, bitcrusher});
        fillUnit(2, {echo, reverb, flanger, bitcrusher});
    }

    // Skin state shared by the QML toolbar and controller mappings.
    m_pShowLibrary = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("show_library")),
            false,
            1.0);
    m_pShowLibrary->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pStemMode = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("stem_mode")));
    // Pad mode per deck: -1 (the default) = unknown, the pad panels stay hidden
    for (int deck = 0; deck < 4; ++deck) {
        m_pPadMode[deck] = std::make_unique<ControlObject>(
                ConfigKey(PlayerManager::groupForDeck(deck), QStringLiteral("pad_mode")),
                true,
                false,
                false,
                -1.0);
    }
    m_pPadBeatjumpUnit = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("pad_beatjump_unit")),
            true,
            false,
            false,
            1.0);
    // Performance mode: always starts off (the control is created by
    // CoreServices at 0 and never saved)
    m_pPerformanceMode = std::make_unique<ControlProxy>(
            ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("performance_mode")), this);
    m_pPerformanceMode->connectValueChanged(this, [this](double value) {
        applyPerformanceMode(value > 0);
    });

    const QStringList visualGroups =
            m_pCoreServices->getPlayerManager()->getVisualPlayerGroups();
    for (const QString& group : visualGroups) {
        m_visualsManager->addDeck(group);
    }

    m_pCoreServices->getPlayerManager()->connect(
            m_pCoreServices->getPlayerManager().get(),
            &PlayerManager::numberOfDecksChanged,
            this,
            [this](int decks) {
                for (int i = 0; i < decks; ++i) {
                    QString group = PlayerManager::groupForDeck(i);
                    m_visualsManager->addDeckIfNotExist(group);
                }
            });
    loadQml(m_mainFilePath);
    QTimer::singleShot(4000, this, [this]() { setupPerfTrace(); });

#ifdef __ANDROID__
    if (!m_pAppEngine->rootObjects().isEmpty()) {
        if (auto* pMainWindow = qobject_cast<QQuickWindow*>(m_pAppEngine->rootObjects().first())) {
            installFrameMonitor(pMainWindow);
        }
    }
    // Many phones run at 90/120 Hz. Rendering the waveforms within 8 ms per
    // frame is not realistic, so frames are dropped and the UI stutters.
    // Ask Android for 60 Hz for this window: 16.7 ms per frame.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            return;
        }
        QJniObject window = activity.callObjectMethod(
                "getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) {
            return;
        }
        QJniObject params = window.callObjectMethod(
                "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
        if (!params.isValid()) {
            return;
        }
        params.setField<jfloat>("preferredRefreshRate", 60.0f);
        // Use the whole screen: also the side with the camera hole
        // (LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS = 3, API 30); with the
        // default mode Android kept a black band along that side
        params.setField<jint>("layoutInDisplayCutoutMode", 3);
        window.callMethod<void>("setAttributes",
                "(Landroid/view/WindowManager$LayoutParams;)V",
                params.object());
        qInfo() << "Requested 60 Hz refresh rate for the main window";
        // A DJ app must not let the screen turn off (and lock) while it is
        // in the foreground. WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON:
        // only active while this window is visible, no permission needed.
        constexpr jint kFlagKeepScreenOn = 0x00000080;
        window.callMethod<void>("addFlags", "(I)V", kFlagKeepScreenOn);
        // Hide the status and navigation bars (the navigation bar took a
        // band at the bottom); a swipe from the edge shows them for a
        // moment (BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE = 2)
        window.callMethod<void>("setDecorFitsSystemWindows", "(Z)V", jboolean(false));
        QJniObject insetsController = window.callObjectMethod(
                "getInsetsController", "()Landroid/view/WindowInsetsController;");
        if (insetsController.isValid()) {
            const jint systemBars = QJniObject::callStaticMethod<jint>(
                    "android/view/WindowInsets$Type", "systemBars", "()I");
            insetsController.callMethod<void>("hide", "(I)V", systemBars);
            insetsController.callMethod<void>("setSystemBarsBehavior", "(I)V", jint(2));
        }
    });

    // "Do Not Disturb" while Mixxx runs ([Config],android_dnd, default on):
    // notification sounds must not end up in the mix.
    const bool dndEnabled = m_pCoreServices->getSettings()->getValue(
            ConfigKey(QStringLiteral("[Config]"), QStringLiteral("android_dnd")),
            true);
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid()) {
        QJniObject::callStaticMethod<void>("org/mixxx/DoNotDisturb",
                "start",
                "(Landroid/app/Activity;Z)V",
                activity.object(),
                static_cast<jboolean>(dndEnabled));
        QJniObject::callStaticMethod<void>("org/mixxx/PlaybackGuard",
                "start",
                "(Landroid/app/Activity;)V",
                activity.object());
    }
#endif

    m_pCoreServices->getControllerManager()->setUpDevices();
#ifdef __ANDROID__
    // The controller's USB audio appears together with its MIDI port (a bit
    // later): retry the saved sound configuration after a USB device change
    connect(m_pCoreServices->getControllerManager().get(),
            &ControllerManager::devicesChanged,
            this,
            [this]() {
                if (!m_soundFallback) {
                    return;
                }
                QTimer::singleShot(1000, this, [this]() { tryRestoreSoundConfig(); });
                QTimer::singleShot(4000, this, [this]() { tryRestoreSoundConfig(); });
            });
#endif

    // Keyboard shortcuts (USB/Bluetooth keyboards, `adb shell input keyevent`).
    app->installEventFilter(this);

    connect(&m_autoReload,
            &QmlAutoReload::triggered,
            this,
            [this]() {
                loadQml(m_mainFilePath);
            });
}

bool QmlApplication::eventFilter(QObject* pObj, QEvent* pEvent) {
    const QEvent::Type type = pEvent->type();
    if (m_pCoreServices &&
            (type == QEvent::KeyPress || type == QEvent::KeyRelease ||
                    type == QEvent::FocusOut)) {
        // Only the window itself: the same key event is delivered again to
        // the focused QQuickItem and must not be counted twice.
        // qobject_cast, not dynamic_cast: the latter fails across Qt
        // libraries on Android (see CODICE.md).
        auto* pWindow = qobject_cast<QQuickWindow*>(pObj);
        if (pWindow) {
            const QQuickItem* pFocus = pWindow->activeFocusItem();
            const bool typingText = pFocus &&
                    (pFocus->inherits("QQuickTextInput") ||
                            pFocus->inherits("QQuickTextEdit"));
            const auto pKeyboard = m_pCoreServices->getKeyboardEventFilter();
            if (!typingText && pKeyboard && pKeyboard->eventFilter(pObj, pEvent)) {
                return true;
            }
        }
    }
    return QObject::eventFilter(pObj, pEvent);
}

QmlApplication::~QmlApplication() {
#ifdef __ANDROID__
    if (m_pPerfHint) {
        disconnect(m_pPerfHint->beginConnection);
        disconnect(m_pPerfHint->endConnection);
        if (m_pPerfHint->pCheckTimer) {
            // It would reopen the session during finalize()'s processEvents()
            m_pPerfHint->pCheckTimer->stop();
        }
        m_pPerfHint->closeSession();
    }
#endif
    QCoreApplication::instance()->removeEventFilter(this);
#ifdef __ANDROID__
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>("org/mixxx/PlaybackGuard",
                "stop",
                "(Landroid/content/Context;)V",
                context.object());
        QJniObject::callStaticMethod<void>("org/mixxx/DoNotDisturb",
                "stop",
                "(Landroid/content/Context;)V",
                context.object());
    }
#endif
    // Delete all the QML singletons in order to prevent leak detection in CoreService
    QmlDlgPreferencesProxy::s_pInstance.reset();
    m_visualsManager.reset();
    m_pAppEngine.reset();
    m_pStemMode.reset();
    m_pShowLibrary.reset();
    m_pCoreServices.reset();
}

void QmlApplication::loadQml(const QString& path) {
    // QQmlApplicationEngine::load creates a new window but also leaves the old one,
    // so it is necessary to destroy the old QQmlApplicationEngine and create a new one.
    m_pAppEngine = std::make_unique<QQmlApplicationEngine>();

    m_autoReload.clear();
    m_pAppEngine->addUrlInterceptor(&m_autoReload);
    m_pAppEngine->addImportPath(QStringLiteral(":/mixxx.org/imports"));

    // No memory leak here, the QQmlEngine takes ownership of the provider
    QQuickAsyncImageProvider* pImageProvider = new AsyncImageProvider(
            m_pCoreServices->getTrackCollectionManager());
    m_pAppEngine->addImageProvider(AsyncImageProvider::kProviderName, pImageProvider);

    m_pAppEngine->load(path);
    if (m_pAppEngine->rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML file" << path;
    }
}

void QmlApplication::tryRestoreSoundConfig() {
    // Fired by a singleShot 1 s / 4 s after a USB change: it can run while
    // CoreServices is being destroyed (m_pCoreServices already null)
    if (!m_soundFallback || !m_pSavedSoundConfig || !m_pCoreServices) {
        return;
    }
    auto pSoundManager = m_pCoreServices->getSoundManager();
    if (!pSoundManager) {
        return;
    }
    // The device list was made before the device was plugged in
    pSoundManager->clearAndQueryDevices();
    const bool persist = false;
    if (pSoundManager->setConfig(*m_pSavedSoundConfig, persist) == SoundDeviceStatus::Ok) {
        m_soundFallback = false;
        qInfo() << "Configured sound devices available again: using them";
        return;
    }
    qInfo() << "Configured sound devices still unavailable";
    SoundManagerConfig fallbackConfig = *m_pSavedSoundConfig;
    fallbackConfig.loadDefaults(pSoundManager.get(), SoundManagerConfig::DEVICES);
    pSoundManager->setConfig(fallbackConfig, persist);
}

} // namespace qml
} // namespace mixxx
