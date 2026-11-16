#pragma once
#include <QColor>
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

    static QmlConfigProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline void registerUserSettings(UserSettingsPointer pConfig) {
        s_pUserSettings = std::move(pConfig);
    }

    static UserSettingsPointer get() {
        return s_pUserSettings;
    }

  signals:
    void waveformGpuChanged(bool enabled);

  private:
    static inline UserSettingsPointer s_pUserSettings = nullptr;

    const UserSettingsPointer m_pConfig;
};

} // namespace qml
} // namespace mixxx
