#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {

class QmlConfigProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Config)
    QML_SINGLETON
    // Waveforms drawn by the GPU (WaveformRendererFilteredGpu) instead of
    // the CPU. Decided on the first start from what the GPU can do, then
    // chosen by the user in the dashboard.
    Q_PROPERTY(bool waveformGpu READ waveformGpu WRITE setWaveformGpu NOTIFY waveformGpuChanged)
    // Whether this GPU can draw them (OpenGL ES 3 with vertex texture fetch)
    Q_PROPERTY(bool waveformGpuSupported READ waveformGpuSupported CONSTANT)
    // OLED burn-in protection: whites a bit dimmer, the DJ view moved by a
    // couple of pixels every few minutes. No cost per frame.
    // First-run tutorial: true once finished or skipped, never shown again
    Q_PROPERTY(bool tutorialDone READ tutorialDone WRITE setTutorialDone NOTIFY tutorialDoneChanged)
    Q_PROPERTY(bool burnInProtection READ burnInProtection WRITE setBurnInProtection NOTIFY burnInProtectionChanged)
    // "Normalize volume": every track is played at the same level (the loudness
    // measured by the analysis: ReplayGain). Off by default: tracks play at their
    // own level, untouched. Takes effect at once, also on the tracks that are playing.
    Q_PROPERTY(bool normalizeLevel READ normalizeLevel WRITE setNormalizeLevel NOTIFY normalizeLevelChanged)
    // Screen turned black (nothing drawn, audio goes on) after these minutes
    // without DJ controller input or touches; 0 = never
    Q_PROPERTY(int screenOffMinutes READ screenOffMinutes WRITE setScreenOffMinutes NOTIFY screenOffMinutesChanged)
    /// Largest size of the log file (mixxx.log) in MB, 0 = no limit; applied at the next start
    Q_PROPERTY(int logMaxFileSizeMb READ logMaxFileSizeMb WRITE setLogMaxFileSizeMb NOTIFY logLimitsChanged)
    /// How many older log files (mixxx.log.1 ...) are kept; applied at the next start
    Q_PROPERTY(int logMaxOldFiles READ logMaxOldFiles WRITE setLogMaxOldFiles NOTIFY logLimitsChanged)
    // Set by QML while the screen is black: the touch that wakes it is
    // swallowed (does not press what is under the finger)
    Q_PROPERTY(bool screenAsleep READ screenAsleep WRITE setScreenAsleep NOTIFY screenAsleepChanged)
    // Interface profile ([Skin],interface_profile): "uboros" (default),
    // "club" or "stack". Anything else is ignored. Applied at once, no restart.
    Q_PROPERTY(QString interfaceProfile READ interfaceProfile WRITE setInterfaceProfile NOTIFY interfaceProfileChanged)
  public:
    explicit QmlConfigProxy(
            UserSettingsPointer pConfig,
            QObject* parent = nullptr);

    // We use method here instead of properties as there is no way to achieve property binding
    // with UserSettings, since there is no synchronisation upon mutations.
    Q_INVOKABLE QVariantList getHotcueColorPalette();
    Q_INVOKABLE QVariantList getTrackColorPalette();
    Q_INVOKABLE int getMultiSamplingLevel();
    Q_INVOKABLE bool useAcceleration();

    // Waveform settings
    Q_INVOKABLE bool waveformZoomSynchronization();
    Q_INVOKABLE double waveformDefaultZoom();

    bool waveformGpu() const;
    void setWaveformGpu(bool enabled);
    static bool waveformGpuSupported();
    /// For the renderer factories (no QML engine needed)
    static bool useWaveformGpu();

    // Screen orientation requested to the OS (Android only, no-op elsewhere).
    // Values of ScreenOrientation.
    enum class ScreenOrientation {
        Landscape,
        Portrait,
        Any,
    };
    Q_ENUM(ScreenOrientation)
    Q_INVOKABLE void setScreenOrientation(mixxx::qml::QmlConfigProxy::ScreenOrientation orientation);

    bool tutorialDone() const;
    void setTutorialDone(bool done);
    bool burnInProtection() const;
    void setBurnInProtection(bool enabled);
    bool normalizeLevel() const;
    void setNormalizeLevel(bool enabled);
    int screenOffMinutes() const;
    void setScreenOffMinutes(int minutes);
    int logMaxFileSizeMb() const;
    void setLogMaxFileSizeMb(int megabytes);
    int logMaxOldFiles() const;
    void setLogMaxOldFiles(int files);
    bool screenAsleep() const {
        return m_screenAsleep;
    }
    void setScreenAsleep(bool asleep);
    QString interfaceProfile() const;
    void setInterfaceProfile(const QString& profile);

    static QmlConfigProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline void registerUserSettings(UserSettingsPointer pConfig) {
        s_pUserSettings = std::move(pConfig);
    }

    static UserSettingsPointer get() {
        return s_pUserSettings;
    }

  signals:
    void waveformGpuChanged(bool enabled);
    void tutorialDoneChanged(bool done);
    void burnInProtectionChanged(bool enabled);
    void normalizeLevelChanged(bool enabled);
    void screenOffMinutesChanged(int minutes);
    void logLimitsChanged();
    void screenAsleepChanged(bool asleep);
    void interfaceProfileChanged(const QString& profile);
    /// DJ controller input or a touch (GUI thread)
    void userActivity();

  protected:
    bool eventFilter(QObject* pObject, QEvent* pEvent) override;

  private:
    bool m_screenAsleep = false;
    // Swallowing the rest of the touch that woke the screen
    bool m_swallowingTouch = false;
    // ... and for a moment after it ends (synthesized mouse events)
    QElapsedTimer m_swallowTimer;
    bool isSwallowWindowOpen() const;

    static inline UserSettingsPointer s_pUserSettings = nullptr;

    const UserSettingsPointer m_pConfig;
};

} // namespace qml
} // namespace mixxx
