#include "stems/spleeter.h"

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

// tf.signal.stft as in Spleeter: 4096 samples, hop 1024, periodic Hann,
// 4096 zeros before the track, the last frame padded with zeros
constexpr int kFftSize = 4096;
constexpr int kStftHop = 1024;
constexpr int kSpectrumBins = kFftSize / 2 + 1;  // 2049
constexpr long kLeadIn = kFftSize;               // zeros before the track
constexpr int kShared = kFftSize - kStftHop;     // samples shared by 2 blocks
constexpr int kStems = 3;
constexpr long kBlockHops = long(SpleeterNet::kFrames) * kStftHop;

} // namespace

// ---------------------------------------------------------------- network

struct SpleeterNet::Impl {
    std::shared_ptr<MNN::Express::Executor::RuntimeManager> pRuntime;
    std::shared_ptr<MNN::Express::Module> pModule;
};

SpleeterNet::SpleeterNet() = default;
SpleeterNet::~SpleeterNet() = default;

bool SpleeterNet::init(const std::string& modelPath, int threads) {
    using namespace MNN;
    using namespace MNN::Express;
    m_pImpl = std::make_unique<Impl>();
    ScheduleConfig sc;
    BackendConfig bc;
    // fp16 weights in the file, computed in fp32: 50 dB from PyTorch (fp16
    // arithmetic gives 31 dB and is barely faster on the CPU)
    bc.precision = BackendConfig::Precision_High;
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
    m_pImpl->pModule.reset(Module::load({"mag"}, {"mask"}, modelPath.c_str(), m_pImpl->pRuntime, &mc));
    if (!m_pImpl->pModule) {
        m_error = "cannot load the model " + modelPath;
        return false;
    }
    return true;
}

bool SpleeterNet::run(const float* mag, float* masks) {
    using namespace MNN::Express;
    if (!m_pImpl || !m_pImpl->pModule) {
        return false;
    }
    constexpr long kInput = 2L * kFrames * kNetBins;
    auto input = _Input({1, 2, kFrames, kNetBins}, NCHW, halide_type_of<float>());
    std::memcpy(input->writeMap<float>(), mag, sizeof(float) * kInput);
    std::vector<VARP> outs = m_pImpl->pModule->onForward({input});
    if (outs.size() != 1) {
        m_error = "unexpected network outputs";
        return false;
    }
    const float* pMask = outs[0]->readMap<float>();
    if (!pMask) {
        m_error = "cannot read the network output";
        return false;
    }
    std::memcpy(masks, pMask, sizeof(float) * kStems * kInput);
    return true;
}

// ---------------------------------------------------------------- track

struct SpleeterSeparator::Fft {
    float* time;
    fftwf_complex* freq;
    fftwf_plan forward;
    fftwf_plan backward;
    Fft() {
        time = static_cast<float*>(fftwf_malloc(sizeof(float) * kFftSize));
        freq = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * kSpectrumBins));
        forward = fftwf_plan_dft_r2c_1d(kFftSize, time, freq, FFTW_ESTIMATE);
        backward = fftwf_plan_dft_c2r_1d(kFftSize, freq, time, FFTW_ESTIMATE);
    }
    ~Fft() {
        fftwf_destroy_plan(forward);
        fftwf_destroy_plan(backward);
        fftwf_free(time);
        fftwf_free(freq);
    }
};

