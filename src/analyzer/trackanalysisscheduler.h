#pragma once

#include <QList>
#include <deque>
#include <memory>
#include <set>
#include <vector>

#include "analyzer/analyzerscheduledtrack.h"
#include "analyzer/analyzerthread.h"
#include "util/db/dbconnectionpool.h"

/// Callbacks for triggering side-effects in the outer context of
/// TrackAnalysisScheduler.
///
/// All functions will only be called from the host thread of
/// TrackAnalysisScheduler, not from worker threads.
class TrackAnalysisSchedulerEnvironment {
  public:
    virtual ~TrackAnalysisSchedulerEnvironment() = default;

    virtual TrackPointer loadTrackById(TrackId trackId) const = 0;
};

class TrackAnalysisScheduler : public QObject {
    Q_OBJECT

  public:
    typedef std::unique_ptr<TrackAnalysisScheduler, void(*)(TrackAnalysisScheduler*)> Pointer;
    // Subclass that provides a default constructor and nothing else
    class NullPointer: public Pointer {
      public:
        NullPointer();
    };

    static Pointer createInstance(
            std::unique_ptr<const TrackAnalysisSchedulerEnvironment> pEnvironment,
            int numWorkerThreads,
            const mixxx::DbConnectionPoolPtr& pDbConnectionPool,
            const UserSettingsPointer& pConfig,
            AnalyzerModeFlags modeFlags);

    /*private*/ TrackAnalysisScheduler(
            std::unique_ptr<const TrackAnalysisSchedulerEnvironment> pEnvironment,
            int numWorkerThreads,
            const mixxx::DbConnectionPoolPtr& pDbConnectionPool,
            const UserSettingsPointer& pUserSettings,
            AnalyzerModeFlags modeFlags);
    ~TrackAnalysisScheduler() override;

    // Schedule single or multiple tracks. After all tracks have been scheduled
    // the caller must invoke resume() once.
    bool scheduleTrack(AnalyzerScheduledTrack track);
    int scheduleTracks(const QList<AnalyzerScheduledTrack>& tracks);

    // Drops trackId from the queue and aborts its analysis if a worker is
    // running it. The partial results are discarded (Analyzer::cleanup()
    // without storeResults()). Returns true if anything was cancelled.
    bool cancelTrack(TrackId trackId);

  public slots:
    void suspend();

    // After scheduling tracks the analysis must be resumed once.
    // Resume must also be called after suspending the analysis.
    void resume();

    // Stops a running analysis and discards all enqueued tracks.
    void stop();

  signals:
    // Progress for individual tracks is passed-through from the workers
    void trackProgress(TrackId trackId, AnalyzerProgress analyzerProgress);
    // Current average progress for all scheduled tracks and from all workers
    void progress(AnalyzerProgress currentTrackProgress, int currentTrackNumber, int totalTracksCount);
    void finished();

  private slots:
    void onWorkerThreadProgress(int threadId, AnalyzerThreadState threadState, TrackId trackId, AnalyzerProgress analyzerProgress);

  private:
    // Owns an analyzer thread and buffers the most recent progress update
    // received from this thread during analysis. It does not need to be
    // thread-safe, because all functions are invoked from the host thread
    // that runs the TrackAnalysisScheduler.
    class Worker {
      public:
        explicit Worker(AnalyzerThread::Pointer thread = AnalyzerThread::NullPointer())
            : m_thread(std::move(thread)),
              m_analyzerProgress(kAnalyzerProgressUnknown) {
        }
        Worker(const Worker&) = delete;
        Worker(Worker&&) = default;

        operator bool() const {
            return static_cast<bool>(m_thread);
        }

        AnalyzerThread* thread() const {
            DEBUG_ASSERT(m_thread);
            return m_thread.get();
        }

        AnalyzerProgress analyzerProgress() const {
            return m_analyzerProgress;
        }

        bool submitNextTrack(const AnalyzerTrack& track, TrackId trackId) {
            DEBUG_ASSERT(m_thread);
            if (!m_thread->submitNextTrack(track)) {
                return false;
            }
            m_submittedTrackId = trackId;
            m_pSubmittedCancelFlag = track.cancelFlag();
            return true;
        }

        // Asks the thread to abort the track it was handed last, if that is
        // trackId and it has not reported it as Done yet. The thread answers
        // with Done (progress unknown) as soon as it notices.
        bool cancelSubmittedTrack(TrackId trackId) {
            if (!m_pSubmittedCancelFlag || m_submittedTrackId != trackId) {
                return false;
            }
            m_pSubmittedCancelFlag->store(true, std::memory_order_relaxed);
            return true;
        }

        // The thread has finished with (or never got) the submitted track
        void forgetSubmittedTrack() {
            m_submittedTrackId = TrackId();
            m_pSubmittedCancelFlag.reset();
        }

        void suspendThread() {
            if (m_thread) {
                m_thread->suspend();
            }
        }

        void resumeThread() {
            if (m_thread) {
                m_thread->resume();
            }
        }

        void stopThread() {
            if (m_thread) {
                m_thread->stop();
            }
        }

        void onAnalyzerProgress(AnalyzerProgress analyzerProgress) {
            DEBUG_ASSERT(m_thread);
            m_analyzerProgress = analyzerProgress;
        }

        void onThreadExit() {
            DEBUG_ASSERT(m_thread);
            m_thread.reset();
            m_analyzerProgress = kAnalyzerProgressUnknown;
            forgetSubmittedTrack();
        }

      private:
        AnalyzerThread::Pointer m_thread;
        AnalyzerProgress m_analyzerProgress;
        // The track last handed to the thread, until it reports it Done
        TrackId m_submittedTrackId;
        AnalyzerTrack::CancelFlag m_pSubmittedCancelFlag;
    };

    bool submitNextTrack(Worker* worker);
    void emitProgressOrFinished();

    bool allTracksFinished() const {
        return m_queuedTracks.empty() &&
                m_pendingTrackIds.empty();
    }

    const std::unique_ptr<const TrackAnalysisSchedulerEnvironment> m_pEnvironment;

    std::vector<Worker> m_workers;

    std::deque<AnalyzerScheduledTrack> m_queuedTracks;

    // Tracks that have already been submitted to workers
    // and not yet reported back as finished.
    std::set<TrackId> m_pendingTrackIds;

    AnalyzerProgress m_currentTrackProgress;

    int m_currentTrackNumber;

    int m_dequeuedTracksCount;

    typedef std::chrono::steady_clock Clock;
    Clock::time_point m_lastProgressEmittedAt;
};
