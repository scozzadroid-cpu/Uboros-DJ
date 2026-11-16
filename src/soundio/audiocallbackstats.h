#pragma once

#include <QtGlobal>
#include <atomic>
#include <cstdint>

class QObject;

namespace mixxx {

/// Aggregated timing of the audio callback of the clock reference device,
/// for diagnosing dropouts (e.g. time stretching of several stem decks that
/// no longer fits into the buffer period).
///
/// The callback side only does a few relaxed atomic operations: no locks, no
/// allocations, no logging. A timer on the main thread collects the counters
/// every 5 s and logs a single warning line, only if the period contained at
/// least one underflow or one callback that ran longer than its budget (the
/// buffer duration). When everything is fine nothing is logged.
///
/// The window counters are reset by exchange() on the main thread while the
/// engine thread updates them, so a callback racing the reset may be counted
/// in the neighbouring window. That is fine for diagnostics.
namespace AudioCallbackStats {

namespace detail {
inline std::atomic<quint32> s_callbacks{0};
inline std::atomic<quint32> s_overBudget{0};
inline std::atomic<quint32> s_underflows{0};
inline std::atomic<quint32> s_underflowCodes{0}; // bit n = code n was seen
inline std::atomic<qint64> s_sumDurationNs{0};
inline std::atomic<qint64> s_maxDurationNs{0};
inline std::atomic<qint64> s_maxGapNs{0};
inline std::atomic<qint64> s_budgetNs{0};
inline std::atomic<qint32> s_framesPerBuffer{0};

// Single writer (the engine thread), so a plain load/store is enough.
inline void storeMax(std::atomic<qint64>* pMax, qint64 value) {
    if (value > pMax->load(std::memory_order_relaxed)) {
        pMax->store(value, std::memory_order_relaxed);
    }
}
} // namespace detail

/// Engine thread, once per callback of the clock reference device.
/// durationNs: time spent in the callback. gapNs: time since the start of
/// the previous callback. budgetNs: the buffer duration.
inline void recordCallback(qint64 durationNs,
        qint64 gapNs,
        qint64 budgetNs,
        qint32 framesPerBuffer) {
    using namespace detail;
    s_callbacks.fetch_add(1, std::memory_order_relaxed);
    s_sumDurationNs.fetch_add(durationNs, std::memory_order_relaxed);
    storeMax(&s_maxDurationNs, durationNs);
    storeMax(&s_maxGapNs, gapNs);
    if (durationNs > budgetNs) {
        s_overBudget.fetch_add(1, std::memory_order_relaxed);
    }
    if (s_budgetNs.load(std::memory_order_relaxed) != budgetNs) {
        s_budgetNs.store(budgetNs, std::memory_order_relaxed);
        s_framesPerBuffer.store(framesPerBuffer, std::memory_order_relaxed);
    }
}

/// Any thread of the audio path: an underflow/overflow was detected. The code
/// identifies the detecting site (see SoundManager::underflowHappened()).
inline void recordUnderflow(int code) {
    using namespace detail;
    s_underflows.fetch_add(1, std::memory_order_relaxed);
    if (code >= 0 && code < 32) {
        s_underflowCodes.fetch_or(1u << code, std::memory_order_relaxed);
    }
}

/// Main thread, once: starts the 5 s reporting timer, owned by parent.
void startReporting(QObject* parent);

} // namespace AudioCallbackStats

} // namespace mixxx
