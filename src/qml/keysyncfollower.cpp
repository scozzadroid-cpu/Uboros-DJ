#include "qml/keysyncfollower.h"

#include <algorithm>
#include <cmath>

#include "mixer/playermanager.h"
#include "track/keyutils.h"
#include "util/logger.h"
#include "moc_keysyncfollower.cpp"

namespace mixxx {
namespace qml {

namespace {
const Logger kLogger("KeySyncFollower");
constexpr int kGatherMs = 150;
} // namespace

KeySyncFollower::KeySyncFollower(QObject* parent)
        : QObject(parent) {
    m_timer.setSingleShot(true);
    m_timer.setInterval(kGatherMs);
    connect(&m_timer, &QTimer::timeout, this, [this]() { reconcile(); });

    // Auto Mix says here when it runs (it shifts its own incoming track)
    m_pAutoMixActive = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[AutoMix]"), QStringLiteral("active")));
    for (int deck = 0; deck < kDecks; ++deck) {
        // The switch exists for every deck (the QML of a 4 deck interface asks for it)
        Deck& d = m_decks[deck];
        d.enabled = std::make_unique<ControlPushButton>(
                ConfigKey(PlayerManager::groupForDeck(deck), QStringLiteral("key_sync_enabled")));
        d.enabled->setButtonMode(mixxx::control::ButtonMode::Toggle);
        d.status = std::make_unique<ControlObject>(
                ConfigKey(PlayerManager::groupForDeck(deck), QStringLiteral("key_sync_status")));
        connect(d.enabled.get(), &ControlObject::valueChanged, this, [this, deck](double value) {
            onEnabledChanged(deck, value);
        });
    }
    m_pNumDecks = std::make_unique<ControlProxy>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks")), this);
    m_pNumDecks->connectValueChanged(this, [this](double) { watchDecks(); });
    watchDecks();
}

void KeySyncFollower::watchDecks() {
    const int decks = std::min(kDecks, static_cast<int>(m_pNumDecks->get()));
    for (int deck = 0; deck < decks; ++deck) {
        if (!m_decks[deck].key) {
            watchDeck(deck);
        }
    }
}

void KeySyncFollower::watchDeck(int deck) {
    const QString group = PlayerManager::groupForDeck(deck);
    Deck& d = m_decks[deck];
    d.pitch = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("pitch")), this);
    d.distance = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("visual_key_distance")), this);
    d.play = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("play")), this);
    d.fileKey = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("file_key")), this);
    d.key = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("key")), this);
    d.loaded = std::make_unique<ControlProxy>(
            ConfigKey(group, QStringLiteral("track_loaded")), this);
    // A track loaded on (or ejected from) any deck, or a key that changed: the
    // incoming track may have to follow. Changes of play do not matter: a track
    // that starts to play keeps the shift it has.
    const auto changed = [this](double) { schedule(); };
    d.fileKey->connectValueChanged(this, changed);
    d.loaded->connectValueChanged(this, changed);
    d.key->connectValueChanged(this, changed);
}

bool KeySyncFollower::isEnabled(int deck) const {
    return m_decks[deck].enabled->toBool();
}

int KeySyncFollower::armedDeck() const {
    for (int deck = 0; deck < kDecks; ++deck) {
        if (isEnabled(deck)) {
            return deck;
        }
    }
    return -1;
}

bool KeySyncFollower::deckHasKey(int deck) const {
    const Deck& d = m_decks[deck];
    return d.loaded && d.key && d.loaded->toBool() &&
            KeyUtils::keyFromNumericValue(d.key->get()) != mixxx::track::io::key::INVALID;
}

