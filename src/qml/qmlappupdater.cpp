#include "qml/qmlappupdater.h"

#include <QDateTime>
#include <QLocale>

#include "control/controlobject.h"
#include "moc_qmlappupdater.cpp"
#include "preferences/usersettings.h"
#include "qml/qmlconfigproxy.h"
#include "util/logger.h"

#ifdef __SELF_UPDATE__
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace mixxx {
namespace qml {

namespace {
const mixxx::Logger kLogger("QmlAppUpdater");
const QString kGroup = QStringLiteral("[Update]");
const QString kAutoKey = QStringLiteral("Auto");
const QString kUrlKey = QStringLiteral("Url");
// Default manifest: HTTPS only. [Update],Url overrides it (tests; http allowed then)
const QString kDefaultUrl = QStringLiteral("https://uroboros-web-nine.vercel.app/update.json");
constexpr int kFirstCheckMs = 60 * 1000;
constexpr int kFastPollMs = 500;
constexpr int kIdleStatusMs = 5000;
constexpr int kBlockedRetryMs = 5 * 60 * 1000;
constexpr int kDailyMs = 24 * 60 * 60 * 1000;
constexpr char16_t kSep = 0x1f;

#ifdef __SELF_UPDATE__
QJniObject androidContext() {
    return QNativeInterface::QAndroidApplication::context();
}
#endif
} // namespace

QmlAppUpdater::QmlAppUpdater(QObject* parent)
        : QObject(parent) {
    if (!supported()) {
        return;
    }
#ifdef __SELF_UPDATE__
    QJniObject ctx = androidContext();
    if (ctx.isValid()) {
        QJniObject::callStaticMethod<void>("org/mixxx/AppUpdater",
                "init",
                "(Landroid/content/Context;)V",
                ctx.object());
    }
#endif
    m_pollTimer.setInterval(kFastPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &QmlAppUpdater::poll);
    m_pollTimer.start();
    // Not in the first seconds: the start of the app has other things to do
    QTimer::singleShot(kFirstCheckMs, this, &QmlAppUpdater::autoCheck);
    m_dailyTimer.setInterval(kDailyMs);
    connect(&m_dailyTimer, &QTimer::timeout, this, &QmlAppUpdater::autoCheck);
    m_dailyTimer.start();
    poll();
}

QmlAppUpdater* QmlAppUpdater::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    return new QmlAppUpdater(pQmlEngine);
}

bool QmlAppUpdater::supported() const {
#ifdef __SELF_UPDATE__
    return true;
#else
    return false;
#endif
}

QString QmlAppUpdater::notes() const {
    const bool italian = QLocale::system().language() == QLocale::Italian;
    if (italian && !m_notesIt.isEmpty()) {
        return m_notesIt;
    }
    return m_notesEn.isEmpty() ? m_notesIt : m_notesEn;
}

bool QmlAppUpdater::autoUpdate() const {
    const auto pConfig = QmlConfigProxy::get();
    return !pConfig || pConfig->getValue(ConfigKey(kGroup, kAutoKey), true);
}

void QmlAppUpdater::setAutoUpdate(bool enabled) {
    const auto pConfig = QmlConfigProxy::get();
    if (!pConfig || enabled == autoUpdate()) {
        return;
    }
    pConfig->setValue(ConfigKey(kGroup, kAutoKey), enabled);
    emit autoUpdateChanged();
}

QString QmlAppUpdater::installBlockedBy() const {
    return m_blockedBy;
}

void QmlAppUpdater::autoCheck() {
    if (!autoUpdate()) {
        return;
    }
    if (!m_blockedBy.isEmpty()) {
        // Live use (deck playing / performance mode): no download now, retry later
        QTimer::singleShot(kBlockedRetryMs, this, &QmlAppUpdater::autoCheck);
        return;
    }
    callCheck(true);
}

void QmlAppUpdater::checkNow() {
    callCheck(false);
}

void QmlAppUpdater::callCheck(bool automatic) {
#ifdef __SELF_UPDATE__
    const auto pConfig = QmlConfigProxy::get();
    const QString override = pConfig ? pConfig->getValueString(ConfigKey(kGroup, kUrlKey)) : QString();
    const bool fromConfig = !override.trimmed().isEmpty();
    const QString url = fromConfig ? override.trimmed() : kDefaultUrl;
    kLogger.info() << "checking" << url << (automatic ? "(automatic)" : "(manual)");
    QJniObject ctx = androidContext();
    if (!ctx.isValid()) {
        return;
    }
    QJniObject jUrl = QJniObject::fromString(url);
    QJniObject::callStaticMethod<void>("org/mixxx/AppUpdater",
            "checkNow",
            "(Ljava/lang/String;ZZ)V",
            jUrl.object<jstring>(),
            static_cast<jboolean>(fromConfig),
            static_cast<jboolean>(automatic));
    m_forceStatus = true;
    poll();
#else
    Q_UNUSED(automatic);
#endif
}

void QmlAppUpdater::download() {
#ifdef __SELF_UPDATE__
    QJniObject::callStaticMethod<void>("org/mixxx/AppUpdater", "download", "(Z)V", jboolean(true));
    m_forceStatus = true;
    poll();
#endif
}

void QmlAppUpdater::cancel() {
#ifdef __SELF_UPDATE__
    QJniObject::callStaticMethod<void>("org/mixxx/AppUpdater", "cancel", "()V");
#endif
}

void QmlAppUpdater::install() {
#ifdef __SELF_UPDATE__
    // The installation stops the audio and closes the app
    poll(); // fresh blockedBy
    if (!m_blockedBy.isEmpty()) {
        kLogger.warning() << "install refused:" << m_blockedBy;
        return;
    }
    QJniObject activity = androidContext();
    if (!activity.isValid()) {
        return;
    }
    QJniObject::callStaticMethod<jboolean>("org/mixxx/AppUpdater",
            "install",
            "(Landroid/app/Activity;)Z",
            activity.object());
    m_forceStatus = true;
    poll();
#endif
}

void QmlAppUpdater::poll() {
#ifdef __SELF_UPDATE__
    // Decks playing or performance mode: never install by ourselves
    QString blocked;
    if (auto* pPerf = ControlObject::getControl(
                ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("performance_mode")),
                ControlFlag::NoWarnIfMissing)) {
        if (pPerf->get() > 0) {
            blocked = QStringLiteral("performance");
        }
    }
    for (int i = 1; i <= 8 && blocked.isEmpty(); ++i) {
        auto* pPlay = ControlObject::getControl(
                ConfigKey(QStringLiteral("[Channel%1]").arg(i), QStringLiteral("play")),
                ControlFlag::NoWarnIfMissing);
        if (!pPlay) {
            break;
        }
        if (pPlay->get() > 0) {
            blocked = QStringLiteral("playing");
        }
    }
    bool dirty = blocked != m_blockedBy;
    m_blockedBy = blocked;

    // Binder calls only when something is going on (or every few seconds idle)
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool active = m_state == QLatin1String("checking") ||
            m_state == QLatin1String("downloading") ||
            m_state == QLatin1String("installing");
    if (!m_forceStatus && !active && nowMs - m_lastStatusMs < kIdleStatusMs) {
        if (dirty) {
            emit changed();
        }
        return;
    }
    m_forceStatus = false;
    m_lastStatusMs = nowMs;
    QJniObject res = QJniObject::callStaticObjectMethod("org/mixxx/AppUpdater",
            "status",
            "()Ljava/lang/String;");
    if (res.isValid()) {
        const QString raw = res.toString();
        if (raw != m_lastRaw) {
            m_lastRaw = raw;
            const QStringList f = raw.split(QChar(kSep));
            if (f.size() >= 14) {
                m_state = f[0];
                m_progress = f[1].toDouble();
                m_availableVersion = f[2];
                m_availableCode = f[3].toInt();
                m_notesEn = f[4];
                m_notesIt = f[5];
                m_errorCode = f[6];
                m_errorDetail = f[7];
                m_lastCheck = f[8].toDouble();
                m_meteredBlocked = f[9] == QLatin1String("1");
                m_size = f[10].toLongLong();
                m_canInstall = f[11] == QLatin1String("1");
                m_installed = QStringLiteral("%1 (%2)").arg(f[12], f[13]);
                dirty = true;
            }
        }
    }
    if (dirty) {
        emit changed();
    }
#endif
}

} // namespace qml
} // namespace mixxx
