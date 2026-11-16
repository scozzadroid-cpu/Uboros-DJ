#include "qml/qmlautomix.h"

#include <QRandomGenerator>
#include <QSqlQuery>
#include <algorithm>
#include <cmath>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_qmlautomix.cpp"
#include "qml/qmllibraryproxy.h"
#include "qml/qmlplayermanagerproxy.h"
#include "track/beats.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/logger.h"

namespace mixxx {
namespace qml {

namespace {

const Logger kLogger("AutoMix");

constexpr int kTickMs = 25;
constexpr double kMixBeats = 32;
constexpr int kPhraseBeats = 16;
constexpr double kMaxTempoDifference = 0.08; // 8%
constexpr int kMaxKeyShift = 2;              // semitones
constexpr int kNumStems = 4;
// NI stem order
constexpr int kDrums = 0;
constexpr int kBass = 1;
constexpr int kMelody = 2;
constexpr int kVocals = 3;

double ramp(double t, double from, double to) {
    if (t <= from) {
        return 0;
    }
    if (t >= to) {
        return 1;
    }
    return (t - from) / (to - from);
}

QString deviceRoot(const QString& location) {
    // /storage/XXXX-XXXX/... (USB drive) or the parent folder of the track
    if (location.startsWith(QStringLiteral("/storage/")) &&
            !location.startsWith(QStringLiteral("/storage/emulated/"))) {
        const int slash = location.indexOf(QChar('/'), 9);
        if (slash > 0) {
            return location.left(slash + 1);
        }
    }
    const int slash = location.lastIndexOf(QChar('/'));
    return slash > 0 ? location.left(slash + 1) : QString();
}

QString keyText(track::io::key::ChromaticKey key) {
    return KeyUtils::keyToString(key, KeyUtils::KeyNotation::Lancelot);
}

} // namespace

struct QmlAutoMix::Deck {
    explicit Deck(const QString& g)
            : group(g),
              play(g, QStringLiteral("play")),
              playposition(g, QStringLiteral("playposition")),
              trackSamples(g, QStringLiteral("track_samples")),
              trackLoaded(g, QStringLiteral("track_loaded")),
              bpm(g, QStringLiteral("bpm")),
              fileBpm(g, QStringLiteral("file_bpm")),
              key(g, QStringLiteral("key")),
              volume(g, QStringLiteral("volume")),
              syncEnabled(g, QStringLiteral("sync_enabled")),
              quantize(g, QStringLiteral("quantize")),
              keylock(g, QStringLiteral("keylock")),
              pitchAdjust(g, QStringLiteral("pitch_adjust")),
              stemCount(g, QStringLiteral("stem_count")),
              introStart(g, QStringLiteral("intro_start_position")),
              outroStart(g, QStringLiteral("outro_start_position")),
              cuePoint(g, QStringLiteral("cue_point")),
              eqLow(QStringLiteral("[EqualizerRack1_") + g + QStringLiteral("_Effect1]"),
                      QStringLiteral("parameter1")),
              eqMid(QStringLiteral("[EqualizerRack1_") + g + QStringLiteral("_Effect1]"),
                      QStringLiteral("parameter2")),
              eqHigh(QStringLiteral("[EqualizerRack1_") + g + QStringLiteral("_Effect1]"),
                      QStringLiteral("parameter3")),
              stems{PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem1]"),
                            QStringLiteral("volume")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem2]"),
                              QStringLiteral("volume")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem3]"),
                              QStringLiteral("volume")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem4]"),
                              QStringLiteral("volume"))},
              stemMutes{PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem1]"),
                                QStringLiteral("mute")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem2]"),
                              QStringLiteral("mute")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem3]"),
                              QStringLiteral("mute")),
                      PollingControlProxy(g.chopped(1) + QStringLiteral("_Stem4]"),
                              QStringLiteral("mute"))} {
    }

    TrackPointer track(const std::shared_ptr<PlayerManager>& pPlayerManager) const {
        BaseTrackPlayer* pPlayer = pPlayerManager->getPlayer(group);
        return pPlayer ? pPlayer->getLoadedTrack() : TrackPointer();
    }
    bool isPlaying() const {
        return play.toBool() && trackLoaded.toBool();
    }
    bool hasStems() const {
        return stemCount.get() >= kNumStems;
    }
    // Current position in frames
    double frame() const {
        return playposition.get() * trackSamples.get() / 2;
    }
    void seekToFrame(double frame) {
        const double samples = trackSamples.get();
        if (samples > 0) {
            playposition.set(std::clamp(frame * 2 / samples, 0.0, 1.0));
        }
    }

    QString group;
    PollingControlProxy play;
    PollingControlProxy playposition;
    PollingControlProxy trackSamples;
    PollingControlProxy trackLoaded;
    PollingControlProxy bpm;
    PollingControlProxy fileBpm;
    PollingControlProxy key;
    PollingControlProxy volume;
    PollingControlProxy syncEnabled;
    PollingControlProxy quantize;
    PollingControlProxy keylock;
    PollingControlProxy pitchAdjust;
    PollingControlProxy stemCount;
    PollingControlProxy introStart;
    PollingControlProxy outroStart;
    PollingControlProxy cuePoint;
    PollingControlProxy eqLow;
    PollingControlProxy eqMid;
    PollingControlProxy eqHigh;
    PollingControlProxy stems[kNumStems];
    PollingControlProxy stemMutes[kNumStems];
    QString loadedLocation; // what Auto Mix loaded here
    double startFrame = 0;  // where the next track starts (intro/first beat)
};

