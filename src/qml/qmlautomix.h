#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QtQml>
#include <memory>

#include "control/pollingcontrolproxy.h"
#include "track/track_decl.h"

class PlayerManager;

namespace mixxx {
namespace qml {

/// Auto Mix for the phone interface: lets the DJ walk away from the console.
///
/// It plays deck 1 and 2 in turn. While one deck plays, it chooses the next
/// track and loads it on the other deck: a track not played in this session,
/// from the same drive/folder tree as the playing one when possible, with a
/// key compatible on the Camelot wheel (or, when KSYNC is armed, one that
/// becomes compatible with a key shift of at most 2 semitones, applied to the
/// incoming track only, with keylock) and a BPM close to the current tempo. Near the end of the playing track (at its outro
/// cue, or 32 beats before its last beat) it starts the next one on a phrase
/// boundary with sync and quantize, and mixes over 32 beats:
///  - both tracks with stems: drums in first, bass swap at beat 16 with the
///    melody, outgoing vocals out before the incoming vocals come in;
///  - otherwise the EQ: incoming low cut, bass swap at beat 16, outgoing
///    fader down over the last 8 beats (stems are used on the deck that has
///    them).
class QmlAutoMix : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    QML_NAMED_ELEMENT(AutoMix)
    QML_SINGLETON

  public:
    explicit QmlAutoMix(QObject* parent = nullptr);
    ~QmlAutoMix() override;

    static QmlAutoMix* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isActive() const {
        return m_active;
    }
    void setActive(bool active);
    QString status() const {
        return m_status;
    }

  signals:
    void activeChanged(bool active);
    void statusChanged(const QString& status);

  private:
    struct Deck;
    struct Candidate;

    enum class State {
        Idle,
        WaitingForNext,  // the next track is loading/analyzing
        Ready,           // next track loaded, waiting for the transition point
        Mixing,
    };

    void tick();
    void startPlaybackIfIdle();
    bool keySyncArmed() const;
    bool loadNextTrack(Deck* pPlaying, Deck* pNext);
    bool prepareNextDeck(Deck* pPlaying, Deck* pNext);
    void startTransition(Deck* pPlaying, Deck* pNext);
    void applyTransition(double beats);
    void finishTransition();
    void resetDeckMix(Deck* pDeck);
    void setStatus(const QString& status);
    Deck* playingDeck() const;
    Deck* otherDeck(const Deck* pDeck) const;

    std::shared_ptr<PlayerManager> m_pPlayerManager;
    std::unique_ptr<Deck> m_decks[2];
    QTimer m_timer;
    bool m_active = false;
    State m_state = State::Idle;
    QString m_status;
    QSet<QString> m_usedLocations;
    Deck* m_pFrom = nullptr;
    Deck* m_pTo = nullptr;
    double m_transitionStartFrame = 0;
    double m_transitionBeats = 32;
    double m_lastPhraseBeat = -1;
    bool m_stemsFrom = false;
    bool m_stemsTo = false;
    int m_pendingKeyShift = 0;
    PollingControlProxy m_crossfader;
};

} // namespace qml
} // namespace mixxx
