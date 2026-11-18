#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

/// Export of the Mixxx library for Pioneer / AlphaTheta gear.
///
/// exportXml() writes a rekordbox XML collection (tracks, beat grid start,
/// hot cues, loops, crates and playlists). Rekordbox imports it
/// (File > Library > Import) and writes the USB stick for CDJ/XDJ players
/// itself, with the analysis files the players need. Optionally the audio
/// files are copied next to the XML (a "Contents" folder), so that the
/// export folder or the USB stick is self-contained.
class QmlRekordboxExport : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    QML_NAMED_ELEMENT(RekordboxExport)
    QML_SINGLETON

  public:
    explicit QmlRekordboxExport(QObject* parent = nullptr);

    static QmlRekordboxExport* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QString status() const {
        return m_status;
    }
    bool busy() const {
        return m_busy;
    }

    /// destination: folder (the file rekordbox.xml is created in it, and
    /// "Contents" when copyFiles is set). crateName: only that crate or
    /// playlist, empty for the whole library. Returns the number of tracks.
    Q_INVOKABLE int exportXml(const QString& destination,
            bool copyFiles,
            const QString& setName = QString());

    /// Names of crates and playlists, for the chooser
    Q_INVOKABLE QStringList setNames() const;

  signals:
    void statusChanged(const QString& status);
    void busyChanged(bool busy);

  private:
    void setStatus(const QString& status);

    QString m_status;
    bool m_busy = false;
};
