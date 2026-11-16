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

#include <memory>
#include <string>
#include <vector>

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
    };

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

/// Separates a whole track, one segment at a time, in any order.
class TrackSeparator {
  public:
    /// overlap: 0.25 is the demucs default
    explicit TrackSeparator(HtdemucsNet* pNet, double overlap = 0.25);
    ~TrackSeparator();

    /// Stereo track at 44.1 kHz, interleaved. Computes the whole-track
    /// normalization (it changes the result: 26-36 dB if skipped).
    void setTrack(const float* interleaved, long frames);

    long frames() const {
        return m_frames;
    }
    int segmentCount() const {
        return static_cast<int>(m_offsets.size());
    }
    long segmentOffset(int segment) const {
        return m_offsets[segment];
    }
    bool isSegmentDone(int segment) const {
        return m_done[segment];
    }
    /// First segment covering the frame
    int segmentAt(long frame) const;

    /// Separates one segment and adds it to the result
    bool processSegment(int segment);

    /// True if every segment covering [start, end) is done
    bool isRangeFinal(long start, long end) const;

    /// Stems of [start, start + count), interleaved per frame:
    /// drums L R, bass + melody L R, vocals L R (6 floats per frame).
    /// Frames that are not final yet are left untouched.
    void readStems(long start, long count, float* out) const;

  private:
    void spectrogram(const float* mix, float* mag) const; // mix planar [2][kSegmentFrames]
    void inverse(const float* spec, float* out) const;    // one channel [2][kBins][kSpecFrames] -> [kSegmentFrames]

    HtdemucsNet* m_pNet;
    double m_overlap;
    long m_frames = 0;
    std::vector<float> m_track[2]; // normalized, planar
    double m_mean = 0;
    double m_std = 1;
    std::vector<long> m_offsets;
    std::vector<bool> m_done;
    std::vector<float> m_weight;      // triangle, kSegmentFrames
    std::vector<float> m_out;         // [kStemCount][2][frames], weighted sum
    std::vector<float> m_weightSum;   // [frames]
    std::vector<float> m_window;      // periodic Hann, kNfft
    std::vector<float> m_envelope;    // iSTFT window envelope, per output sample of a segment
    struct Fft;
    std::unique_ptr<Fft> m_pFft;
    // Work buffers (one segment)
    std::vector<float> m_mix, m_mag, m_x, m_xt, m_stem;
};

} // namespace stems
} // namespace mixxx
