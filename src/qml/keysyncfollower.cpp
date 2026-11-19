#include "qml/keysyncfollower.h"

#include "mixer/playermanager.h"
#include "moc_keysyncfollower.cpp"

namespace mixxx {
namespace qml {

namespace {
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
        const QString group = PlayerManager::groupForDeck(deck);
        Deck& d = m_decks[deck];
        d.enabled = std::make_unique<ControlPushButton>(
                ConfigKey(group, QStringLiteral("key_sync_enabled")));
        d.enabled->setButtonMode(mixxx::control::ButtonMode::Toggle);
        d.syncKey = std::make_unique<ControlProxy>(
                ConfigKey(group, QStringLiteral("sync_key")), this);
        d.fileKey = std::make_unique<ControlProxy>(
                ConfigKey(group, QStringLiteral("file_key")), this);
        d.key = std::make_unique<ControlProxy>(
                ConfigKey(group, QStringLiteral("key")), this);
        d.loaded = std::make_unique<ControlProxy>(
                ConfigKey(group, QStringLiteral("track_loaded")), this);

        connect(d.enabled.get(), &ControlObject::valueChanged, this, [this, deck](double value) {
            onEnabledChanged(deck, value);
        });
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

void KeySyncFollower::syncNow(int deck) {
    if (!isEnabled(deck) || !m_decks[deck].loaded->toBool()) {
        return;
    }
    // sync_key: shifts to the key nearest to the one of the other deck (nothing
    // when there is no other deck with a key)
    m_decks[deck].syncKey->set(1);
    m_decks[deck].syncKey->set(0);
}

} // namespace qml
} // namespace mixxx