void KeySyncFollower::onEnabledChanged(int deck, double value) {
    {
        const Deck& d = m_decks[deck];
        kLogger.info() << "deck" << (deck + 1) << (value > 0 ? "switched on" : "switched off")
                       << "- playing" << (d.play ? d.play->toBool() : false)
                       << "- file key"
                       << (d.fileKey ? KeyUtils::keyToString(KeyUtils::keyFromNumericValue(
                                               d.fileKey->get()))
                                     : QString())
                       << "- shift" << (d.pitch ? d.pitch->get() : 0.0);
    }
    if (value <= 0) {
        setStatus(deck, 0);
        return;
    }
    // One deck at a time
    for (int other = 0; other < kDecks; ++other) {
        if (other != deck && isEnabled(other)) {
            // set() from this object does not call onEnabledChanged() back
            m_decks[other].enabled->set(0);
            setStatus(other, 0);
        }
    }
    // Switched on by hand (pill, pad 8): this deck follows now, also when it
    // is already playing, like KEY SYNC of rekordbox
    reconcile(true);
}

void KeySyncFollower::schedule() {
    if (armedDeck() >= 0) {
        m_timer.start();
    }
}

int KeySyncFollower::pickReference(int incoming, bool allowStopped) const {
    const int decks = std::min(kDecks, static_cast<int>(m_pNumDecks->get()));
    int stopped = -1;
    for (int other = 0; other < decks; ++other) {
        if (other == incoming || !deckHasKey(other)) {
            continue;
        }
        if (m_decks[other].play && m_decks[other].play->toBool()) {
            return other; // the track that plays is the one to follow
        }
        if (allowStopped && stopped < 0) {
            stopped = other;
        }
    }
    return stopped;
}

void KeySyncFollower::setStatus(int deck, int status) {
    Deck& d = m_decks[deck];
    if (d.status && static_cast<int>(d.status->get()) != status) {
        d.status->set(status);
        kLogger.info() << "deck" << (deck + 1) << "status" << status
                       << "(0 following, 1 waiting, 2 no key on the incoming track, 3 armed with no"
                          " incoming track, 4 Auto Mix)";
    }
}

void KeySyncFollower::reconcile(bool shiftArmedNow) {
    const int armed = armedDeck();
    if (armed < 0) {
        return;
    }
    if (m_pAutoMixActive->toBool()) {
        // Auto Mix chooses and shifts its incoming track itself
        setStatus(armed, 4);
        return;
    }
    const int decks = std::min(kDecks, static_cast<int>(m_pNumDecks->get()));
    bool followed = false;
    bool noKey = false;
    bool waiting = false;
    bool anyIncoming = false;
    for (int incoming = 0; incoming < decks; ++incoming) {
        const Deck& d = m_decks[incoming];
        if (!d.loaded || !d.fileKey || !d.pitch || !d.loaded->toBool()) {
            continue;
        }
        const bool playing = d.play && d.play->toBool();
        if (playing && !(shiftArmedNow && incoming == armed)) {
            // A playing deck is never shifted on its own (a new track on the
            // other deck, a key change there): only when its switch is pressed
            continue;
        }
        anyIncoming = true;
        if (KeyUtils::keyFromNumericValue(d.fileKey->get()) == mixxx::track::io::key::INVALID) {
            noKey = true;
            continue;
        }
        // With a deck that plays every incoming deck follows it; with nothing
        // playing only the deck that holds the switch follows the other one
        const int reference = pickReference(incoming, incoming == armed);
        if (playing && reference < 0) {
            continue;
        }
        if (reference < 0) {
            waiting = true;
            continue;
        }
        const Deck& t = m_decks[reference];
        const auto thisKey = KeyUtils::keyFromNumericValue(d.fileKey->get());
        // What sync_key of the engine does: the reference's effective key (after
        // its own shift) and its fraction of a semitone
        const auto targetKey = KeyUtils::keyFromNumericValue(t.key->get());
        const int steps = KeyUtils::shortestStepsToCompatibleKey(thisKey, targetKey);
        const double pitch = steps + t.distance->get();
        if (std::abs(d.pitch->get() - pitch) > 1e-6) {
            d.pitch->set(pitch);
            kLogger.info() << "incoming deck" << (incoming + 1) << "follows deck"
                           << (reference + 1) << "key" << KeyUtils::keyToString(targetKey)
                           << "- shift" << pitch << "semitones";
        }
        followed = true;
    }
    setStatus(armed, followed ? 0 : (noKey ? 2 : (waiting ? 1 : (anyIncoming ? 1 : 3))));
}

} // namespace qml
} // namespace mixxx
