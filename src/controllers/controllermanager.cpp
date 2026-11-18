#include "controllers/controllermanager.h"

#include <QSet>
#include <QThread>

#include "controllers/controller.h"
#include "controllers/controllerlearningeventfilter.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "moc_controllermanager.cpp"
#include "preferences/usersettings.h"
#include "util/cmdlineargs.h"
#include "util/compatibility/qmutex.h"
#include "util/duration.h"
#include "util/thread_affinity.h"
#include "util/time.h"

#ifdef __PORTMIDI__
#include "controllers/midi/portmidienumerator.h"
#endif

#ifdef __ANDROID_MIDI__
#include "controllers/midi/androidmidicontroller.h"
#include "controllers/midi/androidmidienumerator.h"
#endif

#ifdef __HSS1394__
#include "controllers/midi/hss1394enumerator.h"
#endif

#ifdef __HID__
#include "controllers/hid/hidenumerator.h"
#endif

#ifdef __BULK__
#include "controllers/bulk/bulkenumerator.h"
#endif

// http://developer.qt.nokia.com/wiki/Threads_Events_QObjects

// Poll every 1ms (where possible) for good controller response
#ifdef __LINUX__
// Many Linux distros ship with the system tick set to 250Hz so 1ms timer
// reportedly causes CPU hosage. See https://github.com/mixxxdj/mixxx/issues/6383
// rryan 6/2012
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(5);
#else
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(1);
#endif

namespace {
#ifdef __ANDROID__
// Poll interval while no MIDI message arrived for kPollIdleAfter
constexpr int kIdlePollIntervalMs = 4;
const mixxx::Duration kPollIdleAfter = mixxx::Duration::fromMillis(2000);
#endif
/// Strip slashes and spaces from device name, so that it can be used as config
/// key or a filename.
QString sanitizeDeviceName(QString name) {
    return name.replace(" ", "_").replace("/", "_").replace("\\", "_");
}

QFileInfo findMappingFile(const QString& pathOrFilename, const QStringList& paths) {
    QFileInfo fileInfo(pathOrFilename);
    if (fileInfo.isAbsolute()) {
        return fileInfo;
    }

    for (const QString& path : paths) {
        fileInfo = QFileInfo(QDir(path).absoluteFilePath(pathOrFilename));
        if (fileInfo.exists()) {
            return fileInfo;
        }
    }

    return QFileInfo();
}

// Legacy code referred to mappings as "presets", so "[ControllerPreset]" must be
// kept for backwards compatibility.
const QString kSettingsGroup = QLatin1String("[ControllerPreset]");

} // anonymous namespace

QString firstAvailableFilename(QSet<QString>& filenames,
        const QString& originalFilename) {
    QString filename = originalFilename;
    int i = 1;
    while (filenames.contains(filename)) {
        i++;
        filename = QString("%1--%2").arg(originalFilename, QString::number(i));
    }
    filenames.insert(filename);
    return filename;
}

bool controllerCompare(Controller *a,Controller *b) {
    return a->getName() < b->getName();
}

ControllerManager::ControllerManager(UserSettingsPointer pConfig)
        : QObject(),
          m_pConfig(pConfig),
          // WARNING: Do not parent m_pControllerLearningEventFilter to
          // ControllerManager because the CM is moved to its own thread and runs
          // its own event loop.
          m_pControllerLearningEventFilter(new ControllerLearningEventFilter()),
          m_pollTimer(this),
          m_skipPoll(false) {
    qRegisterMetaType<std::shared_ptr<LegacyControllerMapping>>(
            "std::shared_ptr<LegacyControllerMapping>");

    // Create controller mapping paths in the user's home directory.
    QString userMappings = userMappingsPath(m_pConfig);
    if (!QDir(userMappings).exists()) {
        qDebug() << "Creating user controller mappings directory:" << userMappings;
        QDir().mkpath(userMappings);
    }

    m_pollTimer.setInterval(kPollInterval.toIntegerMillis());
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllerManager::slotPollDevices);

    m_pThread = new QThread;
    m_pThread->setObjectName("Controller");

    // Moves all children (including the poll timer) to m_pThread
    moveToThread(m_pThread);

    // Controller processing needs to be prioritized since it can affect the
    // audio directly, like when scratching
    m_pThread->start(QThread::HighPriority);

    connect(this, &ControllerManager::requestInitialize, this, &ControllerManager::slotInitialize);
    connect(this,
            &ControllerManager::requestSetUpDevices,
            this,
            &ControllerManager::slotSetUpDevices);
    connect(this, &ControllerManager::requestShutdown, this, &ControllerManager::slotShutdown);

    // Signal that we should run slotInitialize once our event loop has started
    // up.
    emit requestInitialize(); // clazy:exclude=incorrect-emit
}

