#include "qml/qmlwaveformoverview.h"

#include <QThreadPool>
#include <QtConcurrentRun>
#include <algorithm>

#include "moc_qmlwaveformoverview.cpp"
#include "qmlplayerproxy.h"
#include "qmltrackproxy.h"
#include "track/cue.h"
#include "track/track.h"

namespace {
constexpr double kDesiredChannelHeight = 255;

/// The phrases are read by one background thread of its own: the priority of
/// a thread of the global pool cannot be given back
QThreadPool* phrasesPool() {
    static QThreadPool* const pPool = [] {
        auto* pNewPool = new QThreadPool();
        pNewPool->setMaxThreadCount(1);
        pNewPool->setThreadPriority(QThread::LowPriority);
        return pNewPool;
    }();
    return pPool;
}

QString phraseKindName(RekordboxPhrase::Kind kind) {
    switch (kind) {
    case RekordboxPhrase::Kind::Intro:
        return QStringLiteral("intro");
    case RekordboxPhrase::Kind::Verse:
        return QStringLiteral("verse");
    case RekordboxPhrase::Kind::Bridge:
        return QStringLiteral("bridge");
    case RekordboxPhrase::Kind::Chorus:
        return QStringLiteral("chorus");
    case RekordboxPhrase::Kind::Outro:
        return QStringLiteral("outro");
    case RekordboxPhrase::Kind::Up:
        return QStringLiteral("up");
    case RekordboxPhrase::Kind::Down:
        return QStringLiteral("down");
    }
    return QString();
}
} // namespace

namespace mixxx {
namespace qml {

QmlWaveformOverview::QmlWaveformOverview(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_pTrack(nullptr),
          m_channels(ChannelFlag::BothChannels),
          m_renderer(Renderer::RGB),
          m_colorHigh(0xFF0000),
          m_colorMid(0x00FF00),
          m_colorLow(0x0000FF) {
    // Colors, renderer and channels change what is painted: repaint at once
    // (a resize is repainted by QQuickPaintedItem itself). Summary updates
    // during analysis are only two (start and end, analyzerwaveform.cpp), so
    // they need no throttling.
    connect(this, &QmlWaveformOverview::colorHighChanged, this, [this]() { update(); });
    connect(this, &QmlWaveformOverview::colorMidChanged, this, [this]() { update(); });
    connect(this, &QmlWaveformOverview::colorLowChanged, this, [this]() { update(); });
    connect(this, &QmlWaveformOverview::rendererChanged, this, [this]() { update(); });
    connect(this, &QmlWaveformOverview::channelsChanged, this, [this]() { update(); });
}

QmlTrackProxy* QmlWaveformOverview::getTrack() const {
    return m_pTrack;
}

void QmlWaveformOverview::setTrack(QmlTrackProxy* pTrack) {
    if (m_pTrack == pTrack) {
        return;
    }

    if (m_pTrack != nullptr && m_pTrack->internal() != nullptr) {
        m_pTrack->internal()->disconnect(this);
    }

    m_pTrack = pTrack;

    if (m_pTrack != nullptr && pTrack->internal() != nullptr) {
        connect(pTrack->internal().get(),
                &Track::waveformSummaryUpdated,
                this,
                &QmlWaveformOverview::slotWaveformUpdated);
    }
    slotWaveformUpdated();
}

QmlWaveformOverview::Channels QmlWaveformOverview::getChannels() const {
    return m_channels;
}

void QmlWaveformOverview::setChannels(QmlWaveformOverview::Channels channels) {
    if (m_channels == channels) {
        return;
    }

    m_channels = channels;
    emit channelsChanged(channels);
}

void QmlWaveformOverview::slotWaveformUpdated() {
    update();
}

void QmlWaveformOverview::paint(QPainter* pPainter) {
    if (!m_pTrack) {
        return;
    }
    TrackPointer pTrack = m_pTrack->internal();
    if (!pTrack) {
        return;
    }

    ConstWaveformPointer pWaveform = pTrack->getWaveformSummary();
    if (!pWaveform) {
        return;
    }

    const int dataSize = pWaveform->getDataSize();
    if (dataSize == 0) {
        return;
    }

    constexpr int actualCompletion = 0;
    // Always multiple of 2
    const int waveformCompletion = pWaveform->getCompletion();
    // Test if there is some new to draw (at least of pixel width)
    const int completionIncrement = waveformCompletion - actualCompletion;

    const qreal desiredWidth = static_cast<qreal>(dataSize) / 2;
    const double visiblePixelIncrement = completionIncrement * desiredWidth / dataSize;
    if (waveformCompletion < (dataSize - 2) &&
            (completionIncrement < 2 || visiblePixelIncrement == 0)) {
        return;
    }

    const int nextCompletion = actualCompletion + completionIncrement;

    const Channels channels = m_channels;
    pPainter->save();

    switch (channels) {
    case static_cast<int>(ChannelFlag::LeftChannel):
        // Draw both channels.
        // Set the y axis to half the height of the item
        pPainter->translate(0.0, height());
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / kDesiredChannelHeight);
        break;
    case static_cast<int>(ChannelFlag::RightChannel):
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / kDesiredChannelHeight);
        break;
    default:
        // Draw both channels.
        // Set the y axis to half the height of the item
        pPainter->translate(0.0, height() / 2);
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / (2 * kDesiredChannelHeight));
    }