QmlAutoMix::QmlAutoMix(QObject* parent)
        : QObject(parent),
          m_pPlayerManager(QmlPlayerManagerProxy::playerManager()),
          m_crossfader(QStringLiteral("[Master]"), QStringLiteral("crossfader")) {
    m_decks[0] = std::make_unique<Deck>(QStringLiteral("[Channel1]"));
    m_decks[1] = std::make_unique<Deck>(QStringLiteral("[Channel2]"));
    m_timer.setInterval(kTickMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &QmlAutoMix::tick);
    m_status = tr("Auto Mix off");
}

QmlAutoMix::~QmlAutoMix() = default;

QmlAutoMix* QmlAutoMix::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    Q_UNUSED(pJsEngine);
    VERIFY_OR_DEBUG_ASSERT(QmlPlayerManagerProxy::playerManager() && QmlLibraryProxy::get()) {
        qWarning() << "AutoMix: PlayerManager or Library not registered yet";
        return nullptr;
    }
    return new QmlAutoMix(pQmlEngine);
}

void QmlAutoMix::setStatus(const QString& status) {
    if (status == m_status) {
        return;
    }
    m_status = status;
    kLogger.info() << status;
    emit statusChanged(status);
}

QmlAutoMix::Deck* QmlAutoMix::playingDeck() const {
    // The louder playing deck if both play
    Deck* pBest = nullptr;
    for (const auto& pDeck : m_decks) {
        if (pDeck->isPlaying() && (!pBest || pDeck->volume.get() > pBest->volume.get())) {
            pBest = pDeck.get();
        }
    }
    return pBest;
}

QmlAutoMix::Deck* QmlAutoMix::otherDeck(const Deck* pDeck) const {
    return pDeck == m_decks[0].get() ? m_decks[1].get() : m_decks[0].get();
}

void QmlAutoMix::setActive(bool active) {
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (active) {
        m_state = State::Idle;
        m_usedLocations.clear();
        // The crossfader must not hide one of the decks
        m_crossfader.set(0);
        for (const auto& pDeck : m_decks) {
            pDeck->loadedLocation.clear();
            const TrackPointer pTrack = pDeck->track(m_pPlayerManager);
            if (pTrack) {
                m_usedLocations.insert(pTrack->getLocation());
            }
        }
        startPlaybackIfIdle();
        m_timer.start();
        setStatus(tr("Auto Mix on"));
    } else {
        m_timer.stop();
        if (m_state == State::Mixing) {
            // Leave the incoming track playing at full volume
            finishTransition();
        }
        for (const auto& pDeck : m_decks) {
            if (!pDeck->isPlaying()) {
                resetDeckMix(pDeck.get());
            }
        }
        m_state = State::Idle;
        setStatus(tr("Auto Mix off"));
    }
    emit activeChanged(active);
}

void QmlAutoMix::resetDeckMix(Deck* pDeck) {
    pDeck->volume.set(1.0);
    pDeck->eqLow.set(pDeck->eqLow.getDefault());
    pDeck->eqMid.set(pDeck->eqMid.getDefault());
    pDeck->eqHigh.set(pDeck->eqHigh.getDefault());
    for (int i = 0; i < kNumStems; ++i) {
        pDeck->stems[i].set(1.0);
        pDeck->stemMutes[i].set(0);
    }
}