ControllerManager::~ControllerManager() {
    emit requestShutdown();
    m_pThread->wait();
    delete m_pThread;
    delete m_pControllerLearningEventFilter;
}

ControllerLearningEventFilter* ControllerManager::getControllerLearningEventFilter() const {
    return m_pControllerLearningEventFilter;
}

void ControllerManager::slotInitialize() {
    qDebug() << "ControllerManager:slotInitialize";

    // Initialize mapping info parsers. This object is only for use in the main
    // thread. Do not touch it from within ControllerManager.
    m_pMainThreadUserMappingEnumerator = QSharedPointer<MappingInfoEnumerator>(
            new MappingInfoEnumerator(userMappingsPath(m_pConfig)));
    m_pMainThreadSystemMappingEnumerator = QSharedPointer<MappingInfoEnumerator>(
            new MappingInfoEnumerator(resourceMappingsPath(m_pConfig)));

    // Instantiate all enumerators. Enumerators can take a long time to
    // construct since they interact with host MIDI APIs.
#ifdef __PORTMIDI__
    m_enumerators.append(new PortMidiEnumerator(m_pConfig));
#endif
#ifdef __ANDROID_MIDI__
    m_pAndroidMidiEnumerator = new AndroidMidiEnumerator();
    m_enumerators.append(m_pAndroidMidiEnumerator);
    // Registers the MidiManager device callback before the first scan.
    m_midiDeviceGeneration = AndroidMidiEnumerator::deviceGeneration();
    m_pHotplugTimer = new QTimer(this);
    m_pHotplugTimer->setInterval(1500);
    connect(m_pHotplugTimer,
            &QTimer::timeout,
            this,
            &ControllerManager::slotCheckMidiHotplug);
    m_pHotplugTimer->start();
#endif
#ifdef __HSS1394__
    m_enumerators.append(new Hss1394Enumerator());
#endif
#ifdef __BULK__
    m_enumerators.append(new BulkEnumerator());
#endif
#ifdef __HID__
    m_enumerators.append(new HidEnumerator());
#endif
    emit initialized();
}

void ControllerManager::slotShutdown() {
    stopPolling();
    if (m_pHotplugTimer) {
        m_pHotplugTimer->stop();
    }
    m_pAndroidMidiEnumerator = nullptr;

    // Clear m_enumerators before deleting the enumerators to prevent other code
    // paths from accessing them.
    auto locker = lockMutex(&m_mutex);
    QList<ControllerEnumerator*> enumerators = m_enumerators;
    m_enumerators.clear();
    locker.unlock();

    // Delete enumerators and they'll delete their Devices
    for (ControllerEnumerator* pEnumerator : enumerators) {
        delete pEnumerator;
    }

    // Stop the processor after the enumerators since the engines live in it
    m_pThread->quit();
}

void ControllerManager::updateControllerList() {
    // NOTE: Currently this function is only called on startup. If hotplug is added, changes to the
    // controller list must be synchronized with dlgprefcontrollers to avoid dangling connections
    // and possible crashes.
    auto locker = lockMutex(&m_mutex);
    if (m_enumerators.isEmpty()) {
        qWarning() << "updateControllerList called but no enumerators have been added!";
        return;
    }
    QList<ControllerEnumerator*> enumerators = m_enumerators;
    locker.unlock();

    QList<Controller*> newDeviceList;
    for (ControllerEnumerator* pEnumerator : enumerators) {
        newDeviceList.append(pEnumerator->queryDevices());
    }

    locker.relock();
    if (newDeviceList != m_controllers) {
        m_controllers = newDeviceList;
        locker.unlock();
        emit devicesChanged();
    }
}

QList<Controller*> ControllerManager::getControllers() const {
    const auto locker = lockMutex(&m_mutex);
    return m_controllers;
}

