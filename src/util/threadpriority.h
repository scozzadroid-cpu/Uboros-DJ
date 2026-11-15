#pragma once

#if defined(__ANDROID__) || defined(__LINUX__)
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace mixxx {

// Nice values of the calling thread (Linux/Android): -20 = highest priority,
// 19 = lowest. Android apps may use down to THREAD_PRIORITY_URGENT_AUDIO (-19)
// for their own threads without any permission.
constexpr int kThreadNiceAudio = -16;     // android.os.Process.THREAD_PRIORITY_AUDIO
constexpr int kThreadNiceBackground = 10; // THREAD_PRIORITY_BACKGROUND
constexpr int kThreadNiceLowest = 19;     // THREAD_PRIORITY_LOWEST

// QThread::setPriority() has no effect on Linux/Android for normal
// (SCHED_OTHER) threads: Qt only maps IdlePriority to SCHED_IDLE. Use the
// per-thread nice value instead. Returns false if it could not be applied
// (e.g. raising the priority without the privilege on desktop Linux).
inline bool setCurrentThreadNice(int nice) {
#if defined(__ANDROID__) || defined(__LINUX__)
    return setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), nice) == 0;
#else
    (void)nice;
    return false;
#endif
}

} // namespace mixxx