SpleeterSeparator::SpleeterSeparator(SpleeterNet* pNet)
        : m_pNet(pNet),
          m_pFft(std::make_unique<Fft>()) {
    m_window.resize(kFftSize);
    for (int n = 0; n < kFftSize; ++n) {
        m_window[n] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * n / kFftSize));
    }
    // tf.signal.inverse_stft_window_fn: the window divided by the sum of the
    // squared windows overlapping each sample
    std::vector<double> den(kFftSize, 0.0);
    for (int k = -(kFftSize / kStftHop) + 1; k < kFftSize / kStftHop; ++k) {
        const int shift = k * kStftHop;
        for (int n = 0; n < kFftSize; ++n) {
            const int m = n + shift;
            if (m >= 0 && m < kFftSize) {
                den[n] += double(m_window[m]) * m_window[m];
            }
        }
    }
    m_synthesis.resize(kFftSize);
    for (int n = 0; n < kFftSize; ++n) {
        // irfft of FFTW is not normalized
        m_synthesis[n] = static_cast<float>(m_window[n] / den[n] / kFftSize);
    }
    const long spectrum = 2L * SpleeterNet::kFrames * kSpectrumBins;
    m_re.resize(spectrum);
    m_im.resize(spectrum);
    m_mag.resize(2L * SpleeterNet::kFrames * SpleeterNet::kNetBins);
    m_mask.resize(kStems * m_mag.size());
    m_out.resize(kStems * 2L * (kBlockHops + kShared));
}

SpleeterSeparator::~SpleeterSeparator() = default;

void SpleeterSeparator::setTrack(const float* interleaved, long frames) {
    m_frames = frames;
    m_track.assign(interleaved, interleaved + frames * 2);
    // tf.signal.stft(pad_end=True): ceil(length / hop) frames
    m_stftFrames = (kLeadIn + frames + kStftHop - 1) / kStftHop;
    m_blocks = static_cast<int>((m_stftFrames + SpleeterNet::kFrames - 1) / SpleeterNet::kFrames);
    m_done.assign(m_blocks, false);
    m_boundaries.clear();
}

int SpleeterSeparator::doneCount() const {
    return static_cast<int>(std::count(m_done.begin(), m_done.end(), true));
}

int SpleeterSeparator::segmentAt(long frame) const {
    const long hop = (std::max(0L, frame) + kLeadIn) / kStftHop;
    return static_cast<int>(std::min<long>(hop / SpleeterNet::kFrames, m_blocks - 1));
}

void SpleeterSeparator::segmentRange(int segment, long* pStart, long* pEnd) const {
    const long padded = segment * kBlockHops;
    *pStart = std::max(0L, padded - kLeadIn);
    *pEnd = std::min(m_frames, padded + kBlockHops + kShared - kLeadIn);
}

void SpleeterSeparator::skipSegment(int segment) {
    m_done[segment] = true;
    for (const int boundary : {segment, segment + 1}) {
        if (boundary >= 1 && boundary < m_blocks) {
            Boundary& b = m_boundaries[boundary];
            b.abandoned = true;
            b.sum = std::vector<float>();
        }
    }
}

float SpleeterSeparator::sample(int channel, long padded) const {
    const long frame = padded - kLeadIn;
    if (frame < 0 || frame >= m_frames) {
        return 0.0f;
    }
    return m_track[frame * 2 + channel];
}

void SpleeterSeparator::emitStems(const float* planar,
        long stride,
        long paddedStart,
        long count,
        const Output& output) {
    // planar[(stem * 2 + channel) * stride + i] is padded sample paddedStart + i
    long first = std::max(0L, kLeadIn - paddedStart);
    const long last = std::min(count, m_frames + kLeadIn - paddedStart);
    constexpr long kChunk = 16384;
    m_emit.resize(kChunk * kStems * 2);
    while (first < last) {
        const long n = std::min(kChunk, last - first);
        for (long i = 0; i < n; ++i) {
            for (int sc = 0; sc < kStems * 2; ++sc) {
                m_emit[i * kStems * 2 + sc] = planar[sc * stride + first + i];
            }
        }
        output(paddedStart + first - kLeadIn, n, m_emit.data());
        first += n;
    }
}

void SpleeterSeparator::addToBoundary(int boundary,
        const float* planar,
        long stride,
        long offset,
        const Output& output) {
    Boundary& b = m_boundaries[boundary];
    if (b.abandoned) {
        return;
    }
    if (b.sum.empty()) {
        b.sum.assign(kStems * 2L * kShared, 0.0f);
    }
    for (int sc = 0; sc < kStems * 2; ++sc) {
        for (int i = 0; i < kShared; ++i) {
            b.sum[sc * kShared + i] += planar[sc * stride + offset + i];
        }
    }
    if (++b.parts == 2) {
        emitStems(b.sum.data(), kShared, boundary * kBlockHops, kShared, output);
        m_boundaries.erase(boundary);
    }
}

