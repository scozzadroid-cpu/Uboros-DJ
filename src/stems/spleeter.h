#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "stems/separator.h"

namespace mixxx {
namespace stems {

/// Spleeter 4stems (Deezer, MIT) reduced to the 3 stems of Mixxx: the light
/// quality, for phones too slow for htdemucs. The 4 U-Nets run on the CPU
/// (no GPU needed), ~12x real time on a Snapdragon 8 Gen 1 with 4 threads.
///
/// The network (stems/bench2/code/export_spleeter.py) takes the magnitude
/// of 512 STFT frames (4096/1024, Hann) below 11 kHz, [1, 2, 512, 1024], and
/// returns ratio masks [1, 3, 2, 512, 1024] for drums, bass + melody and
/// vocals. Above 11 kHz each frame uses the mean of its mask. MUSDB18 test:
/// 6.45 dB SDR on average (htdemucs 8.93).
class SpleeterNet {
  public:
    static constexpr int kFrames = 512;  // STFT frames per block (11.9 s)
    static constexpr int kNetBins = 1024; // bins the network sees

    SpleeterNet();
    ~SpleeterNet();

    bool init(const std::string& modelPath, int threads);
    const std::string& error() const {
        return m_error;
    }
    /// mag [2][kFrames][kNetBins] -> masks [3][2][kFrames][kNetBins]
    bool run(const float* mag, float* masks);

  private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
    std::string m_error;
};

/// One segment = one block of 512 STFT frames. The output of a block is final
/// except for the 3 hops it shares with each neighbour (overlap-add), which
/// are output when both blocks are done.
class SpleeterSeparator : public Separator {
  public:
    explicit SpleeterSeparator(SpleeterNet* pNet);
    ~SpleeterSeparator() override;

    void setTrack(const float* interleaved, long frames) override;
    long frames() const override {
        return m_frames;
    }
    int segmentCount() const override {
        return m_blocks;
    }
    bool isSegmentDone(int segment) const override {
        return m_done[segment];
    }
    int doneCount() const override;
    int segmentAt(long frame) const override;
    bool processSegment(int segment, const Output& output) override;
    double segmentSeconds() const override {
        return double(SpleeterNet::kFrames) * 1024 / 44100;
    }
    void segmentRange(int segment, long* pStart, long* pEnd) const override;
    bool canSkipSegments() const override {
        return true;
    }
    void skipSegment(int segment) override;

  private:
    struct Boundary {
        std::vector<float> sum; // [3 stems][2 channels][kShared]
        int parts = 0;
        bool abandoned = false;
    };
    float sample(int channel, long padded) const;
    void emitStems(const float* planar, long stride, long paddedStart, long count, const Output& output);
    void addToBoundary(int boundary, const float* planar, long stride, long offset, const Output& output);

    SpleeterNet* m_pNet;
    long m_frames = 0;
    long m_stftFrames = 0;
    int m_blocks = 0;
    std::vector<float> m_track; // interleaved stereo
    std::vector<bool> m_done;
    std::map<int, Boundary> m_boundaries; // key: index of the block on the right
    std::vector<float> m_window;          // periodic Hann
    std::vector<float> m_synthesis;       // Hann / sum of the overlapping squares
    struct Fft;
    std::unique_ptr<Fft> m_pFft;
    std::vector<float> m_re, m_im;   // [2][kFrames][kBins] spectrum of the block
    std::vector<float> m_mag, m_mask; // network input and output
    std::vector<float> m_out;         // [3][2][kFrames * kHop + kShared] time domain
    std::vector<float> m_emit;
};

} // namespace stems
} // namespace mixxx
