#pragma once

#include <QObject>
#include <QQmlEngine>
#include <memory>

#include "control/controlproxy.h"
#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {

/// Mixxx.DeckPrefs: the deck preferences of the desktop dialog (DlgPrefDeck) for the QML
/// interface (Android Dashboard > Decks). Each one is a key of [Controls] in the config
/// (the same keys as the desktop dialog) and, for the ones that are controls of every
/// deck, is also written to the controls.
///
/// The desktop dialog rereads the keys and writes them to the controls in its constructor,
/// which the QML interface never builds: this object does it at start-up (only the keys
/// that exist in the config, so nothing changes until the user chooses something) and when
/// the number of decks changes.
///   * cueMode           [Controls],CueDefault        -> [ChannelN],cue_mode
///                        0 Mixxx, 1 Pioneer, 2 Denon, 3 Numark, 4 Mixxx (no blinking), 5 Cue and play
///   * keylockMode       [Controls],keylockMode       -> [ChannelN],keylockMode      (0 original key, 1 current key)
///   * keyunlockMode     [Controls],keyunlockMode     -> [ChannelN],keyunlockMode    (0 reset the key, 1 keep it)
///   * rateRangePercent  [Controls],RateRangePercent  -> [ChannelN],rateRange        (4 6 8 10 16 24 50 90 %)
/// Read by the engine when a track is loaded (config only, no control to write):
///   * speedAutoReset    [Controls],SpeedAutoReset    (0 none, 1 pitch, 2 pitch and speed, 3 speed)
///   * loadPoint         [Controls],CueRecall         (0 main cue, 1 beginning, 2 first sound, 3 intro start, 4 first hotcue)
class QmlDeckPrefsProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DeckPrefs)
    QML_SINGLETON
    Q_PROPERTY(int cueMode READ cueMode WRITE setCueMode NOTIFY changed)
    Q_PROPERTY(int keylockMode READ keylockMode WRITE setKeylockMode NOTIFY changed)
    Q_PROPERTY(int keyunlockMode READ keyunlockMode WRITE setKeyunlockMode NOTIFY changed)
    Q_PROPERTY(int rateRangePercent READ rateRangePercent WRITE setRateRangePercent NOTIFY changed)
    Q_PROPERTY(int speedAutoReset READ speedAutoReset WRITE setSpeedAutoReset NOTIFY changed)
    Q_PROPERTY(int loadPoint READ loadPoint WRITE setLoadPoint NOTIFY changed)

  public:
    explicit QmlDeckPrefsProxy(UserSettingsPointer pConfig, QObject* parent = nullptr);
    ~QmlDeckPrefsProxy() override;
    /// The object built by QmlApplication (before the QML is loaded)
    static QmlDeckPrefsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    int cueMode() const;
    int keylockMode() const;
    int keyunlockMode() const;
    int rateRangePercent() const;
    int speedAutoReset() const;
    int loadPoint() const;

    void setCueMode(int mode);
    void setKeylockMode(int mode);
    void setKeyunlockMode(int mode);
    void setRateRangePercent(int percent);
    void setSpeedAutoReset(int mode);
    void setLoadPoint(int mode);

  signals:
    void changed();

  private:
    int value(const char* item, int defaultValue) const;
    void store(const char* item, int value);
    /// Writes a value to the control of every deck
    void applyToDecks(const char* control, double value);
    /// The values saved in the config, to the controls of the decks
    void applySaved();

    UserSettingsPointer m_pConfig;
    std::unique_ptr<ControlProxy> m_pNumDecks;
    static inline QmlDeckPrefsProxy* s_pInstance = nullptr;
};

} // namespace qml
} // namespace mixxx
