#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {
class QmlLibrarySource;
class QmlLibraryTrackListColumn;
} // namespace qml

namespace plugins {

/// A plugin is code that lives OUTSIDE this source tree (a folder with a
/// plugin.cmake, see cmake/UborosPlugins.cmake and docs in PLUGINS.md) and is
/// compiled into the app only when the folder is given with
/// -DUBOROS_PLUGINS_DIR=<folder>[;<folder>...]. Without it nothing of this
/// exists in the app: no thread, no timer, no QML. The host only offers the
/// few extension points below, so that a plugin never needs a change in the
/// files of this tree and both can be developed at the same time.
class Plugin : public QObject {
    Q_OBJECT
  public:
    explicit Plugin(QObject* parent = nullptr)
            : QObject(parent) {
    }

    /// Short lower case name, e.g. "soundcloud" (QML folder, settings group)
    virtual QString id() const = 0;

    /// Called once at start-up, before the QML interface is created. The
    /// plugin must stay idle (no threads, no timers, no network) until the
    /// user does something with it.
    virtual void init(UserSettingsPointer pConfig) {
        Q_UNUSED(pConfig)
    }

    /// Entries for the sidebar of the library, created when the sidebar is
    /// built. They are shown above the USB drives (Rekordbox source). The
    /// entry is hidden or shown later with PluginHost::setLabelHidden().
    virtual QList<qml::QmlLibrarySource*> createLibrarySources(QObject* parent,
            const QList<qml::QmlLibraryTrackListColumn*>& defaultColumns) {
        Q_UNUSED(parent)
        Q_UNUSED(defaultColumns)
        return {};
    }

    /// QML files, relative to the QML root (e.g. "Plugins/soundcloud/Dashboard.qml"):
    /// each one is a section at the end of the Dashboard (Dashboard > ...)
    virtual QStringList dashboardSections() const {
        return {};
    }
};

class PluginHost : public QObject {
    Q_OBJECT
  public:
    static PluginHost& instance();

    /// Takes ownership
    void add(Plugin* pPlugin);
    /// Creates the plugins compiled in (registerBuiltinPlugins) and initializes them, once
    void init(UserSettingsPointer pConfig);

    const QList<Plugin*>& plugins() const {
        return m_plugins;
    }
    QStringList dashboardSections() const;

    /// Sidebar labels the library must not show now (e.g. "SoundCloud" until the
    /// user has signed in)
    QStringList hiddenLabels() const {
        return m_hiddenLabels;
    }
    void setLabelHidden(const QString& label, bool hidden);

  signals:
    void hiddenLabelsChanged();

  private:
    PluginHost() = default;

    QList<Plugin*> m_plugins;
    QStringList m_hiddenLabels;
    bool m_initialized = false;
};

/// Defined by the file CMake generates when plugins are configured
/// (UBOROS_HAS_PLUGINS), empty otherwise
void registerBuiltinPlugins(PluginHost& host);

} // namespace plugins
} // namespace mixxx
