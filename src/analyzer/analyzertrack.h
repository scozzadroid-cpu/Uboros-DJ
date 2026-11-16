#pragma once

#include <atomic>
#include <memory>
#include <optional>

#include "track/track_decl.h"

/// A scheduled not-null track with additional options for analysis.
class AnalyzerTrack {
  public:
    struct Options {
        /// If set, overrides whether the analysis should assume constant BPM.
        std::optional<bool> useFixedTempo;
    };

    explicit AnalyzerTrack(TrackPointer track, Options options = Options());

    /// Fetches the (not-null) track to be analyzed.
    const TrackPointer& getTrack() const;

    /// Fetches the additional options.
    const Options& getOptions() const;

    /// Cancellation flag of this submission, shared by all copies of this
    /// object (the host keeps one, the worker thread the other). Each
    /// AnalyzerTrack gets its own flag, so a cancellation never carries over
    /// to a later submission of the same track.
    using CancelFlag = std::shared_ptr<std::atomic<bool>>;
    const CancelFlag& cancelFlag() const {
        return m_pCancelFlag;
    }
    /// Checked by the worker thread between chunks.
    bool isCancelRequested() const {
        return m_pCancelFlag->load(std::memory_order_relaxed);
    }

  private:
    /// The (not-null) track to be analyzed.
    TrackPointer m_track;
    /// The additional options.
    Options m_options;
    /// Set by the host to abort the analysis (see cancelFlag()).
    CancelFlag m_pCancelFlag;
};
