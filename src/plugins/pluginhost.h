#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

#include "preferences/usersettings.h"
#include "track/track_decl.h"

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

    /// Called before a track is loaded into a deck/sampler/preview deck ("group").
    /// A plugin whose tracks are not on the device yet (a streaming service)
    /// returns true and calls `ready` later, on the GUI thread, with the track
    /// to load (a track whose file now exists), or with null if it could not be
    /// got. Return false (default) to let the load go on at once.
    virtual bool prepareTrack(const QString& group,
            const TrackPointer& pTrack,
            const std::function<void(TrackPointer)>& ready) {
        Q_UNUSED(group)
        Q_UNUSED(pTrack)
        Q_UNUSED(ready)
        return false;
    }

    /// The user selected the track with this file location in a library list:
    /// a plugin may start getting it in the background (and analyze it).
    virtual void prefetchTrack(const QString& location) {
        Q_UNUSED(location)
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

    /// See Plugin::prepareTrack(): true when a plugin took over the load
    bool prepareTrack(const QString& group,
            const TrackPointer& pTrack,
            const std::function<void(TrackPointer)>& ready);
    void prefetchTrack(const QString& location);

    /// What a deck shows while a plugin gets its track (a panel over the info bar): group
    /// ("[Channel1]") -> {title, artist, progress (0..1, -1 = not known yet), state ("loading" or
    /// "error"), message}. Absent when the deck waits for nothing.
    QVariantMap loadingInfo() const {
        return m_loadingInfo;
    }
    /// An empty map clears it
    void setLoadingInfo(const QString& group, const QVariantMap& info);

  signals:
    void hiddenLabelsChanged();
    void loadingInfoChanged();

  private:
    PluginHost() = default;

    QList<Plugin*> m_plugins;
    QStringList m_hiddenLabels;
    QVariantMap m_loadingInfo;
    bool m_initialized = false;
};

/// Defined by the file CMake generates when plugins are configured
/// (UBOROS_HAS_PLUGINS), empty otherwise
void registerBuiltinPlugins(PluginHost& host);

} // namespace plugins
} // namespace mixxx