QList<Controller*> ControllerManager::getControllerList(bool bOutputDevices, bool bInputDevices) {
    qDebug() << "ControllerManager::getControllerList";

    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    // Create a list of controllers filtered to match the given input/output
    // options.
    QList<Controller*> filteredDeviceList;

    for (Controller* device : controllers) {
        if ((bOutputDevices == device->isOutputDevice()) ||
            (bInputDevices == device->isInputDevice())) {
            filteredDeviceList.push_back(device);
        }
    }
    return filteredDeviceList;
}

QString ControllerManager::getConfiguredMappingFileForDevice(const QString& name) {
    return m_pConfig->getValueString(ConfigKey(kSettingsGroup, sanitizeDeviceName(name)));
}

void ControllerManager::slotSetUpDevices() {
    qDebug() << "ControllerManager: Setting up devices";

    updateControllerList();
    const QList<Controller*> deviceList = getControllerList(false, true);
    QStringList mappingPaths(getMappingPaths(m_pConfig));

    for (Controller* pController : deviceList) {
        if (pController->isOpen()) {
            pController->close();
        }
        setUpController(pController, mappingPaths);
    }

    pollIfAnyControllersOpen();
}

void ControllerManager::setUpController(
        Controller* pController, const QStringList& mappingPaths) {
    QString name = pController->getName();
    autoConfigureController(pController);

    // The filename for this device name.
    QString deviceName = sanitizeDeviceName(name);

    // Check if device is enabled
    if (!m_pConfig->getValue(ConfigKey("[Controller]", deviceName), 0)) {
        return;
    }

    // Check if device has a configured mapping
    QString mappingFilePath = getConfiguredMappingFileForDevice(deviceName);
    if (mappingFilePath.isEmpty()) {
        return;
    }

    qDebug() << "Searching for controller mapping" << mappingFilePath
             << "in paths:" << mappingPaths.join(",");
    QFileInfo mappingFile = findMappingFile(mappingFilePath, mappingPaths);
    if (!mappingFile.exists()) {
        qDebug() << "Could not find" << mappingFilePath << "in any mapping path.";
        return;
    }

    std::shared_ptr<LegacyControllerMapping> pMapping =
            LegacyControllerMappingFileHandler::loadMapping(
                    mappingFile, resourceMappingsPath(m_pConfig));

    if (!pMapping) {
        return;
    }
    pMapping->loadSettings(m_pConfig, pController->getName());

    // This runs on the main thread but LegacyControllerMapping is not thread safe, so clone it.
    pController->setMapping(std::move(pMapping));

    // If we are in safe mode, skip opening controllers.
    if (CmdlineArgs::Instance().getSafeMode()) {
        qDebug() << "We are in safe mode -- skipping opening controller.";
        return;
    }

    qDebug() << "Opening controller:" << name;

    int value = pController->open(m_pConfig->getResourcePath());
    if (value != 0) {
        qWarning() << "There was a problem opening" << name;
        return;
    }
}

