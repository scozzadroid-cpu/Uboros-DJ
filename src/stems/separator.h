#pragma once

#include <functional>

namespace mixxx {
namespace stems {

/// Separates one track into 3 stereo stems (drums, bass + melody, vocals),
/// one segment at a time, in any order (the play position first).
/// Implemented by TrackSeparator (htdemucs) and SpleeterSeparator.
class Separator {
  public:
    /// Final stems of frames [start, start + count): 6 floats per frame
    /// (drums L R, bass + melody L R, vocals L R)
    using Output = std::function<void(long start, long count, const float* stems)>;

    virtual ~Separator() = default;

    /// interleaved stereo at 44.1 kHz
    virtual void setTrack(const float* interleaved, long frames) = 0;
    virtual long frames() const = 0;
    virtual int segmentCount() const = 0;
    virtual bool isSegmentDone(int segment) const = 0;
    virtual int doneCount() const = 0;
    /// The segment whose output contains this frame
    virtual int segmentAt(long frame) const = 0;
    virtual bool processSegment(int segment, const Output& output) = 0;
    /// Seconds of new audio a segment adds (without the overlap)
    virtual double segmentSeconds() const = 0;

    /// Frames [*pStart, *pEnd) that the segment outputs
    virtual void segmentRange(int segment, long* pStart, long* pEnd) const = 0;
    /// Whether segments can be skipped (their frames are already separated):
    /// only when the output of a segment does not need its neighbours
    virtual bool canSkipSegments() const {
        return false;
    }
    /// Marks the segment done without computing it; the frames shared with
    /// its neighbours are then not output either
    virtual void skipSegment(int /*segment*/) {
    }
};

} // namespace stems
} // namespace mixxx
