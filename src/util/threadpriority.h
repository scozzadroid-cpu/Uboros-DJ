#pragma once

#if defined(__ANDROID__) || defined(__LINUX__)
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <atomic>

namespace mixxx {

// Kernel thread id of the audio callback thread (0 until the first callback);
// read by the Android performance hint session
inline std::atomic<int> g_audioCallbackTid{0};

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

// Turns the calling thread into background CPU/IO work that must never delay
// the audio callback or the GUI render thread (Linux/Android only, no-op and
// returns false elsewhere):
//  - SCHED_BATCH: the thread never preempts a running thread on wakeup, so
//    the audio and render threads are not interrupted when it becomes
//    runnable. Unlike SCHED_IDLE it still receives a fair (nice-weighted)
//    share of CPU when all cores are busy, so analysis and stem separation
//    keep progressing while decks are playing.
//  - the given nice value (CPU weight relative to normal threads).
//  - best-effort I/O class at its lowest level (7): file reads of the audio
//    engine (CachingReader) are served first by schedulers that honor I/O
//    priorities, without the starvation risk of the idle I/O class.
// Lowering the own priority needs no privilege, also for Android apps.
// Threads created afterwards by the calling thread (e.g. decoder or
// inference thread pools) inherit all three settings.
// The result is logged once per process. Returns true if at least the
// scheduling policy or the nice value could be applied.
bool demoteCurrentThreadToBackground(int nice, const char* threadName);

} // namespace mixxx
