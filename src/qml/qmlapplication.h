#pragma once

#include <QApplication>
#include <QQmlApplicationEngine>

#include "coreservices.h"
#include "qmlautoreload.h"

class ControlPushButton;
class GuiTick;
class VisualsManager;

namespace mixxx {
namespace qml {

class QmlApplication : public QObject {
    Q_OBJECT
  public:
    QmlApplication(
            QApplication* app,
            const CmdlineArgs& args);
    ~QmlApplication() override;

  public slots:
    void loadQml(const QString& path);

  protected:
    // Feeds key events of the QML window to Mixxx's KeyboardEventFilter
    // (the widget skin installs it on its widgets; QML had no keyboard).
    bool eventFilter(QObject* pObj, QEvent* pEvent) override;

  private:
    std::unique_ptr<CoreServices> m_pCoreServices;
    std::unique_ptr<::VisualsManager> m_visualsManager;

    QString m_mainFilePath;

    // [Skin],show_library: 0 hides the library in compact mode so the
    // waveforms get the whole height (toggled by the FLX4 BROWSE press).
    std::unique_ptr<ControlPushButton> m_pShowLibrary;

    std::unique_ptr<QQmlApplicationEngine> m_pAppEngine;
    QmlAutoReload m_autoReload;
};

} // namespace qml
} // namespace mixxx
