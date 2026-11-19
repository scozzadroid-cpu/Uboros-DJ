#include "qml/qmldeckprefsproxy.h"

#include <algorithm>

#include "control/controlobject.h"
#include "mixer/playermanager.h"
#include "moc_qmldeckprefsproxy.cpp"

namespace mixxx {
namespace qml {

namespace {
const QString kControls = QStringLiteral("[Controls]");
constexpr int kDefaultRateRangePercent = 8;   // as the desktop dialog
constexpr int kDefaultSpeedAutoReset = 1;     // RESET_PITCH (basetrackplayer.h)
constexpr int kDefaultLoadPoint = 3;          // intro start (cuecontrol.cpp)
constexpr int kRateRanges[] = {4, 6, 8, 10, 16, 24, 50, 90};
} // namespace

QmlDeckPrefsProxy::QmlDeckPrefsProxy(UserSettingsPointer pConfig, QObject* parent)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_pNumDecks(std::make_unique<ControlProxy>(
                  ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks")), this)) {
    s_pInstance = this;
    // New decks (4 decks on a tablet/PC) get the saved values too
    m_pNumDecks->connectValueChanged(this, [this](double) { applySaved(); });
    applySaved();
}

QmlDeckPrefsProxy::~QmlDeckPrefsProxy() {
    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }
}

QmlDeckPrefsProxy* QmlDeckPrefsProxy::create(QQmlEngine*, QJSEngine*) {
    // Built by QmlApplication before the QML engine loads: owned by C++
    if (s_pInstance) {
        QQmlEngine::setObjectOwnership(s_pInstance, QQmlEngine::CppOwnership);
    }
    return s_pInstance;
}

int QmlDeckPrefsProxy::value(const char* item, int defaultValue) const {
    return m_pConfig->getValue(ConfigKey(kControls, QString::fromLatin1(item)), defaultValue);
}

void QmlDeckPrefsProxy::store(const char* item, int value) {
    m_pConfig->setValue(ConfigKey(kControls, QString::fromLatin1(item)), value);
    emit changed();
}

void QmlDeckPrefsProxy::applyToDecks(const char* control, double value) {
    const int decks = static_cast<int>(m_pNumDecks->get());
    for (int deck = 0; deck < decks; ++deck) {
        ControlObject::set(ConfigKey(PlayerManager::groupForDeck(deck), QString::fromLatin1(control)), value);
    }
}

void QmlDeckPrefsProxy::applySaved() {
    // Only what the user has saved: without a key the control keeps its own default
    const auto exists = [this](const char* item) {
        return m_pConfig->exists(ConfigKey(kControls, QString::fromLatin1(item)));
    };
    if (exists("CueDefault")) {
        applyToDecks("cue_mode", cueMode());
    }
    if (exists("keylockMode")) {
        applyToDecks("keylockMode", keylockMode());
    }
    if (exists("keyunlockMode")) {
        applyToDecks("keyunlockMode", keyunlockMode());
    }
    if (exists("RateRangePercent")) {
        applyToDecks("rateRange", rateRangePercent() / 100.0);
    }
}

int QmlDeckPrefsProxy::cueMode() const {
    return std::clamp(value("CueDefault", 0), 0, 5);
}
int QmlDeckPrefsProxy::keylockMode() const {
    return std::clamp(value("keylockMode", 0), 0, 1);
}
int QmlDeckPrefsProxy::keyunlockMode() const {
    return std::clamp(value("keyunlockMode", 0), 0, 1);
}
int QmlDeckPrefsProxy::rateRangePercent() const {
    const int percent = value("RateRangePercent", kDefaultRateRangePercent);
    return percent > 0 && percent <= 90 ? percent : kDefaultRateRangePercent;
}
int QmlDeckPrefsProxy::speedAutoReset() const {
    return std::clamp(value("SpeedAutoReset", kDefaultSpeedAutoReset), 0, 3);
}
int QmlDeckPrefsProxy::loadPoint() const {
    return std::clamp(value("CueRecall", kDefaultLoadPoint), 0, 4);
}

void QmlDeckPrefsProxy::setCueMode(int mode) {
    mode = std::clamp(mode, 0, 5);
    store("CueDefault", mode);
    applyToDecks("cue_mode", mode);
}
void QmlDeckPrefsProxy::setKeylockMode(int mode) {
    mode = std::clamp(mode, 0, 1);
    store("keylockMode", mode);
    applyToDecks("keylockMode", mode);
}
void QmlDeckPrefsProxy::setKeyunlockMode(int mode) {
    mode = std::clamp(mode, 0, 1);
    store("keyunlockMode", mode);
    applyToDecks("keyunlockMode", mode);
}
void QmlDeckPrefsProxy::setRateRangePercent(int percent) {
    if (std::find(std::begin(kRateRanges), std::end(kRateRanges), percent) == std::end(kRateRanges)) {
        return;
    }
    store("RateRangePercent", percent);
    applyToDecks("rateRange", percent / 100.0);
}
void QmlDeckPrefsProxy::setSpeedAutoReset(int mode) {
    store("SpeedAutoReset", std::clamp(mode, 0, 3));
}
void QmlDeckPrefsProxy::setLoadPoint(int mode) {
    store("CueRecall", std::clamp(mode, 0, 4));
}

} // namespace qml
} // namespace mixxx