void ControllerManager::autoConfigureController(Controller* pController) {
#ifdef __ANDROID_MIDI__
    const QString deviceName = sanitizeDeviceName(pController->getName());
    if (!m_pConfig->getValueString(ConfigKey(kSettingsGroup, deviceName)).isEmpty()) {
        return; // the user already chose a mapping for this device
    }
    if (pController->getPhysicalTransportProtocol() !=
            PhysicalTransportProtocol::USB) {
        qDebug() << "Not auto-configuring non-USB controller" << pController->getName();
        return;
    }
    // Last word of the device name, e.g. "DDJ-FLX4" for
    // "AlphaTheta Corporation DDJ-FLX4".
    const QString product = pController->getName()
                                    .section(QChar(' '), -1, -1, QString::SectionSkipEmpty)
                                    .toLower();
    if (product.size() < 4) {
        return;
    }
    MappingInfoEnumerator enumerator(resourceMappingsPath(m_pConfig));
    enumerator.loadSupportedMappings();
    const QList<MappingInfo> mappings =
            enumerator.getMappingsByExtension(MIDI_MAPPING_EXTENSION);
    QString matchPath;
    int matches = 0;
    for (const MappingInfo& mapping : mappings) {
        const QString mappingProduct = mapping.getName()
                                               .section(QChar(' '), -1, -1, QString::SectionSkipEmpty)
                                               .toLower();
        // File names look like "Pioneer-DDJ-FLX4.midi.xml"
        const QString fileName = QFileInfo(mapping.getPath()).fileName().toLower();
        if (mappingProduct == product ||
                fileName.endsWith(QStringLiteral("-") + product + MIDI_MAPPING_EXTENSION) ||
                fileName == product + MIDI_MAPPING_EXTENSION) {
            matchPath = mapping.getPath();
            ++matches;
        }
    }
    if (matches > 1) {
        // Ambiguous last word ("Control", "Advanced"...): try the full name
        const QString fullName = pController->getName().toLower();
        QString exactPath;
        int exactMatches = 0;
        for (const MappingInfo& mapping : mappings) {
            const QString mappingName = mapping.getName().toLower();
            if (fullName.contains(mappingName) || mappingName.contains(fullName)) {
                exactPath = mapping.getPath();
                ++exactMatches;
            }
        }
        if (exactMatches == 1) {
            matchPath = exactPath;
            matches = 1;
        }
    }
    if (matches != 1) {
        qDebug() << "No unique mapping for" << pController->getName() << "found"
                 << matches;
        return;
    }
    qInfo() << "Auto-configuring" << pController->getName() << "with mapping"
            << matchPath;
    m_pConfig->set(ConfigKey(kSettingsGroup, deviceName), matchPath);
    m_pConfig->setValue(ConfigKey("[Controller]", deviceName), 1);
#else
    Q_UNUSED(pController);
#endif
}

void ControllerManager::slotCheckMidiHotplug() {
#ifdef __ANDROID_MIDI__
    if (!m_pAndroidMidiEnumerator) {
        return;
    }
    const int generation = AndroidMidiEnumerator::deviceGeneration();
    if (generation == m_midiDeviceGeneration) {
        return;
    }
    m_midiDeviceGeneration = generation;
    qInfo() << "Android MIDI devices changed, rescanning";

    auto locker = lockMutex(&m_mutex);
    QList<Controller*> oldControllers = m_controllers;
    locker.unlock();

    const QList<Controller*> midiControllers = m_pAndroidMidiEnumerator->queryDevices();

    QList<Controller*> newControllers;
    for (Controller* pController : std::as_const(oldControllers)) {
        if (qobject_cast<AndroidMidiController*>(pController) &&
                !midiControllers.contains(pController)) {
            // Unplugged: close it, the enumerator keeps the object alive
            if (pController->isOpen()) {
                qInfo() << "Closing unplugged controller" << pController->getName();
                // Not closeController(): that would also disable the device
                // in the settings, and it must reopen when plugged back in.
                pController->close();
            }
            continue;
        }
        newControllers.append(pController);
    }
    for (Controller* pController : midiControllers) {
        if (!newControllers.contains(pController)) {
            newControllers.append(pController);
        }
    }

    locker.relock();
    m_controllers = newControllers;
    locker.unlock();
    emit devicesChanged();

    const QStringList mappingPaths(getMappingPaths(m_pConfig));
    for (Controller* pController : midiControllers) {
        if (!pController->isOpen()) {
            setUpController(pController, mappingPaths);
        }
    }
    pollIfAnyControllersOpen();
#endif
}

void ControllerManager::pollIfAnyControllersOpen() {
    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    bool shouldPoll = false;
    for (Controller* pController : controllers) {
        if (pController->isOpen() && pController->isPolling()) {
            shouldPoll = true;
        }
    }
    if (shouldPoll) {
        startPolling();
    } else {
        stopPolling();
    }
}

void ControllerManager::startPolling() {
    // Start the polling timer.
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
        qDebug() << "Controller polling started.";
    }
}

void ControllerManager::stopPolling() {
    m_pollTimer.stop();
    qDebug() << "Controller polling stopped.";
}

