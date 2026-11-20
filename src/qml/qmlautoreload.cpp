#include "qml/qmlautoreload.h"

#include <QDebug>
#include <QFileInfo>
#include <QUrl>

#include "moc_qmlautoreload.cpp"
#include "util/autofilereloader.h"

namespace mixxx {

namespace qml {

QmlAutoReload::QmlAutoReload()
        : m_autoReloader(RuntimeLoggingCategory(QStringLiteral("qml_auto_reload"))) {
    // propagate inner signal outwards
    connect(&m_autoReloader, &AutoFileReloader::fileChanged, this, &QmlAutoReload::triggered);
};

QUrl QmlAutoReload::intercept(const QUrl& url, QQmlAbstractUrlInterceptor::DataType) {
    if (!url.isLocalFile()) {
        return url;
    }
#ifdef __ANDROID__
    // Development feature (edit a QML file, the UI reloads). On Android the QML
    // is in the APK assets, so nothing useful is watched; but the interceptor also
    // sees every other local file URL the engine resolves, e.g. Qt's runtime
    // shader cache (cache/_qt_QGfxShaderBuilder_*/*.qsb). When something cleans the
    // app cache those files change, the whole UI is reloaded (a new
    // QQmlApplicationEngine and window) and Qt aborts: "makeCurrent(): no
    // EGLSurface, likely Surface destroyed by Android" -> qFatal "Failed to
    // initialize graphics backend for OpenGL" (tombstones 27 and 29).
    return url;
#endif
    QString filePath = url.toLocalFile();
    if (!QFileInfo(filePath).isFile()) {
        return url;
    }
    m_autoReloader.addPath(filePath);
    return url;
}

} // namespace qml
} // namespace mixxx
