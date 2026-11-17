#pragma once

#include <memory>
#include <string>
#include <vector>

#include "stems/htdemucs.h"

namespace mixxx {
namespace stems {

/// SCNet small (MIT, starrytong / ZFTurbo checkpoint) reduced to 3 stems:
/// the best quality, used while no deck plays. MUSDB18 test 9.72 dB SDR
/// against 8.93 for htdemucs. Only on the CPU: its LSTMs give wrong results
/// on the GPU with MNN. ~1.6x real time with 4 threads on a Snapdragon
/// 8 Gen 1 (stems/bench2/code/scnet_run.py, model scnet_small_rnn).
///
/// The network takes the STFT of 11 s of normalized audio, [1, 4, 2049, 476]
/// (channel * 2 + re/im; 4096/1024, rectangular window, centered, normalized)
/// and returns the STFTs of drums, bass + melody and vocals [1, 12, 2049, 476].
class ScnetNet : public SegmentNet {
  public:
    ScnetNet();
    ~ScnetNet() override;

    bool init(const std::string& modelPath, int threads);
    const std::string& error() const {
        return m_error;
    }

    long segmentFrames() const override;
    bool separate(const float* mix, float* stems) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
    std::string m_error;
};

} // namespace stems
} // namespace mixxx
