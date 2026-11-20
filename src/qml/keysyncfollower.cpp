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
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        const int pending = std::exchange(m_pending, 0);
        for (int deck = 0; deck < kDecks; ++deck) {
            if (pending & (1 << deck)) {
                syncNow(deck);
            }
        }
    });

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
    // A track loaded on this deck (its file key, or "loaded", changes)
    d.fileKey->connectValueChanged(this, [this, deck](double) {
        if (isEnabled(deck)) {
            schedule(deck);
        }
    });
    d.loaded->connectValueChanged(this, [this, deck](double value) {
        if (value > 0 && isEnabled(deck)) {
            schedule(deck);
        }
        // A track loaded on, or ejected from, another deck: the deck that follows
        // has a new target (or none), also when the key of the new track is the same
        for (int other = 0; other < kDecks; ++other) {
            if (other != deck && isEnabled(other)) {
                schedule(other);
            }
        }
    });
    // The key of this deck changed: the decks that follow have to follow it
    d.key->connectValueChanged(this, [this, deck](double) {
        for (int other = 0; other < kDecks; ++other) {
            if (other != deck && isEnabled(other)) {
                schedule(other);
            }
        }
    });
}

bool KeySyncFollower::isEnabled(int deck) const {
    return m_decks[deck].enabled->toBool();
}

void KeySyncFollower::onEnabledChanged(int deck, double value) {
    if (value <= 0) {
        return;
    }
    // One deck at a time
    for (int other = 0; other < kDecks; ++other) {
        if (other != deck && isEnabled(other)) {
            m_decks[other].enabled->set(0);
        }
    }
    syncNow(deck);
}

void KeySyncFollower::schedule(int deck) {
    m_pending |= 1 << deck;
    m_timer.start();
}

int KeySyncFollower::pickTarget(int deck) const {
    const int decks = std::min(kDecks, static_cast<int>(m_pNumDecks->get()));
    int first = -1;
    for (int other = 0; other < decks; ++other) {
        const Deck& o = m_decks[other];
        if (other == deck || !o.loaded || !o.key || !o.loaded->toBool() ||
                KeyUtils::keyFromNumericValue(o.key->get()) ==
                        mixxx::track::io::key::INVALID) {
            continue;
        }
        if (o.play && o.play->toBool()) {
            return other; // the one that is playing is the one to follow
        }
        if (first < 0) {
            first = other;
        }
    }
    return first;
}

void KeySyncFollower::setStatus(int deck, int status) {
    Deck& d = m_decks[deck];
    if (d.status && static_cast<int>(d.status->get()) != status) {
        d.status->set(status);
        kLogger.info() << "deck" << (deck + 1) << "status" << status
                       << "(0 following, 1 waiting for another deck with a key, 2 no key on this track)";
    }
}

void KeySyncFollower::syncNow(int deck) {
    const Deck& d = m_decks[deck];
    if (!isEnabled(deck) || !d.loaded || !d.fileKey || !d.pitch || !d.loaded->toBool()) {
        setStatus(deck, 0);
        return;
    }
    const auto thisKey = KeyUtils::keyFromNumericValue(d.fileKey->get());
    if (thisKey == mixxx::track::io::key::INVALID) {
        setStatus(deck, 2);
        return;
    }
    const int target = pickTarget(deck);
    if (target < 0) {
        // Nothing to follow yet: the shift that is there stays, and the next
        // track (or key) loaded on another deck takes it from here
        setStatus(deck, 1);
        return;
    }
    const Deck& t = m_decks[target];
    // What sync_key of the engine does, with this target: the other deck's
    // effective key (after its own shift) and its fraction of a semitone
    const auto targetKey = KeyUtils::keyFromNumericValue(t.key->get());
    const int steps = KeyUtils::shortestStepsToCompatibleKey(thisKey, targetKey);
    const double pitch = steps + t.distance->get();
    if (std::abs(d.pitch->get() - pitch) > 1e-6) {
        d.pitch->set(pitch);
        kLogger.info() << "deck" << (deck + 1) << "follows deck" << (target + 1)
                       << "key" << KeyUtils::keyToString(targetKey) << "- shift" << pitch
                       << "semitones";
    }
    setStatus(deck, 0);
}

} // namespace qml
} // namespace mixxx
