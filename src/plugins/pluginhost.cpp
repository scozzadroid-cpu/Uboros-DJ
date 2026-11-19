#include "plugins/pluginhost.h"

#include "moc_pluginhost.cpp"
#include "util/logger.h"

namespace mixxx {
namespace plugins {

namespace {
const Logger kLogger("PluginHost");
} // namespace

PluginHost& PluginHost::instance() {
    static PluginHost host;
    return host;
}

void PluginHost::add(Plugin* pPlugin) {
    if (!pPlugin) {
        return;
    }
    pPlugin->setParent(this);
    m_plugins.append(pPlugin);
}

void PluginHost::init(UserSettingsPointer pConfig) {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    registerBuiltinPlugins(*this);
    for (Plugin* pPlugin : std::as_const(m_plugins)) {
        kLogger.info() << "plugin" << pPlugin->id();
        pPlugin->init(pConfig);
    }
}

QStringList PluginHost::dashboardSections() const {
    QStringList sections;
    for (const Plugin* pPlugin : m_plugins) {
        sections += pPlugin->dashboardSections();
    }
    return sections;
}

void PluginHost::setLabelHidden(const QString& label, bool hidden) {
    const bool wasHidden = m_hiddenLabels.contains(label);
    if (hidden == wasHidden) {
        return;
    }
    if (hidden) {
        m_hiddenLabels.append(label);
    } else {
        m_hiddenLabels.removeAll(label);
    }
    emit hiddenLabelsChanged();
}

bool PluginHost::prepareTrack(const QString& group,
        const TrackPointer& pTrack,
        const std::function<void(TrackPointer)>& ready) {
    for (Plugin* pPlugin : std::as_const(m_plugins)) {
        if (pPlugin->prepareTrack(group, pTrack, ready)) {
            return true;
        }
    }
    return false;
}

void PluginHost::prefetchTrack(const QString& location) {
    for (Plugin* pPlugin : std::as_const(m_plugins)) {
        pPlugin->prefetchTrack(location);
    }
}

void PluginHost::setLoadingText(const QString& group, const QString& text) {
    if (text.isEmpty()) {
        if (m_loadingTexts.remove(group) == 0) {
            return;
        }
    } else {
        if (m_loadingTexts.value(group).toString() == text) {
            return;
        }
        m_loadingTexts.insert(group, text);
    }
    emit loadingTextsChanged();
}

#ifndef UBOROS_HAS_PLUGINS
void registerBuiltinPlugins(PluginHost&) {
}
#endif

} // namespace plugins
} // namespace mixxx