void QmlAutoMix::startPlaybackIfIdle() {
    if (playingDeck()) {
        return;
    }
    // Nothing plays: play a loaded deck, or load a track on deck 1
    for (const auto& pDeck : m_decks) {
        if (pDeck->trackLoaded.toBool()) {
            resetDeckMix(pDeck.get());
            otherDeck(pDeck.get())->volume.set(0);
            pDeck->play.set(1);
            return;
        }
    }
    if (loadNextTrack(nullptr, m_decks[0].get())) {
        m_state = State::WaitingForNext;
    }
}

bool QmlAutoMix::loadNextTrack(Deck* pPlaying, Deck* pNext) {
    struct Choice {
        QString location;
        double score;
        int keyShift;
    };
    QString root;
    double refBpm = 0;
    auto refKey = track::io::key::INVALID;
    if (pPlaying) {
        const TrackPointer pTrack = pPlaying->track(m_pPlayerManager);
        if (pTrack) {
            root = deviceRoot(pTrack->getLocation());
        }
        refBpm = pPlaying->bpm.get();
        refKey = KeyUtils::keyFromNumericValue(pPlaying->key.get());
    }

    Library* pLibrary = QmlLibraryProxy::get();
    QSqlQuery query(pLibrary->trackCollectionManager()->internalCollection()->database());
    query.prepare(QStringLiteral(
            "SELECT track_locations.location, library.bpm, library.key_id "
            "FROM library JOIN track_locations ON library.location=track_locations.id "
            "WHERE library.mixxx_deleted=0 AND track_locations.fs_deleted=0 "
            "AND library.played=0 AND library.bpm>0"));
    if (!query.exec()) {
        kLogger.warning() << "Track query failed" << query.lastError();
        return false;
    }
    QList<Choice> choices;
    while (query.next()) {
        const QString location = query.value(0).toString();
        if (m_usedLocations.contains(location)) {
            continue;
        }
        const double bpm = query.value(1).toDouble();
        const auto key = KeyUtils::keyFromNumericValue(query.value(2).toDouble());
        double score = 0;
        int keyShift = 0;
        if (!root.isEmpty() && !location.startsWith(root)) {
            score += 3; // other folder/drive
        }
        if (refBpm > 0) {
            // Also half/double tempo
            double best = 10;
            for (double factor : {0.5, 1.0, 2.0}) {
                best = std::min(best, std::fabs(refBpm / (bpm * factor) - 1));
            }
            if (best > kMaxTempoDifference) {
                continue;
            }
            score += best * 40; // 1 point every 2.5%
        }
        if (refKey != track::io::key::INVALID) {
            if (key == track::io::key::INVALID) {
                score += 4;
            } else {
                const QList<track::io::key::ChromaticKey> compatible =
                        KeyUtils::getCompatibleKeys(refKey);
                int bestShift = 99;
                for (int shift = -kMaxKeyShift; shift <= kMaxKeyShift; ++shift) {
                    if (compatible.contains(KeyUtils::scaleKeySteps(key, shift)) &&
                            std::abs(shift) < std::abs(bestShift)) {
                        bestShift = shift;
                    }
                }
                if (bestShift == 99) {
                    continue; // never clashing keys
                }
                keyShift = bestShift;
                score += std::abs(bestShift) * 1.5;
                if (KeyUtils::scaleKeySteps(key, bestShift) == refKey) {
                    score -= 0.5; // same key
                }
            }
        }
        // A little variety among equally good tracks
        score += QRandomGenerator::global()->bounded(1.0);
        choices.append({location, score, keyShift});
    }
    if (choices.isEmpty()) {
        setStatus(tr("Auto Mix: no compatible track left"));
        return false;
    }
    const auto best = std::min_element(choices.cbegin(),
            choices.cend(),
            [](const Choice& a, const Choice& b) { return a.score < b.score; });
    m_usedLocations.insert(best->location);
    pNext->loadedLocation = best->location;
    pNext->play.set(0);
    pNext->volume.set(pPlaying ? 0.0 : 1.0);
    resetDeckMix(pNext);
    if (pPlaying) {
        pNext->volume.set(0);
    }
    m_pPlayerManager->slotLoadLocationToPlayer(best->location, pNext->group, false);
    m_pendingKeyShift = best->keyShift;
    kLogger.info() << "Next track" << best->location << "key shift" << best->keyShift
                   << "score" << best->score;
    setStatus(tr("Auto Mix: loading the next track"));
    return true;
}

