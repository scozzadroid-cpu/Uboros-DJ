#pragma once

#include <QObject>
#include <atomic>

namespace mixxx {

/// Tells the GUI that the user is active (a DJ controller sends MIDI, the
/// screen is touched), e.g. to wake the screen that the "screen off when
/// idle" option turned black (Android).
///
/// notify() may be called from any thread (the controller thread calls it at
/// most once per second, see Controller::triggerActivity()): the signal
/// reaches receivers on the GUI thread through a queued connection.
class UserActivity : public QObject {
    Q_OBJECT
  public:
    /// Create it on the GUI thread before any notify()
    static UserActivity* instance();

    static void notify() {
        if (UserActivity* pInstance = s_pInstance.load(std::memory_order_acquire)) {
            emit pInstance->activity();
        }
    }

  signals:
    void activity();

  private:
    explicit UserActivity(QObject* parent = nullptr)
            : QObject(parent) {
    }

    static inline std::atomic<UserActivity*> s_pInstance = nullptr;
};

} // namespace mixxx
