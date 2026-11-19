#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

#include "control/controlproxy.h"
#include "control/controlpushbutton.h"

namespace mixxx {
namespace qml {

/// Key Sync as a switch: [ChannelN],key_sync_enabled (N = 1..4, off at every start).
/// While it is on, the deck follows the key of the other decks: it shifts to the
/// nearest compatible key (the sync_key of the engine) when it is switched on, when
/// a track is loaded on it and whenever the key of another deck changes (a track is
/// loaded there, it is shifted...). Switched off, the shift that was made stays
/// (like KEY SYNC of rekordbox): reset_key takes it away. One deck follows at a
/// time (a second one would make the two chase each other).
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
        std::unique_ptr<ControlProxy> syncKey;      // [ChannelN],sync_key
        std::unique_ptr<ControlProxy> fileKey;      // [ChannelN],file_key
        std::unique_ptr<ControlProxy> key;          // [ChannelN],key
        std::unique_ptr<ControlProxy> loaded;       // [ChannelN],track_loaded
    };

    bool isEnabled(int deck) const;
    /// Watches the decks that exist (2 on a phone, 4 on a tablet/PC), also those added later
    void watchDecks();
    void watchDeck(int deck);
    void onEnabledChanged(int deck, double value);
    void schedule(int deck);
    void syncNow(int deck);

    Deck m_decks[kDecks];
    std::unique_ptr<ControlProxy> m_pNumDecks; // [App],num_decks
    /// Gathers the changes of one track load (file key, key, loaded arrive one by one)
    QTimer m_timer;
    int m_pending = 0; // a bit per deck
};

} // namespace qml
} // namespace mixxx
