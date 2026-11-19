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
    /// group ("[Channel1]") -> "Loading ..." text while a plugin gets the track of the deck
    Q_PROPERTY(QVariantMap loadingTexts READ loadingTexts NOTIFY loadingTextsChanged)

  public:
    explicit QmlPluginsProxy(QObject* parent = nullptr);
    static QmlPluginsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QStringList dashboardSections() const;
    QStringList hiddenLabels() const;
    QVariantMap loadingTexts() const;

    /// A track of a library list was selected (file location): plugins may prefetch it
    Q_INVOKABLE void trackSelected(const QUrl& url);

  signals:
    void hiddenLabelsChanged();
    void loadingTextsChanged();
};

} // namespace qml
} // namespace mixxx