    Renderer renderer = m_renderer;
    for (int currentCompletion = actualCompletion;
            currentCompletion < nextCompletion;
            currentCompletion += 2) {
        switch (renderer) {
        case Renderer::Filtered:
            drawFiltered(pPainter, channels, pWaveform, currentCompletion);
            break;
        default:
            drawRgb(pPainter, channels, pWaveform, currentCompletion);
        }
    }
    pPainter->restore();
}

void QmlWaveformOverview::drawRgb(QPainter* pPainter,
        Channels channels,
        ConstWaveformPointer pWaveform,
        int completion) const {
    const double offsetX = completion / 2.0;

    if (channels.testFlag(ChannelFlag::LeftChannel)) {
        // Draw left channel
        const QColor leftColor = getRgbPenColor(pWaveform, completion);
        if (leftColor.isValid()) {
            const uint8_t leftValue = pWaveform->getAll(completion);
            pPainter->setPen(leftColor);
            pPainter->drawLine(QPointF(offsetX, -leftValue), QPointF(offsetX, 0.0));
        }
    }

    if (channels.testFlag(ChannelFlag::RightChannel)) {
        // Draw right channel
        QColor rightColor = getRgbPenColor(pWaveform, completion + 1);
        if (rightColor.isValid()) {
            const uint8_t rightValue = pWaveform->getAll(completion + 1);
            pPainter->setPen(rightColor);
            pPainter->drawLine(QPointF(offsetX, 0.0), QPointF(offsetX, rightValue));
        }
    }
}

void QmlWaveformOverview::drawFiltered(QPainter* pPainter,
        Channels channels,
        ConstWaveformPointer pWaveform,
        int completion) const {
    const double offsetX = completion / 2.0;

    if (channels.testFlag(ChannelFlag::LeftChannel)) {
        const uint8_t leftHigh = pWaveform->getHigh(completion);
        pPainter->setPen(m_colorHigh);
        pPainter->drawLine(QPointF(offsetX, 2 * -leftHigh), QPointF(offsetX, 0.0));

        const uint8_t leftMid = pWaveform->getMid(completion);
        pPainter->setPen(m_colorMid);
        pPainter->drawLine(QPointF(offsetX, 1.5 * -leftMid), QPointF(offsetX, 0.0));

        const uint8_t leftLow = pWaveform->getLow(completion);
        pPainter->setPen(m_colorLow);
        pPainter->drawLine(QPointF(offsetX, -leftLow), QPointF(offsetX, 0.0));
    }

    if (channels.testFlag(ChannelFlag::RightChannel)) {
        const uint8_t rightHigh = pWaveform->getHigh(completion + 1);
        pPainter->setPen(m_colorHigh);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, 2 * rightHigh));

        const uint8_t rightMid = pWaveform->getMid(completion + 1) * 2;
        pPainter->setPen(m_colorMid);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, 1.5 * rightMid));

        const uint8_t rightLow = pWaveform->getLow(completion + 1);
        pPainter->setPen(m_colorLow);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, rightLow));
    }
}

QColor QmlWaveformOverview::getRgbPenColor(ConstWaveformPointer pWaveform, int completion) const {
    // Retrieve "raw" LMH values from waveform
    qreal low = static_cast<qreal>(pWaveform->getLow(completion));
    qreal mid = static_cast<qreal>(pWaveform->getMid(completion));
    qreal high = static_cast<qreal>(pWaveform->getHigh(completion));

    // Do matrix multiplication
    qreal red = low * m_colorLow.redF() + mid * m_colorMid.redF() + high * m_colorHigh.redF();
    qreal green = low * m_colorLow.greenF() + mid * m_colorMid.greenF() +
            high * m_colorHigh.greenF();
    qreal blue = low * m_colorLow.blueF() + mid * m_colorMid.blueF() + high * m_colorHigh.blueF();

    // Normalize and draw
    qreal max = math_max3(red, green, blue);
    if (max > 0.0) {
        QColor color;
        color.setRgbF(
                static_cast<float>(red / max),
                static_cast<float>(green / max),
                static_cast<float>(blue / max));
        return color;
    }
    return QColor();
}

QmlTrackMarks::QmlTrackMarks(QObject* parent)
        : QObject(parent) {
    connect(&m_phrasesWatcher,
            &QFutureWatcher<QList<RekordboxPhrase>>::finished,
            this,
            &QmlTrackMarks::slotPhrasesRead);
}

