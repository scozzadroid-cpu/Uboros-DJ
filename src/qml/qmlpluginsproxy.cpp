#include "qml/qmlpluginsproxy.h"

#include "moc_qmlpluginsproxy.cpp"
#include "plugins/pluginhost.h"

namespace mixxx {
namespace qml {

QmlPluginsProxy::QmlPluginsProxy(QObject* parent)
        : QObject(parent) {
    connect(&plugins::PluginHost::instance(),
            &plugins::PluginHost::hiddenLabelsChanged,
            this,
            &QmlPluginsProxy::hiddenLabelsChanged);
    connect(&plugins::PluginHost::instance(),
            &plugins::PluginHost::loadingTextsChanged,
            this,
            &QmlPluginsProxy::loadingTextsChanged);
}

QmlPluginsProxy* QmlPluginsProxy::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    return new QmlPluginsProxy(pQmlEngine);
}

QStringList QmlPluginsProxy::dashboardSections() const {
    return plugins::PluginHost::instance().dashboardSections();
}

QStringList QmlPluginsProxy::hiddenLabels() const {
    return plugins::PluginHost::instance().hiddenLabels();
}

QVariantMap QmlPluginsProxy::loadingTexts() const {
    return plugins::PluginHost::instance().loadingTexts();
}

void QmlPluginsProxy::trackSelected(const QUrl& url) {
    if (url.isLocalFile()) {
        plugins::PluginHost::instance().prefetchTrack(url.toLocalFile());
    }
}

} // namespace qml
} // namespace mixxx