bool SpleeterSeparator::processSegment(int segment, const Output& output) {
    constexpr int kFrames = SpleeterNet::kFrames;
    constexpr int kNetBins = SpleeterNet::kNetBins;
    const long firstFrame = long(segment) * kFrames;
    const int frames = static_cast<int>(std::min<long>(kFrames, m_stftFrames - firstFrame));
    const long paddedStart = firstFrame * kStftHop;

    // Spectrum of the block and its magnitude below 11 kHz
    std::fill(m_mag.begin(), m_mag.end(), 0.0f);
    for (int c = 0; c < 2; ++c) {
        for (int t = 0; t < frames; ++t) {
            const long start = paddedStart + long(t) * kStftHop;
            for (int n = 0; n < kFftSize; ++n) {
                m_pFft->time[n] = sample(c, start + n) * m_window[n];
            }
            fftwf_execute(m_pFft->forward);
            const long row = (long(c) * kFrames + t) * kSpectrumBins;
            for (int k = 0; k < kSpectrumBins; ++k) {
                m_re[row + k] = m_pFft->freq[k][0];
                m_im[row + k] = m_pFft->freq[k][1];
            }
            float* mag = m_mag.data() + (long(c) * kFrames + t) * kNetBins;
            for (int k = 0; k < kNetBins; ++k) {
                mag[k] = std::hypot(m_pFft->freq[k][0], m_pFft->freq[k][1]);
            }
        }
    }
    if (!m_pNet->run(m_mag.data(), m_mask.data())) {
        return false;
    }

    // Masked spectra back to the time domain, overlap-added
    const long length = long(frames) * kStftHop + kShared;
    std::fill(m_out.begin(), m_out.end(), 0.0f);
    for (int s = 0; s < kStems; ++s) {
        for (int c = 0; c < 2; ++c) {
            float* out = m_out.data() + (long(s) * 2 + c) * length;
            for (int t = 0; t < frames; ++t) {
                const float* mask = m_mask.data() +
                        ((long(s) * 2 + c) * kFrames + t) * kNetBins;
                float mean = 0.0f;
                for (int k = 0; k < kNetBins; ++k) {
                    mean += mask[k];
                }
                mean /= kNetBins;
                const long row = (long(c) * kFrames + t) * kSpectrumBins;
                for (int k = 0; k < kSpectrumBins; ++k) {
                    const float m = k < kNetBins ? mask[k] : mean;
                    m_pFft->freq[k][0] = m_re[row + k] * m;
                    m_pFft->freq[k][1] = m_im[row + k] * m;
                }
                fftwf_execute(m_pFft->backward);
                float* dst = out + long(t) * kStftHop;
                for (int n = 0; n < kFftSize; ++n) {
                    dst[n] += m_pFft->time[n] * m_synthesis[n];
                }
            }
        }
    }

    // The samples of this block alone are final; the 3 hops shared with each
    // neighbour wait for it
    const bool first = segment == 0;
    const bool last = segment == m_blocks - 1;
    const long interiorStart = first ? 0 : kShared;
    const long interiorEnd = last ? length : long(frames) * kStftHop;
    if (interiorEnd > interiorStart) {
        emitStems(m_out.data() + interiorStart,
                length,
                paddedStart + interiorStart,
                interiorEnd - interiorStart,
                output);
    }
    if (!first) {
        addToBoundary(segment, m_out.data(), length, 0, output);
    }
    if (!last) {
        addToBoundary(segment + 1, m_out.data(), length, long(frames) * kStftHop, output);
    }
    m_done[segment] = true;
    return true;
}

} // namespace stems
} // namespace mixxx