QmlTrackMarks::~QmlTrackMarks() = default;

void QmlTrackMarks::setTrack(QmlTrackProxy* pTrack) {
    if (m_pTrack == pTrack) {
        return;
    }
    if (m_pTrackObject) {
        m_pTrackObject->disconnect(this);
    }
    m_pTrack = pTrack;
    m_pTrackObject = pTrack ? pTrack->internal().get() : nullptr;
    if (m_pTrackObject) {
        connect(m_pTrackObject.data(),
                &Track::cuesUpdated,
                this,
                &QmlTrackMarks::slotCuesUpdated);
    }
    emit trackChanged();
    rebuildCues();
    requestPhrases();
}

void QmlTrackMarks::setReadPhrases(bool read) {
    if (m_readPhrases == read) {
        return;
    }
    m_readPhrases = read;
    emit readPhrasesChanged();
    requestPhrases();
}

void QmlTrackMarks::slotCuesUpdated() {
    rebuildCues();
}

void QmlTrackMarks::rebuildCues() {
    QVariantList cues;
    if (m_pTrackObject) {
        const QList<CuePointer> cuePoints = m_pTrackObject->getCuePoints();
        for (const CuePointer& pCue : cuePoints) {
            const int hotCue = pCue->getHotCue();
            const CueType type = pCue->getType();
            if (hotCue < 0 || (type != CueType::HotCue && type != CueType::Loop)) {
                continue;
            }
            const auto position = pCue->getPosition();
            if (!position.isValid()) {
                continue;
            }
            const auto endPosition = pCue->getEndPosition();
            QVariantMap cue;
            cue.insert(QStringLiteral("index"), hotCue);
            // Engine samples (2 per frame), as the hotcue_N_position controls,
            // for the track_samples control to be the whole track
            cue.insert(QStringLiteral("pos"), position.toEngineSamplePos());
            cue.insert(QStringLiteral("endPos"),
                    type == CueType::Loop && endPosition.isValid()
                            ? endPosition.toEngineSamplePos()
                            : -1.0);
            cue.insert(QStringLiteral("color"), RgbColor::toQColor(pCue->getColor()));
            cue.insert(QStringLiteral("label"), pCue->getLabel());
            cue.insert(QStringLiteral("isLoop"), type == CueType::Loop);
            cues.append(cue);
        }
        std::sort(cues.begin(), cues.end(), [](const QVariant& a, const QVariant& b) {
            return a.toMap().value(QStringLiteral("index")).toInt() <
                    b.toMap().value(QStringLiteral("index")).toInt();
        });
    }
    if (cues == m_cues) {
        return;
    }
    m_cues = cues;
    emit cuesChanged();
}

void QmlTrackMarks::requestPhrases() {
    ++m_phrasesGeneration;
    m_waitingForDrive = false;
    if (!m_phrases.isEmpty()) {
        m_phrases.clear();
        emit phrasesChanged();
    }
    if (!m_readPhrases || !m_pTrackObject) {
        return;
    }
    RekordboxFeature* pFeature = RekordboxFeature::instance();
    if (!pFeature) {
        return;
    }
    if (!m_connectedToRekordbox) {
        connect(pFeature,
                &RekordboxFeature::busyTextChanged,
                this,
                &QmlTrackMarks::slotRekordboxBusyChanged);
        m_connectedToRekordbox = true;
    }
    const QString anlzPath = pFeature->analyzePathForLocation(m_pTrackObject->getLocation());
    if (anlzPath.isEmpty()) {
        // Not a track of a drive (or the drive is still being read: asked
        // again when the feature has finished)
        m_waitingForDrive = true;
        return;
    }
    m_watcherGeneration = m_phrasesGeneration;
    m_phrasesWatcher.setFuture(
            QtConcurrent::run(phrasesPool(), &readRekordboxPhrases, anlzPath));
}

void QmlTrackMarks::slotRekordboxBusyChanged(const QString& busyText) {
    if (m_waitingForDrive && busyText.isEmpty()) {
        requestPhrases();
    }
}

void QmlTrackMarks::slotPhrasesRead() {
    // The track changed since the request
    if (m_watcherGeneration != m_phrasesGeneration) {
        return;
    }
    const QList<RekordboxPhrase> phrases = m_phrasesWatcher.result();
    QVariantList list;
    for (const RekordboxPhrase& phrase : phrases) {
        QVariantMap map;
        map.insert(QStringLiteral("kind"), phraseKindName(phrase.kind));
        map.insert(QStringLiteral("number"), phrase.number);
        map.insert(QStringLiteral("mood"), phrase.mood);
        map.insert(QStringLiteral("start"), phrase.startSec);
        map.insert(QStringLiteral("end"), phrase.endSec);
        list.append(map);
    }
    if (list == m_phrases) {
        return;
    }
    m_phrases = list;
    emit phrasesChanged();
}

} // namespace qml
} // namespace mixxx
