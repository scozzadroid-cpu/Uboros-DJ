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

#ifndef UBOROS_HAS_PLUGINS
void registerBuiltinPlugins(PluginHost&) {
}
#endif

} // namespace plugins
} // namespace mixxx
