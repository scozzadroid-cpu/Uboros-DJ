#pragma once

#include <QApplication>
#include <QQmlApplicationEngine>

#include "coreservices.h"
#include "soundio/soundmanagerconfig.h"
#include "qmlautoreload.h"

class ControlObject;
class ControlProxy;
class ControlPushButton;
class GuiTick;
class VisualsManager;

namespace mixxx {
namespace qml {

class KeySyncFollower;

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
    // 1 while the DDJ-FLX4 SMART CFX is held (EQ knobs = stems), for the deck overlay
    std::unique_ptr<ControlObject> m_pStemMode;
    // Pad mode of the controller, for the pad panels of the decks
    // (res/qml/DeckPads.qml): [ChannelN],pad_mode, N = 1..4, -1 when no
    // controller reports it; written by the controller script (DDJ-FLX4:
    // 0 hot cue, 1 beat loop, 2 beat jump, 3 sampler, 4 key shift, 5 pad fx 1,
    // 6 pad fx 2, 7 keyboard/stems). [Skin],pad_beatjump_unit: beats of the
    // smallest BEAT JUMP pad. A script cannot create controls, so they are here.
    std::unique_ptr<ControlObject> m_pPadMode[4];
    std::unique_ptr<ControlObject> m_pPadBeatjumpUnit;
    // [ChannelN],key_sync_enabled: Key Sync as a switch (KEY SYNC of rekordbox)
    std::unique_ptr<KeySyncFollower> m_pKeySyncFollower;

    // [Skin],performance_mode (created by CoreServices, 0 at every start)
    std::unique_ptr<ControlProxy> m_pPerformanceMode;
    void applyPerformanceMode(bool on);
    void setupPerfTrace();
    // Android performance hint session (PerformanceHintManager); the QJniObject
    // lives in the .cpp
    struct PerfHint;
    std::shared_ptr<PerfHint> m_pPerfHint;

    std::unique_ptr<QQmlApplicationEngine> m_pAppEngine;
    QmlAutoReload m_autoReload;

    // Android: started without the configured sound device (e.g. the DDJ-FLX4
    // plugged in after Mixxx) and fell back to the phone output. When a USB
    // device appears, the saved configuration is tried again.
    void tryRestoreSoundConfig();
    bool m_soundFallback = false;
    std::unique_ptr<SoundManagerConfig> m_pSavedSoundConfig;
};

} // namespace qml
} // namespace mixxx
