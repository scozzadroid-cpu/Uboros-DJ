#include "qmlapplication.h"

#include <qtextdocument.h>

#include <QQmlEngineExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#ifdef __ANDROID__
#include <QCoreApplication>
#include <QJniObject>
#endif

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "controllers/controllermanager.h"
#include "controllers/keyboard/keyboardeventfilter.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/effectchain.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "mixer/playermanager.h"
#include "moc_qmlapplication.cpp"
#include "preferences/configobject.h"
#include "qml/asyncimageprovider.h"
#include "qml/qmldlgpreferencesproxy.h"
#include "soundio/soundmanager.h"
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
        SoundManagerConfig fallbackConfig = pSoundManager->getConfig();
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
                }
            }
            qInfo() << "Effect unit" << unitIndex + 1 << "was empty, loaded" << effectIds;
        };
        const QString echo = QStringLiteral("org.mixxx.effects.echo");
        const QString reverb = QStringLiteral("org.mixxx.effects.reverb");
        const QString flanger = QStringLiteral("org.mixxx.effects.flanger");
        const QString bitcrusher = QStringLiteral("org.mixxx.effects.bitcrusher");
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

#ifdef __ANDROID__
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
        window.callMethod<void>("setAttributes",
                "(Landroid/view/WindowManager$LayoutParams;)V",
                params.object());
        qInfo() << "Requested 60 Hz refresh rate for the main window";
        // A DJ app must not let the screen turn off (and lock) while it is
        // in the foreground. WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON:
        // only active while this window is visible, no permission needed.
        constexpr jint kFlagKeepScreenOn = 0x00000080;
        window.callMethod<void>("addFlags", "(I)V", kFlagKeepScreenOn);
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
    }
#endif

    m_pCoreServices->getControllerManager()->setUpDevices();

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
    QCoreApplication::instance()->removeEventFilter(this);
#ifdef __ANDROID__
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
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

} // namespace qml
} // namespace mixxx
