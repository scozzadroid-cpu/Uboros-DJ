#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

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

  public:
    explicit QmlPluginsProxy(QObject* parent = nullptr);
    static QmlPluginsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QStringList dashboardSections() const;
    QStringList hiddenLabels() const;

  signals:
    void hiddenLabelsChanged();
};

} // namespace qml
} // namespace mixxx
