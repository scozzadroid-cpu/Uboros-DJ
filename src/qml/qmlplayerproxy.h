#pragma once
#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include "analyzer/analyzerprogress.h"
#include "mixer/basetrackplayer.h"
#include "track/trackid.h"
#include "qmltrackproxy.h"
#include "track/track_decl.h"

namespace mixxx {
namespace qml {

class QmlPlayerProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QmlTrackProxy* currentTrack READ currentTrack NOTIFY trackChanged)
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY trackChanged)
    // A track is being opened (decoder, beats, cues): show "Loading..."
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    // Analysis of the loaded track (waveform, beats, key) in 0..1, or -1
    // when the track is not being analyzed
    Q_PROPERTY(double analysisProgress READ analysisProgress NOTIFY analysisProgressChanged)
    QML_NAMED_ELEMENT(Player)
    QML_UNCREATABLE("Only accessible via Mixxx.PlayerManager.getPlayer(group)")

  public:
    explicit QmlPlayerProxy(BaseTrackPlayer* pTrackPlayer, QObject* parent = nullptr);

    bool isLoaded() const;
    bool isLoading() const {
        return m_isLoading;
    }
    double analysisProgress() const {
        return m_analysisProgress;
    }
    /// Needed for interacting with the raw track player object.
    BaseTrackPlayer* internalTrackPlayer() const {
        return m_pTrackPlayer;
    }

    Q_INVOKABLE void loadTrack(mixxx::qml::QmlTrackProxy* track, bool play = false);
    Q_INVOKABLE void loadTrackFromLocation(const QString& trackLocation, bool play = false);
    Q_INVOKABLE void loadTrackFromLocationUrl(const QUrl& trackLocationUrl, bool play = false);

  public slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotTrackUnloaded(TrackPointer pOldTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotTrackAnalyzerProgress(TrackId trackId, AnalyzerProgress analyzerProgress);

  signals:
    void trackLoading();
    void trackLoaded();
    void trackUnloaded();
    void trackChanged();
    void loadFailed(const QString& reason);
    void isLoadingChanged();
    void analysisProgressChanged();
    void cloneFromGroup(const QString& group);

    void loadTrackFromLocationRequested(const QString& trackLocation, bool play);
    void loadTrackRequested(TrackPointer track,
#ifdef __STEM__
            mixxx::StemChannelSelection stemSelection,
#endif
            bool play);

  private:
    QmlTrackProxy* currentTrack();

    QPointer<BaseTrackPlayer> m_pTrackPlayer;
    void setLoading(bool loading);
    void setAnalysisProgress(double progress);

    TrackPointer m_pCurrentTrack;
    bool m_isLoading{false};
    double m_analysisProgress{-1.0};
};

} // namespace qml
} // namespace mixxx
