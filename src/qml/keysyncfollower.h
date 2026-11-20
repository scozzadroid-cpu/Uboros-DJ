#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"

namespace mixxx {
namespace qml {

/// Key Sync as a switch: [ChannelN],key_sync_enabled (N = 1..4, off at every start).
/// Switched on ("armed"), the track that is coming in follows the key of the one
/// that plays: it is shifted to the nearest compatible key (KeyUtils::
/// shortestStepsToCompatibleKey, the rule of the engine's sync_key) when it is
/// loaded, when the switch is turned on and when the key of the playing deck
/// changes. Only the INCOMING deck is ever shifted: a loaded deck that does not
/// play. A deck that plays is never touched (its key would jump under the
/// listeners), whether it is the armed one or not: the switch arms the key sync
/// for the next track, it does not matter on which deck it was switched on. With
/// nothing playing, the armed deck follows the other loaded deck. Switched off,
/// the shift that was made stays (like KEY SYNC of rekordbox): reset_key takes it
/// away. One deck holds the switch at a time.
/// Auto Mix (QmlAutoMix) picks and shifts its own incoming track, also only the
/// incoming one: while it runs ([AutoMix],active) this follower stays out of the
/// way (two writers of the same shift), and Auto Mix only shifts a track when the
/// switch is on.
/// [ChannelN],key_sync_status (of the deck that holds the switch) tells the
/// interface what happens: 0 = the incoming track follows, 1 = waiting (no deck
/// plays / no other deck has a track with a key), 2 = the incoming track has no
/// key, 3 = armed, no incoming track (every loaded deck plays), 4 = Auto Mix
/// does it.
/// Everything happens on the GUI thread, as a reaction to control changes: no timer
/// runs while nothing changes.
class KeySyncFollower : public QObject {
    Q_OBJECT
  public:
    explicit KeySyncFollower(QObject* parent = nullptr);

  private:
    static constexpr int kDecks = 4;
    struct Deck {
        std::unique_ptr<ControlPushButton> enabled; // [ChannelN],key_sync_enabled
        std::unique_ptr<ControlObject> status;      // [ChannelN],key_sync_status
        std::unique_ptr<ControlProxy> pitch;        // [ChannelN],pitch (semitones from the file key)
        std::unique_ptr<ControlProxy> distance;     // [ChannelN],visual_key_distance
        std::unique_ptr<ControlProxy> play;         // [ChannelN],play
        std::unique_ptr<ControlProxy> fileKey;      // [ChannelN],file_key
        std::unique_ptr<ControlProxy> key;          // [ChannelN],key
        std::unique_ptr<ControlProxy> loaded;       // [ChannelN],track_loaded
    };

    bool isEnabled(int deck) const;
    /// Watches the decks that exist (2 on a phone, 4 on a tablet/PC), also those added later
    void watchDecks();
    void watchDeck(int deck);
    void onEnabledChanged(int deck, double value);
    void schedule();
    /// Looks at the decks and shifts the incoming one if it has to
    void reconcile();
    /// The deck whose key `incoming` follows: the playing one (any loaded deck
    /// with a key when allowStopped); -1 when there is none
    int pickReference(int incoming, bool allowStopped) const;
    int armedDeck() const;
    bool deckHasKey(int deck) const;
    void setStatus(int deck, int status);

    Deck m_decks[kDecks];
    std::unique_ptr<ControlObject> m_pAutoMixActive; // [AutoMix],active (written by Auto Mix)
    std::unique_ptr<ControlProxy> m_pNumDecks; // [App],num_decks
    /// Gathers the changes of one track load (file key, key, loaded arrive one by one)
    QTimer m_timer;
};

} // namespace qml
} // namespace mixxx
