#include "soundio/audiocallbackstats.h"

#include <QElapsedTimer>
#include <QTimer>
#include <memory>

#include "util/logger.h"

namespace mixxx {

namespace AudioCallbackStats {

namespace {

const Logger kLogger("AudioCallbackStats");

constexpr int kReportIntervalMs = 5000;

double nanosToMillis(qint64 nanos) {
    return static_cast<double>(nanos) / 1e6;
}

QString codeList(quint32 codes) {
    QString list;
    for (int code = 0; code < 32; ++code) {
        if (codes & (1u << code)) {
            if (!list.isEmpty()) {
                list += QLatin1Char(',');
            }
            list += QString::number(code);
        }
    }
    return list;
}

void report(double windowSecs) {
    using namespace detail;
    // Cheap check first: nothing to report in the normal case
    if (s_overBudget.load(std::memory_order_relaxed) == 0 &&
            s_underflows.load(std::memory_order_relaxed) == 0) {
        // Restart the window without logging
        s_callbacks.store(0, std::memory_order_relaxed);
        s_sumDurationNs.store(0, std::memory_order_relaxed);
        s_maxDurationNs.store(0, std::memory_order_relaxed);
        s_maxGapNs.store(0, std::memory_order_relaxed);
        return;
    }
    const quint32 callbacks = s_callbacks.exchange(0, std::memory_order_relaxed);
    const qint64 sumNs = s_sumDurationNs.exchange(0, std::memory_order_relaxed);
    const qint64 maxNs = s_maxDurationNs.exchange(0, std::memory_order_relaxed);
    const qint64 maxGapNs = s_maxGapNs.exchange(0, std::memory_order_relaxed);
    const quint32 overBudget = s_overBudget.exchange(0, std::memory_order_relaxed);
    const quint32 underflows = s_underflows.exchange(0, std::memory_order_relaxed);
    const quint32 codes = s_underflowCodes.exchange(0, std::memory_order_relaxed);
    const qint64 budgetNs = s_budgetNs.load(std::memory_order_relaxed);
    const qint32 frames = s_framesPerBuffer.load(std::memory_order_relaxed);
    const double avgMs = callbacks > 0 ? nanosToMillis(sumNs) / callbacks : 0.0;
    kLogger.warning().noquote()
            << QStringLiteral(
                       "audio callbacks in %1 s: %2, avg %3 ms, max %4 ms, "
                       "budget %5 ms (%6 frames), over budget %7, "
                       "underflows %8 (codes %9), max gap %10 ms")
                       .arg(QString::number(windowSecs, 'f', 1))
                       .arg(callbacks)
                       .arg(QString::number(avgMs, 'f', 2))
                       .arg(QString::number(nanosToMillis(maxNs), 'f', 2))
                       .arg(QString::number(nanosToMillis(budgetNs), 'f', 2))
                       .arg(frames)
                       .arg(overBudget)
                       .arg(underflows)
                       .arg(codes ? codeList(codes) : QStringLiteral("-"))
                       .arg(QString::number(nanosToMillis(maxGapNs), 'f', 2));
}

} // namespace

void startReporting(QObject* parent) {
    auto* pTimer = new QTimer(parent);
    // A coarse timer is fine and avoids extra wakeups
    pTimer->setTimerType(Qt::VeryCoarseTimer);
    pTimer->setInterval(kReportIntervalMs);
    auto pWindow = std::make_shared<QElapsedTimer>();
    pWindow->start();
    QObject::connect(pTimer, &QTimer::timeout, pTimer, [pWindow] {
        report(pWindow->restart() / 1000.0);
    });
    pTimer->start();
}

} // namespace AudioCallbackStats

} // namespace mixxx
