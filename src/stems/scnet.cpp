#include "stems/scnet.h"

#include <MNN/Interpreter.hpp>
#include <MNN/MNNForwardType.h>
#include <MNN/expr/Executor.hpp>
#include <MNN/expr/ExprCreator.hpp>
#include <MNN/expr/Module.hpp>
#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mixxx {
namespace stems {

namespace {

constexpr long kChunk = 485100;      // 11 s, the training length
constexpr long kPadded = 486400;     // SCNet pad_len(): an odd number of hops
constexpr int kFft = 4096;
constexpr int kStep = 1024;
constexpr int kFreq = kFft / 2 + 1;  // 2049
constexpr int kTimes = kPadded / kStep + 1; // 476 (centered)
constexpr int kCenter = kFft / 2;
constexpr long kPlane = long(kFreq) * kTimes;

// torch "reflect" padding
inline long reflectIndex(long i, long n) {
    if (i < 0) {
        i = -i;
    }
    if (i >= n) {
        i = 2 * (n - 1) - i;
    }
    return i;
}

} // namespace

struct ScnetNet::Impl {
    std::shared_ptr<MNN::Express::Executor::RuntimeManager> pRuntime;
    std::shared_ptr<MNN::Express::Module> pModule;
    float* time = nullptr;
    fftwf_complex* freq = nullptr;
    fftwf_plan forward = nullptr;
    fftwf_plan backward = nullptr;
    std::vector<float> padded;   // one channel, kPadded
    std::vector<float> spec;     // [4][kFreq][kTimes]
    std::vector<float> wave;     // overlap-add, kPadded + kFft
    std::vector<float> envelope; // frames covering each sample of wave
    Impl() {
        time = static_cast<float*>(fftwf_malloc(sizeof(float) * kFft));
        freq = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * kFreq));
        forward = fftwf_plan_dft_r2c_1d(kFft, time, freq, FFTW_ESTIMATE);
        backward = fftwf_plan_dft_c2r_1d(kFft, freq, time, FFTW_ESTIMATE);
        padded.resize(kPadded);
        spec.resize(4 * kPlane);
        wave.resize(kPadded + kFft);
        envelope.assign(kPadded + kFft, 0.0f);
        for (int t = 0; t < kTimes; ++t) {
            for (int n = 0; n < kFft; ++n) {
                envelope[long(t) * kStep + n] += 1.0f; // rectangular window
            }
        }
    }
    ~Impl() {
        fftwf_destroy_plan(forward);
        fftwf_destroy_plan(backward);
        fftwf_free(time);
        fftwf_free(freq);
    }
};

ScnetNet::ScnetNet() = default;
ScnetNet::~ScnetNet() = default;

long ScnetNet::segmentFrames() const {
    return kChunk;
}

bool ScnetNet::init(const std::string& modelPath, int threads) {
    using namespace MNN;
    using namespace MNN::Express;
    m_pImpl = std::make_unique<Impl>();
    ScheduleConfig sc;
    BackendConfig bc;
    bc.precision = BackendConfig::Precision_High; // fp16 weights, fp32 math (43 dB from PyTorch)
    bc.memory = BackendConfig::Memory_Normal;
    bc.power = BackendConfig::Power_Normal;
    sc.backendConfig = &bc;
    sc.type = MNN_FORWARD_CPU;
    sc.numThread = threads;
    m_pImpl->pRuntime.reset(Executor::RuntimeManager::createRuntimeManager(sc));
    if (!m_pImpl->pRuntime) {
        m_error = "cannot create the MNN runtime";
        return false;
    }
    Module::Config mc;
    mc.shapeMutable = false;
    m_pImpl->pModule.reset(Module::load({"spec"}, {"y"}, modelPath.c_str(), m_pImpl->pRuntime, &mc));
    if (!m_pImpl->pModule) {
        m_error = "cannot load the model " + modelPath;
        return false;
    }
    return true;
}

bool ScnetNet::separate(const float* mix, float* stems) {
    using namespace MNN::Express;
    Impl& d = *m_pImpl;
    // torch.stft(center=True, normalized=True, no window)
    const float norm = 1.0f / std::sqrt(float(kFft));
    for (int c = 0; c < 2; ++c) {
        std::fill(d.padded.begin(), d.padded.end(), 0.0f);
        std::copy_n(mix + long(c) * kChunk, kChunk, d.padded.begin());
        for (int t = 0; t < kTimes; ++t) {
            for (int n = 0; n < kFft; ++n) {
                d.time[n] = d.padded[reflectIndex(long(t) * kStep + n - kCenter, kPadded)];
            }
            fftwf_execute(d.forward);
            for (int f = 0; f < kFreq; ++f) {
                d.spec[(long(c) * 2 * kFreq + f) * kTimes + t] = d.freq[f][0] * norm;
                d.spec[((long(c) * 2 + 1) * kFreq + f) * kTimes + t] = d.freq[f][1] * norm;
            }
        }
    }
    auto input = _Input({1, 4, kFreq, kTimes}, NCHW, halide_type_of<float>());
    std::memcpy(input->writeMap<float>(), d.spec.data(), sizeof(float) * 4 * kPlane);
    std::vector<VARP> outs = d.pModule->onForward({input});
    const float* y = outs.size() == 1 ? outs[0]->readMap<float>() : nullptr;
    if (!y) {
        m_error = "cannot read the network output";
        return false;
    }
    // torch.istft(center=True, normalized=True, no window): frames scaled
    // back by sqrt(n_fft), overlap-added, divided by the window envelope
    const float scale = std::sqrt(float(kFft)) / kFft; // FFTW c2r is not normalized
    for (int s = 0; s < kStemCount; ++s) {
        for (int c = 0; c < 2; ++c) {
            const float* re = y + ((long(s) * 4 + c * 2) * kFreq) * kTimes;
            const float* im = re + kPlane;
            std::fill(d.wave.begin(), d.wave.end(), 0.0f);
            for (int t = 0; t < kTimes; ++t) {
                for (int f = 0; f < kFreq; ++f) {
                    d.freq[f][0] = re[long(f) * kTimes + t];
                    d.freq[f][1] = im[long(f) * kTimes + t];
                }
                fftwf_execute(d.backward);
                float* dst = d.wave.data() + long(t) * kStep;
                for (int n = 0; n < kFft; ++n) {
                    dst[n] += d.time[n] * scale;
                }
            }
            float* out = stems + (long(s) * 2 + c) * kChunk;
            for (long i = 0; i < kChunk; ++i) {
                out[i] = d.wave[kCenter + i] / d.envelope[kCenter + i];
            }
        }
    }
    return true;
}

} // namespace stems
} // namespace mixxx
