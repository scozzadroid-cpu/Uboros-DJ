#include "qml/qmlconfigproxy.h"

#include "moc_qmlconfigproxy.cpp"
#include "preferences/colorpalettesettings.h"
#include "preferences/constants.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include "util/logger.h"

#ifdef __ANDROID__
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace {
QVariantList paletteToQColorList(const ColorPalette& palette) {
    QVariantList colors;
    for (mixxx::RgbColor rgbColor : palette) {
        colors.append(mixxx::RgbColor::toQVariantColor(rgbColor));
    }
    return colors;
}

const QString kPreferencesGroup = QStringLiteral("[Preferences]");
const QString kMultiSamplingKey = QStringLiteral("multi_sampling");
const QString k3DHardwareAccelerationKey = QStringLiteral("force_hardware_acceleration");

const QString kWaveformGroup = QStringLiteral("[Waveform]");
const QString kWaveformZoomSynchronizationKey = QStringLiteral("ZoomSynchronization");
const QString kWaveformDefaultZoomKey = QStringLiteral("DefaultZoom");
const bool kWaveformZoomSynchronizationDefault = true;
const double kWaveformDefaultZoomDefault = 3.0;
const QString kWaveformGpuKey = QStringLiteral("GpuRendering");

const mixxx::Logger kLogger("QmlConfigProxy");

#ifndef GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#endif

/// Whether the GPU can draw the waveforms (waveformfiltered.vert): OpenGL
/// (ES) 3 or newer, texture reads in the vertex shader, and a hardware
/// renderer (with a software one the CPU renderer is cheaper).
bool detectWaveformGpuSupport() {
    QOffscreenSurface surface;
    surface.create();
    QOpenGLContext context;
    if (!context.create() || !context.makeCurrent(&surface)) {
        kLogger.warning() << "GPU waveform: no OpenGL context, using the CPU";
        return false;
    }
    QOpenGLFunctions* pFunctions = context.functions();
    GLint vertexTextureUnits = 0;
    pFunctions->glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vertexTextureUnits);
    const auto renderer = QByteArray(reinterpret_cast<const char*>(
                                             pFunctions->glGetString(GL_RENDERER)))
                                  .toLower();
    const QSurfaceFormat format = context.format();
    context.doneCurrent();
    const bool software = renderer.contains("llvmpipe") ||
            renderer.contains("softpipe") || renderer.contains("swiftshader") ||
            renderer.contains("software");
    const bool supported = format.majorVersion() >= 3 && vertexTextureUnits > 0 && !software;
    kLogger.info() << "GPU waveform:" << (supported ? "supported" : "not supported")
                   << "-" << renderer << "OpenGL" << format.majorVersion()
                   << format.minorVersion() << (context.isOpenGLES() ? "ES" : "")
                   << "vertex texture units" << vertexTextureUnits;
    return supported;
}

} // namespace

namespace mixxx {
namespace qml {

QmlConfigProxy::QmlConfigProxy(
        UserSettingsPointer pConfig, QObject* parent)
        : QObject(parent), m_pConfig(pConfig) {
}

QVariantList QmlConfigProxy::getHotcueColorPalette() {
    ColorPaletteSettings colorPaletteSettings(m_pConfig);
    return paletteToQColorList(colorPaletteSettings.getHotcueColorPalette());
}

QVariantList QmlConfigProxy::getTrackColorPalette() {
    ColorPaletteSettings colorPaletteSettings(m_pConfig);
    return paletteToQColorList(colorPaletteSettings.getTrackColorPalette());
}

int QmlConfigProxy::getMultiSamplingLevel() {
    return static_cast<int>(m_pConfig->getValue(
            ConfigKey(kPreferencesGroup, kMultiSamplingKey),
            mixxx::preferences::MultiSamplingMode::Disabled));
}

bool QmlConfigProxy::useAcceleration() {
    if (!m_pConfig->exists(
                ConfigKey(kPreferencesGroup, k3DHardwareAccelerationKey))) {
        // TODO: detect whether QML currently run with 3D acceleration. QSGRendererInterface?
        return false;
    }
    return m_pConfig->getValue<bool>(
            ConfigKey(kPreferencesGroup, k3DHardwareAccelerationKey));
}

bool QmlConfigProxy::waveformZoomSynchronization() {
    return m_pConfig->getValue(
            ConfigKey(kWaveformGroup, kWaveformZoomSynchronizationKey),
            kWaveformZoomSynchronizationDefault);
}
double QmlConfigProxy::waveformDefaultZoom() {
    return m_pConfig->getValue(
            ConfigKey(kWaveformGroup, kWaveformDefaultZoomKey),
            kWaveformDefaultZoomDefault);
}

// static
bool QmlConfigProxy::waveformGpuSupported() {
    // Once per run, on the first call (GUI thread)
    static const bool supported = detectWaveformGpuSupport();
    return supported;
}

// static
bool QmlConfigProxy::useWaveformGpu() {
    if (!s_pUserSettings) {
        return false;
    }
    const ConfigKey key(kWaveformGroup, kWaveformGpuKey);
    if (!s_pUserSettings->exists(key)) {
        // First start: decide from what the GPU can do; the user can
        // change it in the dashboard
        s_pUserSettings->setValue(key, waveformGpuSupported());
    }
    return s_pUserSettings->getValue<bool>(key) && waveformGpuSupported();
}

bool QmlConfigProxy::waveformGpu() const {
    return useWaveformGpu();
}

void QmlConfigProxy::setWaveformGpu(bool enabled) {
    if (enabled && !waveformGpuSupported()) {
        return;
    }
    if (enabled == waveformGpu()) {
        return;
    }
    m_pConfig->setValue(ConfigKey(kWaveformGroup, kWaveformGpuKey), enabled);
    m_pConfig->save();
    emit waveformGpuChanged(enabled);
}

// static
QmlConfigProxy* QmlConfigProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pUserSettings) {
        qWarning() << "UserSettings hasn't been registered yet";
        return nullptr;
    }
    // On the GUI thread, before any waveform display: the renderers are
    // made on the render thread, which must not create OpenGL contexts or
    // write the settings
    useWaveformGpu();
    return new QmlConfigProxy(s_pUserSettings, pQmlEngine);
}

void QmlConfigProxy::setScreenOrientation(ScreenOrientation orientation) {
#ifdef __ANDROID__
    // android.content.pm.ActivityInfo constants
    constexpr jint kSensorLandscape = 6;
    constexpr jint kSensorPortrait = 7;
    constexpr jint kFullUser = 13;
    jint value = kFullUser;
    switch (orientation) {
    case ScreenOrientation::Landscape:
        value = kSensorLandscape;
        break;
    case ScreenOrientation::Portrait:
        value = kSensorPortrait;
        break;
    case ScreenOrientation::Any:
        break;
    }
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([value]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid()) {
            activity.callMethod<void>("setRequestedOrientation", "(I)V", value);
        }
    });
#else
    Q_UNUSED(orientation);
#endif
}

} // namespace qml
} // namespace mixxx
