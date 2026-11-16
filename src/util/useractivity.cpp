#include "util/useractivity.h"

#include "moc_useractivity.cpp"

namespace mixxx {

// static
UserActivity* UserActivity::instance() {
    UserActivity* pInstance = s_pInstance.load(std::memory_order_acquire);
    if (!pInstance) {
        // Lives as long as the application, on the thread that creates it
        pInstance = new UserActivity();
        s_pInstance.store(pInstance, std::memory_order_release);
    }
    return pInstance;
}

} // namespace mixxx
