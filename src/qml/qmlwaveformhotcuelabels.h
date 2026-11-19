#pragma once

#include <atomic>

namespace mixxx {
namespace qml {

/// How the marks of the hot cues on the scrolling waveforms are labeled, set
/// by the waveform the QML interface makes (hotcueLabels of
/// Mixxx.WaveformRendererMark, see qmlwaveformrenderer.cpp) and read when a
/// mark draws its image (WaveformMark::generateImage, render thread):
/// 0 the number (Mixxx's own), 1 the letter A-H of the pad, 2 the name of the
/// cue (its number when it has none).
/// A header of its own, with an inline variable, so that the waveform code
/// (another library than the QML one) can read it without linking to it.
inline std::atomic<int> g_hotcueLabelStyle{0};

inline int hotcueLabelStyle() {
    return g_hotcueLabelStyle.load(std::memory_order_relaxed);
}

} // namespace qml
} // namespace mixxx
