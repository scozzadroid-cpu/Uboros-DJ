#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace mixxx {
namespace qml {

/// Track analysis, analysis caches and stems for the QML dashboard:
/// batch analysis with progress, the analysis options (BPM, key, stems,
/// USB drives) and the state of the caches, with their cleanup.
class QmlAnalysisProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY progressChanged)
    Q_PROPERTY(int currentTrack READ currentTrack NOTIFY progressChanged)
    Q_PROPERTY(int totalTracks READ totalTracks NOTIFY progressChanged)
    Q_PROPERTY(double trackProgress READ trackProgress NOTIFY progressChanged)
    /// name -> bool, see setOption()
    Q_PROPERTY(QVariantMap options READ options NOTIFY optionsChanged)
    /// "Loading USB drive…" / "Preparing USB drive… N/M", "" when idle
    Q_PROPERTY(QString deviceBusy READ deviceBusy NOTIFY deviceBusyChanged)
    QML_NAMED_ELEMENT(Analysis)
    QML_SINGLETON

  public:
    explicit QmlAnalysisProxy(QObject* parent = nullptr);

    static QmlAnalysisProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isActive() const {
        return m_active;
    }
    int currentTrack() const {
        return m_currentTrack;
    }
    int totalTracks() const {
        return m_totalTracks;
    }
    double trackProgress() const {
        return m_trackProgress;
    }
    QVariantMap options() const;

    /// Queues tracks for analysis. Already analyzed parts are skipped.
    /// scope: "missing" (no BPM or no waveform), "drives" (tracks on USB
    /// drives / SD cards), "all". Returns the number of queued tracks.
    Q_INVOKABLE int analyze(const QString& scope);
    Q_INVOKABLE void stop();
    QString deviceBusy() const {
        return m_deviceBusy;
    }

    /// Options: bpmDetection, bpmFixedTempo, bpmFast, bpmReanalyzeOnChange,
    /// bpmReanalyzeImported, bpmFromDrums, keyDetection, keyFast,
    /// keyReanalyzeOnChange, keyWithoutDrums, devicePreanalysis,
    /// deviceStore, stemAutoReset
    Q_INVOKABLE void setOption(const QString& name, bool value);

    /// Library and cache figures: libraryTracks, tracksWithoutBpm,
    /// tracksWithoutWaveform, waveformBytes, summaryBytes, stemTracks,
    /// stemBytes, drives (list of {label, path, sidecars, bytes, writable}).
    Q_INVOKABLE QVariantMap cacheInfo() const;
    /// Deletes the waveforms cached on the phone: they are rebuilt when a
    /// track is loaded or analyzed (USB drive tracks reimport theirs).
    Q_INVOKABLE bool clearWaveformCache();
    /// Deletes Mixxx's analyses stored on a drive (only <drive>/MIXXX/ANLZ,
    /// never anything else). Refused while a deck plays. Returns the number
    /// of deleted files, -1 when refused.
    Q_INVOKABLE int deleteDriveAnalyses(const QString& drivePath);

  signals:
    void deviceBusyChanged();
    void progressChanged();
    void optionsChanged();
    void cacheChanged();

  private:
    QString m_deviceBusy;
    void onBatchProgress(double currentTrackProgress, int currentTrackNumber, int totalTracksCount);

    bool m_active{false};
    int m_currentTrack{0};
    int m_totalTracks{0};
    double m_trackProgress{0};
};

} // namespace qml
} // namespace mixxx
