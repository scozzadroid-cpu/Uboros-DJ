#pragma once

#include <QFutureWatcher>
#include <QPainter>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QVariantList>

#include "library/rekordbox/rekordboxfeature.h"
#include "qmltrackproxy.h"
#include "track/track.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace qml {

class QmlWaveformOverview : public QQuickPaintedItem {
    Q_OBJECT
    Q_FLAGS(Channels)
    Q_PROPERTY(mixxx::qml::QmlTrackProxy* track READ getTrack WRITE setTrack
                    NOTIFY trackChanged REQUIRED)
    Q_PROPERTY(Channels channels READ getChannels WRITE setChannels NOTIFY channelsChanged)
    Q_PROPERTY(Renderer renderer MEMBER m_renderer NOTIFY rendererChanged)
    Q_PROPERTY(QColor colorHigh MEMBER m_colorHigh NOTIFY colorHighChanged)
    Q_PROPERTY(QColor colorMid MEMBER m_colorMid NOTIFY colorMidChanged)
    Q_PROPERTY(QColor colorLow MEMBER m_colorLow NOTIFY colorLowChanged)
    QML_NAMED_ELEMENT(WaveformOverview)

  public:
    enum class ChannelFlag : int {
        LeftChannel = 1,
        RightChannel = 2,
        BothChannels = LeftChannel | RightChannel,
    };
    Q_DECLARE_FLAGS(Channels, ChannelFlag)

    enum class Renderer {
        RGB = 1,
        Filtered = 2,
    };
    Q_ENUM(Renderer)

    QmlWaveformOverview(QQuickItem* parent = nullptr);
    ~QmlWaveformOverview() override = default;

    void paint(QPainter* painter) override;

    void setTrack(QmlTrackProxy* track);
    QmlTrackProxy* getTrack() const;

    void setChannels(Channels channels);
    Channels getChannels() const;
  private slots:
    void slotWaveformUpdated();

  signals:
    void trackChanged();
    void channelsChanged(mixxx::qml::QmlWaveformOverview::Channels channels);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    void rendererChanged(Renderer renderer);
#else
    void rendererChanged(mixxx::qml::QmlWaveformOverview::Renderer renderer);
#endif
    void colorHighChanged(const QColor& color);
    void colorMidChanged(const QColor& color);
    void colorLowChanged(const QColor& color);

  private:
    void drawFiltered(QPainter* pPainter,
            Channels channels,
            ConstWaveformPointer pWaveform,
            int completion) const;
    void drawRgb(QPainter* pPainter,
            Channels channels,
            ConstWaveformPointer pWaveform,
            int completion) const;
    QColor getRgbPenColor(ConstWaveformPointer pWaveform, int completion) const;
    QmlTrackProxy* m_pTrack;
    Channels m_channels;
    Renderer m_renderer;
    QColor m_colorHigh;
    QColor m_colorMid;
    QColor m_colorLow;
};

/// What the overview of a deck draws, built once when the track is loaded
/// or its cues change (never per frame): the hot cues with their color and
/// name and, when asked, the phrases (song structure) that rekordbox stored
/// with the track. The QML interface draws one static item for each.
class QmlTrackMarks : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlTrackProxy* track READ getTrack WRITE setTrack
                    NOTIFY trackChanged)
    // Read the phrases of the track too (they are only shown by one interface
    // profile, the others do not pay for them)
    Q_PROPERTY(bool readPhrases READ readPhrases WRITE setReadPhrases NOTIFY readPhrasesChanged)
    // [{index, time, end, color, label, isLoop}], one for each hot cue: index
    // is the pad number from 0 (A = 0), time and end are seconds (end is -1
    // for a cue that is not a loop), color is the one of the cue
    Q_PROPERTY(QVariantList cues READ cues NOTIFY cuesChanged)
    // [{kind, number, mood, start, end}] in seconds (end 0 = to the end of
    // the track), kind is "intro", "verse", "bridge", "chorus", "outro", "up"
    // or "down"; empty when the track has none
    Q_PROPERTY(QVariantList phrases READ phrases NOTIFY phrasesChanged)
    QML_NAMED_ELEMENT(TrackMarks)

  public:
    explicit QmlTrackMarks(QObject* parent = nullptr);
    ~QmlTrackMarks() override;

    QmlTrackProxy* getTrack() const {
        return m_pTrack;
    }
    void setTrack(QmlTrackProxy* pTrack);

    bool readPhrases() const {
        return m_readPhrases;
    }
    void setReadPhrases(bool read);

    QVariantList cues() const {
        return m_cues;
    }
    QVariantList phrases() const {
        return m_phrases;
    }

  signals:
    void trackChanged();
    void readPhrasesChanged();
    void cuesChanged();
    void phrasesChanged();

  private slots:
    void slotCuesUpdated();
    void slotPhrasesRead();
    void slotRekordboxBusyChanged(const QString& busyText);

  private:
    void rebuildCues();
    void requestPhrases();

    QPointer<QmlTrackProxy> m_pTrack;
    // Its Track, to disconnect when the proxy is already gone
    QPointer<Track> m_pTrackObject;
    bool m_readPhrases{false};
    QVariantList m_cues;
    QVariantList m_phrases;
    // A phrase request is waiting for the drive to be read (the track is not
    // in the rekordbox table yet)
    bool m_waitingForDrive{false};
    // Results of a request made for another track are dropped
    int m_phrasesGeneration{0};
    int m_watcherGeneration{0};
    bool m_connectedToRekordbox{false};
    QFutureWatcher<QList<RekordboxPhrase>> m_phrasesWatcher;
};

} // namespace qml
} // namespace mixxx
