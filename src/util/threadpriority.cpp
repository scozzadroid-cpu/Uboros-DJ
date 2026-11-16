#include "util/threadpriority.h"

#include <atomic>

#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>

#include <cerrno>
#include <cstring>
#endif

#include "util/logger.h"

namespace mixxx {

namespace {

const Logger kLogger("ThreadPriority");

#ifdef __linux__
// ioprio_set(2) has no libc wrapper. IOPRIO_WHO_PROCESS with who == 0 is the
// calling thread, the same as for setpriority(PRIO_PROCESS, 0, ...).
constexpr int kIoprioWhoProcess = 1;
constexpr int kIoprioClassShift = 13;
constexpr int kIoprioClassBestEffort = 2;
constexpr int kIoprioBestEffortLowest = 7;

std::atomic<bool> s_loggedSuccess{false};
std::atomic<bool> s_loggedFailure{false};
#endif

} // namespace

bool demoteCurrentThreadToBackground(int nice, const char* threadName) {
#ifdef __linux__
    // The policy first: sched_setscheduler() keeps the current nice value,
    // which is then lowered independently of whether SCHED_BATCH took.
    sched_param param{};
    param.sched_priority = 0; // SCHED_BATCH has no static priorities
    int batchErr = 0;
    if (sched_setscheduler(0, SCHED_BATCH, &param) != 0) {
        batchErr = errno;
    }
    int niceErr = 0;
    if (setpriority(PRIO_PROCESS, 0, nice) != 0) {
        niceErr = errno;
    }
    constexpr int ioprio = (kIoprioClassBestEffort << kIoprioClassShift) |
            kIoprioBestEffortLowest;
    int ioErr = 0;
    if (syscall(SYS_ioprio_set, kIoprioWhoProcess, 0, ioprio) != 0) {
        ioErr = errno;
    }

    const bool ok = batchErr == 0 || niceErr == 0;
    if (batchErr == 0 && niceErr == 0 && ioErr == 0) {
        if (!s_loggedSuccess.exchange(true, std::memory_order_relaxed)) {
            kLogger.info() << "Background thread" << threadName
                           << "scheduled with SCHED_BATCH, nice" << nice
                           << ", I/O best-effort" << kIoprioBestEffortLowest;
        }
    } else if (!s_loggedFailure.exchange(true, std::memory_order_relaxed)) {
        kLogger.warning() << "Background thread" << threadName
                          << "only partially demoted:"
                          << "SCHED_BATCH"
                          << (batchErr ? std::strerror(batchErr) : "ok")
                          << "| nice" << nice
                          << (niceErr ? std::strerror(niceErr) : "ok")
                          << "| ioprio"
                          << (ioErr ? std::strerror(ioErr) : "ok");
    }
    return ok;
#else
    (void)nice;
    (void)threadName;
    return false;
#endif
}

} // namespace mixxx
