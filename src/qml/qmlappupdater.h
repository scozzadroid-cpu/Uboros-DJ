#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

namespace mixxx {
namespace qml {

/// Over-the-air update of the Android app for QML (Mixxx.AppUpdater).
/// The work is done by org.mixxx.AppUpdater (Java); this object polls its
/// state, runs the automatic check (about 60 s after the start, then every
/// 24 h) and refuses to install while a deck plays or in performance mode.
/// A no-op (state "idle", supported false) without __SELF_UPDATE__.
class QmlAppUpdater : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppUpdater)
    QML_SINGLETON
    Q_PROPERTY(bool supported READ supported CONSTANT)
    /// idle, checking, available, downloading, ready, installing, error
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(int availableCode READ availableCode NOTIFY changed)
    /// release notes: Italian on an Italian system, English otherwise
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(QString notesEn READ notesEn NOTIFY changed)
    Q_PROPERTY(QString notesIt READ notesIt NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(double sizeMb READ sizeMb NOTIFY changed)
    /// "1.1 (10)"
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY changed)
    /// ms since the epoch of the last check, 0 = never
    Q_PROPERTY(double lastCheck READ lastCheck NOTIFY changed)
    /// network, check, manifest, download, hash, missing, install, permission, busy
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)
    /// a new version is waiting for Wi-Fi (metered network)
    Q_PROPERTY(bool meteredBlocked READ meteredBlocked NOTIFY changed)
    Q_PROPERTY(bool canInstallPackages READ canInstallPackages NOTIFY changed)
    /// "" when installing is allowed, else "playing" or "performance"
    Q_PROPERTY(QString installBlockedBy READ installBlockedBy NOTIFY changed)
    /// [Update],Auto (default on)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)

  public:
    explicit QmlAppUpdater(QObject* parent = nullptr);
    static QmlAppUpdater* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool supported() const;
    QString state() const { return m_state; }
    QString availableVersion() const { return m_availableVersion; }
    int availableCode() const { return m_availableCode; }
    QString notes() const;
    QString notesEn() const { return m_notesEn; }
    QString notesIt() const { return m_notesIt; }
    double progress() const { return m_progress; }
    double sizeMb() const { return m_size / (1024.0 * 1024.0); }
    QString installedVersion() const { return m_installed; }
    double lastCheck() const { return m_lastCheck; }
    QString errorCode() const { return m_errorCode; }
    QString errorDetail() const { return m_errorDetail; }
    bool meteredBlocked() const { return m_meteredBlocked; }
    bool canInstallPackages() const { return m_canInstall; }
    QString installBlockedBy() const;
    bool autoUpdate() const;
    void setAutoUpdate(bool enabled);

    /// Checks the website now (also with the automatic updates off)
    Q_INVOKABLE void checkNow();
    /// Downloads the new version, also on mobile data (explicit request)
    Q_INVOKABLE void download();
    Q_INVOKABLE void cancel();
    /// Installs the downloaded version: closes the app. Refused while a deck
    /// plays or in performance mode (errorCode "busy")
    Q_INVOKABLE void install();

  signals:
    void changed();
    void autoUpdateChanged();

  private:
    void poll();
    void autoCheck();
    void callCheck(bool automatic);

    QTimer m_pollTimer;
    QTimer m_dailyTimer;
    QString m_state = QStringLiteral("idle");
    QString m_availableVersion;
    int m_availableCode = 0;
    QString m_notesEn;
    QString m_notesIt;
    double m_progress = 0;
    qint64 m_size = 0;
    QString m_installed;
    double m_lastCheck = 0;
    QString m_errorCode;
    QString m_errorDetail;
    bool m_meteredBlocked = false;
    bool m_canInstall = true;
    QString m_blockedBy;
    QString m_lastRaw;
    // The binder calls of status() are slow-polled while idle
    qint64 m_lastStatusMs = 0;
    bool m_forceStatus = true;
};

} // namespace qml
} // namespace mixxx