void ControllerManager::slotPollDevices() {
    // Note: this function is called from a high priority thread which
    // may stall the GUI or may reduce the available CPU time for other
    // High Priority threads like caching reader or broadcasting more
    // then desired, if it is called endless loop like.
    //
    // This especially happens if a controller like the 3x Speed
    // Stanton SCS.1D emits more massages than Mixxx is able to handle
    // or a controller like Hercules RMX2 goes wild. In such a case the
    // receive buffer is stacked up every call to insane values > 500 messages.
    //
    // To avoid this we pick here a strategies similar like the audio
    // thread. In case pollDevice() takes longer than a call cycle
    // we are cooperative a skip the next cycle to free at least some
    // CPU time
    //
    // Some random test data from a i5-3317U CPU @ 1.70GHz Running
    // Ubuntu Trusty:
    // * Idle poll: ~5 µs.
    // * 5 messages burst (full midi bandwidth): ~872 µs.

    if (m_skipPoll) {
        // skip poll in overload situation
        m_skipPoll = false;
        //qDebug() << "ControllerManager::pollDevices() skip";
        return;
    }

    mixxx::Duration start = mixxx::Time::elapsed();
    bool activity = false;
    for (Controller* pDevice : std::as_const(m_controllers)) {
        if (pDevice->isOpen() && pDevice->isPolling()) {
            activity |= pDevice->poll();
        }
    }

    mixxx::Duration duration = mixxx::Time::elapsed() - start;
#ifdef __ANDROID__
    if (activity) {
        m_lastPollActivity = start;
        if (m_pollTimer.interval() != kPollInterval.toIntegerMillis()) {
            m_pollTimer.setInterval(kPollInterval.toIntegerMillis());
        }
    } else if (start - m_lastPollActivity > kPollIdleAfter &&
            m_pollTimer.interval() != kIdlePollIntervalMs) {
        m_pollTimer.setInterval(kIdlePollIntervalMs);
    }
#else
    Q_UNUSED(activity);
#endif
    if (duration > kPollInterval) {
        m_skipPoll = true;
    }
    //qDebug() << "ControllerManager::pollDevices()" << duration << start;
}

void ControllerManager::openController(Controller* pController) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
    if (!pController) {
        return;
    }
    if (pController->isOpen()) {
        pController->close();
    }
    int result = pController->open(m_pConfig->getResourcePath());
    pollIfAnyControllersOpen();

    // If successfully opened the device, apply the mapping and save the
    // preference setting.
    if (result == 0) {
        // Update configuration to reflect controller is enabled.
        m_pConfig->setValue(
                ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 1);
    }
}

void ControllerManager::closeController(Controller* pController) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
    if (!pController) {
        return;
    }
    pController->close();
    pollIfAnyControllersOpen();
    // Update configuration to reflect controller is disabled.
    m_pConfig->setValue(
            ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 0);
}

// This needs to be called in a Qt::BlockingQueuedConnection so that the
// signaling thread can't alter the LegacyControllerMapping during applying
void ControllerManager::slotApplyMapping(Controller* pController,
        std::shared_ptr<LegacyControllerMapping> pMapping,
        bool bEnabled) {
    VERIFY_OR_DEBUG_ASSERT(pController) {
        qWarning() << "slotApplyMapping got invalid controller!";
        return;
    }

    closeController(pController);
    ConfigKey key(kSettingsGroup, sanitizeDeviceName(pController->getName()));
    if (!pMapping) {
        // Unset the controller mapping for this controller
        pController->setMapping(nullptr);
        m_pConfig->remove(key);
        emit mappingApplied(false);
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(!pMapping->isDirty()) {
        qWarning() << "Mapping is dirty, changes might be lost on restart!";
    }

    // Save the file path/name in the config so it can be auto-loaded at
    // startup next time
    m_pConfig->set(key, pMapping->filePath());

    pController->setMapping(std::move(pMapping));

    if (bEnabled) {
        emit mappingApplied(pController->isMappable());
    } else {
        emit mappingApplied(false);
        return;
    }

    // Note: openController() may call ControllerRenderingEngine::setup()
    // Which has a blocking invokeMethod() call for QOffscreenSurface::create()
    // That why we need to return from this blocking call first.
    QMetaObject::invokeMethod(
            this,
            [this, pController]() { openController(pController); },
            Qt::QueuedConnection);
}

// static
QList<QString> ControllerManager::getMappingPaths(UserSettingsPointer pConfig) {
    QList<QString> scriptPaths;
    scriptPaths.append(userMappingsPath(pConfig));
    scriptPaths.append(resourceMappingsPath(pConfig));
    return scriptPaths;
}
