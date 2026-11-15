#pragma once

#include <QMutex>
#include <QSharedPointer>
#include <QTimer>
#include <memory>

#include "controllers/controllerenumerator.h"
#include "preferences/usersettings.h"
#include "util/duration.h"

// Forward declaration(s)
class Controller;
class ControllerLearningEventFilter;
class MappingInfoEnumerator;
class LegacyControllerMapping;
class ControllerEnumerator;

/// Function to sort controllers by name
bool controllerCompare(Controller *a, Controller *b);

/// Manages enumeration/operation/deletion of hardware controllers.
class ControllerManager : public QObject {
    Q_OBJECT
  public:
    ControllerManager(UserSettingsPointer pConfig);
    virtual ~ControllerManager();

    static const mixxx::Duration kPollInterval;

    QList<Controller*> getControllers() const;
    QList<Controller*> getControllerList(bool outputDevices=true, bool inputDevices=true);
    ControllerLearningEventFilter* getControllerLearningEventFilter() const;
    QSharedPointer<MappingInfoEnumerator> getMainThreadUserMappingEnumerator() {
        return m_pMainThreadUserMappingEnumerator;
    }
    QSharedPointer<MappingInfoEnumerator> getMainThreadSystemMappingEnumerator() {
        return m_pMainThreadSystemMappingEnumerator;
    }
    QString getConfiguredMappingFileForDevice(const QString& name);

    /// Prevent other parts of Mixxx from having to manually connect to our slots
    void setUpDevices() { emit requestSetUpDevices(); };

    static QList<QString> getMappingPaths(UserSettingsPointer pConfig);

  signals:
    void initialized();
    void devicesChanged();
    void requestSetUpDevices();
    void requestShutdown();
    void requestInitialize();
    void mappingApplied(bool applied);

  public slots:
    void slotApplyMapping(Controller* pController,
            std::shared_ptr<LegacyControllerMapping> pMapping,
            bool bEnabled);

  private slots:
    /// Perform initialization that should be delayed until the ControllerManager
    /// thread is started.
    void slotInitialize();
    /// Open whatever controllers are selected in the preferences. This currently
    /// only runs on start-up but maybe should instead be signaled by the
    /// preferences dialog on apply, and only open/close changed devices
    void slotSetUpDevices();
    void slotShutdown();
    /// Calls poll() on all devices that have isPolling() true.
    void slotPollDevices();
    /// Android: rescan MIDI devices when MidiManager reports a change.
    void slotCheckMidiHotplug();

  private:
    void updateControllerList();
    void startPolling();
    void stopPolling();
    void pollIfAnyControllersOpen();
    void openController(Controller* pController);
    void closeController(Controller* pController);
    /// Load the configured mapping for pController and open it, if enabled.
    void setUpController(Controller* pController, const QStringList& mappingPaths);
    /// If pController has no configuration yet, enable it with the unique
    /// system mapping whose name ends with the same product name.
    void autoConfigureController(Controller* pController);

    UserSettingsPointer m_pConfig;
    ControllerLearningEventFilter* m_pControllerLearningEventFilter;
    QTimer m_pollTimer;
    mutable QMutex m_mutex;
    QList<ControllerEnumerator*> m_enumerators;
    QList<Controller*> m_controllers;
    QThread* m_pThread;
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadUserMappingEnumerator;
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadSystemMappingEnumerator;
    bool m_skipPoll;
#ifdef __ANDROID__
    // Adaptive polling: 1 ms while the controller is in use, slower when it
    // is idle (1000 wake-ups per second cost ~12% of a core on a phone).
    mixxx::Duration m_lastPollActivity;
#endif
    ControllerEnumerator* m_pAndroidMidiEnumerator = nullptr;
    QTimer* m_pHotplugTimer = nullptr;
    int m_midiDeviceGeneration = -1;
};