bool QmlAutoMix::prepareNextDeck(Deck* pPlaying, Deck* pNext) {
    // Wait for the load and the beat grid
    const TrackPointer pTrack = pNext->track(m_pPlayerManager);
    if (!pTrack || pTrack->getLocation() != pNext->loadedLocation ||
            !pNext->trackLoaded.toBool() || pNext->fileBpm.get() <= 0 ||
            pNext->trackSamples.get() <= 0) {
        return false;
    }
    const BeatsPointer pBeats = pTrack->getBeats();
    if (!pBeats) {
        return false;
    }
    // Start point: intro cue, else main cue, else first beat
    double startSamples = pNext->introStart.get();
    if (startSamples <= 0) {
        startSamples = pNext->cuePoint.get();
    }
    double startFrame = startSamples > 0 ? startSamples / 2 : 0;
    if (startFrame <= 0) {
        const auto first = pBeats->firstBeat();
        startFrame = first.isValid() ? first.value() : 0;
    }
    pNext->startFrame = startFrame;
    pNext->keylock.set(1);
    pNext->quantize.set(1);
    pNext->pitchAdjust.set(m_pendingKeyShift);
    pNext->seekToFrame(startFrame);
    if (pPlaying) {
        pPlaying->syncEnabled.set(1);
        pNext->syncEnabled.set(1);
        pNext->volume.set(0);
    } else {
        pNext->volume.set(1);
    }
    const TrackPointer pTrack2 = pNext->track(m_pPlayerManager);
    setStatus(tr("Auto Mix next: %1 – %2 (%3, %4 BPM%5)")
                    .arg(pTrack2->getArtist(),
                            pTrack2->getTitle(),
                            keyText(KeyUtils::keyFromNumericValue(pNext->key.get())),
                            QString::number(pNext->fileBpm.get(), 'f', 1),
                            m_pendingKeyShift
                                    ? tr(", key %1%2")
                                              .arg(m_pendingKeyShift > 0 ? "+" : "")
                                              .arg(m_pendingKeyShift)
                                    : QString()));
    return true;
}

void QmlAutoMix::tick() {
    if (!m_active) {
        return;
    }
    Deck* pPlaying = playingDeck();
    switch (m_state) {
    case State::Idle: {
        if (!pPlaying) {
            startPlaybackIfIdle();
            return;
        }
        if (loadNextTrack(pPlaying, otherDeck(pPlaying))) {
            m_state = State::WaitingForNext;
        }
        return;
    }
    case State::WaitingForNext: {
        Deck* pNext = pPlaying ? otherDeck(pPlaying) : nullptr;
        if (!pPlaying) {
            // First track of the set: start it as soon as it is ready
            Deck* pFirst = m_decks[0].get();
            if (prepareNextDeck(nullptr, pFirst)) {
                pFirst->play.set(1);
                m_state = State::Idle;
            }
            return;
        }
        if (prepareNextDeck(pPlaying, pNext)) {
            m_state = State::Ready;
        }
        return;
    }
    case State::Ready: {
        if (!pPlaying) {
            m_state = State::Idle;
            return;
        }
        Deck* pNext = otherDeck(pPlaying);
        const TrackPointer pTrack = pPlaying->track(m_pPlayerManager);
        const BeatsPointer pBeats = pTrack ? pTrack->getBeats() : BeatsPointer();
        if (!pBeats || pPlaying->bpm.get() <= 0) {
            return;
        }
        const auto sampleRate = pTrack->getSampleRate();
        const double frame = pPlaying->frame();
        const double endFrame = pPlaying->trackSamples.get() / 2;
        const auto firstBeat = pBeats->firstBeat();
        if (!firstBeat.isValid() || !sampleRate.isValid()) {
            return;
        }
        // Mix out at the outro cue, else so that the mix ends at the last beat
        const auto lastBeat = pBeats->findPrevBeat(audio::FramePos(endFrame));
        const double beatFrames = 60.0 / pPlaying->fileBpm.get() * sampleRate.value();
        double mixStartFrame = lastBeat.isValid()
                ? lastBeat.value() - kMixBeats * beatFrames
                : endFrame - kMixBeats * beatFrames;
        const double outro = pPlaying->outroStart.get() / 2;
        if (outro > 0 && outro < mixStartFrame) {
            mixStartFrame = outro;
        }
        const double beat = (frame - firstBeat.value()) / beatFrames;
        const double beatsToMix = (mixStartFrame - frame) / beatFrames;
        // Start on a phrase boundary (16 beats from the first beat) within
        // the 16 beats before the mix point, or right away if too late
        const bool phrase = std::fmod(beat + 1e-3, kPhraseBeats) < 0.25;
        if ((beatsToMix <= kPhraseBeats && phrase && std::floor(beat) != m_lastPhraseBeat) ||
                beatsToMix < -kPhraseBeats) {
            m_lastPhraseBeat = std::floor(beat);
            startTransition(pPlaying, pNext);
        } else if (beatsToMix > 0) {
            // Countdown in the status line
            const int seconds = static_cast<int>(beatsToMix * 60 / pPlaying->bpm.get());
            if (seconds % 5 == 0) {
                const QString prefix = m_status.section(QStringLiteral(" · "), 0, 0);
                setStatus(prefix + tr(" · mix in %1:%2")
                                           .arg(seconds / 60)
                                           .arg(seconds % 60, 2, 10, QChar('0')));
            }
        }
        return;
    }
    case State::Mixing: {
        if (!m_pFrom || !m_pTo) {
            m_state = State::Idle;
            return;
        }
        const TrackPointer pTrack = m_pTo->track(m_pPlayerManager);
        if (!pTrack || !m_pTo->play.toBool()) {
            // Stopped by the DJ
            finishTransition();
            return;
        }
        const auto sampleRate = pTrack->getSampleRate();
        const double beatFrames = 60.0 / m_pTo->fileBpm.get() * sampleRate.value();
        const double beats = (m_pTo->frame() - m_transitionStartFrame) / beatFrames;
        applyTransition(beats);
        if (beats >= m_transitionBeats || !m_pFrom->play.toBool()) {
            finishTransition();
        }
        return;
    }
    }
}

