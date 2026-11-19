#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

namespace mixxx {
namespace qml {

/// Mixxx.Plugins: what the plugins compiled into the app (src/plugins) ask
/// of the QML interface. Empty when there are none.
class QmlPluginsProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Plugins)
    QML_SINGLETON
    /// QML files (relative to the QML root), one Dashboard section each
    Q_PROPERTY(QStringList dashboardSections READ dashboardSections CONSTANT)
    /// Sidebar labels that must not be shown now
    Q_PROPERTY(QStringList hiddenLabels READ hiddenLabels NOTIFY hiddenLabelsChanged)
    /// group ("[Channel1]") -> {title, artist, progress, state, message} while a plugin gets the
    /// track of the deck (see PluginHost::loadingInfo)
    Q_PROPERTY(QVariantMap loadingInfo READ loadingInfo NOTIFY loadingInfoChanged)

  public:
    explicit QmlPluginsProxy(QObject* parent = nullptr);
    static QmlPluginsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QStringList dashboardSections() const;
    QStringList hiddenLabels() const;
    QVariantMap loadingInfo() const;

    /// A track of a library list was selected (file location): plugins may prefetch it
    Q_INVOKABLE void trackSelected(const QUrl& url);

  signals:
    void hiddenLabelsChanged();
    void loadingInfoChanged();
};

} // namespace qml
} // namespace mixxx
