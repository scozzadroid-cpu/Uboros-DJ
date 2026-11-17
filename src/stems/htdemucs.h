#pragma once

// htdemucs stem separation on the phone (3 stems: drums, bass + melody, vocals).
//
// The network (model "CoreN3", see ~/mixxx-android/stems/gpu/code/core.py) runs
// with MNN, preferably on the GPU with OpenCL in fp16. Everything numerically
// delicate stays on the CPU: STFT/iSTFT (FFTW) and the mean/std normalizations,
// whose reductions overflow fp16. The splitting into overlapping segments and
// their weights replicate demucs.apply.apply_model, so the result matches
// PyTorch (stems/gpu/RISULTATI.md: ~45 dB in fp16, ~70 dB in fp32).
//
// Plain C++ (no Qt) so that it can also be built as a command line test.

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "stems/separator.h"

namespace mixxx {
namespace stems {

constexpr int kSampleRate = 44100;
constexpr int kSegmentFrames = 343980; // 7.8 s, the training length of htdemucs
constexpr int kNfft = 4096;
constexpr int kHop = 1024;
constexpr int kBins = kNfft / 2;                                          // last bin dropped
constexpr int kSpecFrames = (kSegmentFrames + kHop - 1) / kHop;           // 336
constexpr int kStemCount = 3;                                             // drums, bass + melody, vocals
// Each output slot carries the mean of the sources summed into it
constexpr float kSlotMeanFactor[kStemCount] = {1.0f, 2.0f, 1.0f};

/// The network between spectrogram and mask (no STFT inside).
class HtdemucsNet {
  public:
    enum class Backend {
        Cpu,
        OpenCl,
    };
    struct Config {
        std::string modelPath;
        /// Directory for the OpenCL kernel tuning cache (first run: minutes)
        std::string cacheDir;
        Backend backend = Backend::OpenCl;
        bool lowPrecision = true; // fp16
        int threads = 4;          // CPU backend
        /// OpenCL kernel tuning, done once and kept in cacheDir. POCO F4 GT,
        /// first start / s per segment: None 2.6 s / 3.3, Fast 24 s / 2.2,
        /// Wide ~570 s / 1.94
        enum class Tuning {
            None,
            Fast,
            Wide,
        };
        Tuning tuning = Tuning::None;
    };
    /// Cache file of the tuned kernels for a configuration
    static std::string cacheFile(const Config& config);

    HtdemucsNet();
    ~HtdemucsNet();

    /// Loads the model; false (with error()) if the backend is not available.
    bool init(const Config& config);
    const std::string& error() const {
        return m_error;
    }
    /// Backend actually in use (MNN may fall back to the CPU)
    Backend backend() const {
        return m_backend;
    }

    /// mix: normalized, planar [2][kSegmentFrames]
    /// mag: normalized, [4][kBins][kSpecFrames] (re/im of each channel)
    /// x:   out, [kStemCount][4][kBins][kSpecFrames]
    /// xt:  out, [kStemCount][2][kSegmentFrames]
    bool run(const float* mix, const float* mag, float* x, float* xt);

  private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
    std::string m_error;
    Backend m_backend = Backend::Cpu;
};

/// Separates a whole track, one segment at a time, in any order (e.g.
/// starting at the play position). Memory stays bounded: only the segments
/// whose overlap with a neighbour is still open are kept, the final frames
/// are handed to a callback as soon as every segment covering them is done.
/// A network that separates a whole segment of normalized audio (STFT and
/// inverse included), for TrackSeparator: e.g. SCNet
class SegmentNet {
  public:
    virtual ~SegmentNet() = default;
    virtual long segmentFrames() const = 0;
    /// mix: planar [2][segmentFrames()], normalized like the whole track
    /// (mean and std of its mono mix); stems: out, [kStemCount][2][segmentFrames()]
    virtual bool separate(const float* mix, float* stems) = 0;
};

/// Overlapping segments weighted by a triangle, as demucs.apply_model; the
/// network is htdemucs or a SegmentNet
class TrackSeparator : public Separator {
  public:
    explicit TrackSeparator(HtdemucsNet* pNet, double overlap = 0.25);
    TrackSeparator(SegmentNet* pNet, double overlap);
    ~TrackSeparator() override;

    void setTrack(const float* interleaved, long frames) override;

    long frames() const override {
        return m_frames;
    }
    int segmentCount() const override {
        return static_cast<int>(m_offsets.size());
    }
    long segmentOffset(int segment) const {
        return m_offsets[segment];
    }
    bool isSegmentDone(int segment) const override {
        return m_done[segment];
    }
    int doneCount() const override;
    int segmentAt(long frame) const override;

    bool processSegment(int segment, const Output& output) override;
    double segmentSeconds() const override {
        return (1.0 - m_overlap) * m_segmentFrames / kSampleRate;
    }

    void segmentRange(int segment, long* pStart, long* pEnd) const override {
        *pStart = m_offsets[segment];
        *pEnd = segmentEnd(segment);
    }

    bool isRangeFinal(long start, long end) const;

  private:
    long segmentEnd(int segment) const {
        return std::min<long>(m_offsets[segment] + m_segmentFrames, m_frames);
    }
    void emitFinal(long start, long end, const Output& output);
    void spectrogram(const float* mix, float* mag) const; // mix planar [2][kSegmentFrames]
    void inverse(const float* spec, float* out) const;    // one channel [2][kBins][kSpecFrames] -> [kSegmentFrames]

    void initWeights();
    /// Outputs the frames that became final and frees what is not needed
    bool finishSegment(int segment, const Output& output);

    HtdemucsNet* m_pNet = nullptr;
    SegmentNet* m_pSegmentNet = nullptr;
    long m_segmentFrames = kSegmentFrames;
    double m_overlap;
    long m_frames = 0;
    std::vector<float> m_track[2]; // normalized, planar
    double m_mean = 0;
    double m_std = 1;
    std::vector<long> m_offsets;
    std::vector<bool> m_done;
    /// Output of each segment, [kStemCount][2][length], freed once all its
    /// frames are final
    std::vector<std::vector<float>> m_segmentOut;
    std::vector<float> m_weight;      // triangle, kSegmentFrames
    std::vector<float> m_window;      // periodic Hann, kNfft
    std::vector<float> m_envelope;    // iSTFT window envelope, per output sample of a segment
    struct Fft;
    std::unique_ptr<Fft> m_pFft;
    // Work buffers (one segment)
    std::vector<float> m_mix, m_mag, m_x, m_xt, m_stem, m_emit;
};

} // namespace stems
} // namespace mixxx