void QmlAutoMix::startTransition(Deck* pPlaying, Deck* pNext) {
    m_pFrom = pPlaying;
    m_pTo = pNext;
    m_stemsFrom = pPlaying->hasStems();
    m_stemsTo = pNext->hasStems();
    m_transitionBeats = kMixBeats;
    m_transitionStartFrame = pNext->startFrame;
    // Start muted (stems/EQ), then the fader; sync + quantize align the beat
    applyTransition(0);
    pNext->play.set(1);
    m_state = State::Mixing;
    setStatus(tr("Auto Mix: mixing (%1)")
                    .arg(m_stemsFrom || m_stemsTo ? tr("stems") : tr("EQ")));
}

void QmlAutoMix::applyTransition(double t) {
    Deck* a = m_pFrom; // outgoing
    Deck* b = m_pTo;   // incoming
    const double eqUnityA = a->eqLow.getDefault();
    const double eqUnityB = b->eqLow.getDefault();
    const double bassSwap = ramp(t, 16, 17);

    // Incoming
    if (m_stemsTo) {
        b->volume.set(1);
        b->stems[kDrums].set(ramp(t, 0, 8));
        b->stems[kMelody].set(ramp(t, 8, 16));
        b->stems[kBass].set(bassSwap);
        b->stems[kVocals].set(ramp(t, 24, 26));
        b->eqLow.set(eqUnityB);
    } else {
        b->volume.set(ramp(t, 0, 8));
        b->eqLow.set(eqUnityB * bassSwap);
    }
    // Outgoing
    if (m_stemsFrom) {
        a->stems[kBass].set(1 - bassSwap);
        a->stems[kDrums].set(1 - ramp(t, 16, 20));
        a->stems[kVocals].set(1 - ramp(t, 20, 24)); // out before the new vocals
        a->stems[kMelody].set(1 - ramp(t, 24, 30));
        a->volume.set(1 - ramp(t, 30, 32));
    } else {
        a->eqLow.set(eqUnityA * (1 - bassSwap));
        a->volume.set(1 - ramp(t, 24, 32));
    }
}

void QmlAutoMix::finishTransition() {
    if (m_pFrom && m_pTo) {
        applyTransition(m_transitionBeats);
        m_pFrom->play.set(0);
        resetDeckMix(m_pFrom);
        m_pFrom->volume.set(0);
        resetDeckMix(m_pTo);
    }
    m_pFrom = nullptr;
    m_pTo = nullptr;
    m_state = State::Idle;
    setStatus(tr("Auto Mix on"));
}

} // namespace qml
} // namespace mixxx
