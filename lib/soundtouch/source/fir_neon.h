// NEON versions of the SoundTouch FIR filter inner loops (float samples).
//
// SoundTouch 2.3.3 evaluates the multichannel filter (stems: 8 interleaved
// channels) with a plain C loop whose channel loop has a run time length: the
// compiler cannot keep the sums in registers, and on the phone this was 49 %
// of the audio thread (simpleperf, 2 decks with stems, 19/11/2026).
//
//   dest[f * C + c] = sum_{i < length} coeffs[i] * src[(f + i) * C + c]
//   for f in [0, numFrames - length), the same as FIRFilter::evaluateFilterMulti
//
// Header only so that tools/fir-neon/fir_bench.cpp (benchmark on the phone) and
// the Mixxx build use the same code.
#pragma once

#include <cstddef>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define MIXXX_FIR_NEON 1
#endif

namespace mixxx {
namespace fir {

// Reference (what SoundTouch does): any channel count < 16
inline unsigned referenceMulti(float* dest,
        const float* src,
        unsigned numFrames,
        unsigned numChannels,
        const float* coeffs,
        unsigned length) {
    const int ilength = static_cast<int>(length & ~7u);
    const int end = static_cast<int>(numChannels * (numFrames - ilength));
    for (int j = 0; j < end; j += static_cast<int>(numChannels)) {
        float sums[16];
        for (unsigned c = 0; c < numChannels; c++) {
            sums[c] = 0;
        }
        const float* ptr = src + j;
        for (int i = 0; i < ilength; i++) {
            const float coef = coeffs[i];
            for (unsigned c = 0; c < numChannels; c++) {
                sums[c] += ptr[0] * coef;
                ptr++;
            }
        }
        for (unsigned c = 0; c < numChannels; c++) {
            dest[j + c] = sums[c];
        }
    }
    return numFrames - ilength;
}

// Reference of FIRFilter::evaluateFilterStereo (stereoCoeffs = each coefficient twice)
inline unsigned referenceStereo(float* dest,
        const float* src,
        unsigned numFrames,
        const float* stereoCoeffs,
        unsigned length) {
    const unsigned ilength = length & ~7u;
    const int end = static_cast<int>(2 * (numFrames - ilength));
    for (int j = 0; j < end; j += 2) {
        const float* ptr = src + j;
        float suml = 0, sumr = 0;
        for (unsigned i = 0; i < ilength; i++) {
            suml += ptr[2 * i] * stereoCoeffs[2 * i];
            sumr += ptr[2 * i + 1] * stereoCoeffs[2 * i + 1];
        }
        dest[j] = suml;
        dest[j + 1] = sumr;
    }
    return numFrames - ilength;
}

#ifdef MIXXX_FIR_NEON

// Stereo: a 4 float vector holds two frames (L R L R); with the vector loaded
// at frame f + i, lanes 0-1 are the tap i of the output frame f and lanes 2-3
// the tap i of the output frame f + 1. Eight output frames per round.
inline unsigned neonStereo(float* dest,
        const float* src,
        unsigned numFrames,
        const float* coeffs,
        unsigned length) {
    const int ilength = static_cast<int>(length & ~7u);
    const unsigned n = numFrames - ilength;
    unsigned f = 0;
    for (; f + 8 <= n; f += 8) {
        float32x4_t acc0 = vdupq_n_f32(0.0f), acc1 = acc0, acc2 = acc0, acc3 = acc0;
        const float* p = src + static_cast<size_t>(f) * 2;
        for (int i = 0; i < ilength; i++) {
            const float32x4_t coef = vdupq_n_f32(coeffs[i]);
            const float* q = p + static_cast<size_t>(i) * 2;
            acc0 = vfmaq_f32(acc0, vld1q_f32(q), coef);
            acc1 = vfmaq_f32(acc1, vld1q_f32(q + 4), coef);
            acc2 = vfmaq_f32(acc2, vld1q_f32(q + 8), coef);
            acc3 = vfmaq_f32(acc3, vld1q_f32(q + 12), coef);
        }
        float* d = dest + static_cast<size_t>(f) * 2;
        vst1q_f32(d, acc0);
        vst1q_f32(d + 4, acc1);
        vst1q_f32(d + 8, acc2);
        vst1q_f32(d + 12, acc3);
    }
    // 0..7 frames left, one frame at a time
    for (; f < n; f++) {
        float32x2_t acc = vdup_n_f32(0.0f);
        const float* p = src + static_cast<size_t>(f) * 2;
        for (int i = 0; i < ilength; i++) {
            acc = vfma_n_f32(acc, vld1_f32(p + static_cast<size_t>(i) * 2), coeffs[i]);
        }
        vst1_f32(dest + static_cast<size_t>(f) * 2, acc);
    }
    return n;
}

// NV = number of 4 float vectors per frame (channels = 4 * NV). Four output
// frames at a time: 4 * NV independent accumulators hide the latency of the
// multiply-add, and every coefficient is read once for the four frames.
template<int NV>
inline void multiVec(float* dest,
        const float* src,
        unsigned outFrames,
        const float* coeffs,
        int ilength) {
    constexpr int C = 4 * NV;
    unsigned f = 0;
    for (; f + 4 <= outFrames; f += 4) {
        float32x4_t acc[4][NV];
        for (int r = 0; r < 4; r++) {
            for (int v = 0; v < NV; v++) {
                acc[r][v] = vdupq_n_f32(0.0f);
            }
        }
        const float* p = src + static_cast<size_t>(f) * C;
        for (int i = 0; i < ilength; i++) {
            const float32x4_t coef = vdupq_n_f32(coeffs[i]);
            const float* q = p + static_cast<size_t>(i) * C;
            for (int r = 0; r < 4; r++) {
                for (int v = 0; v < NV; v++) {
                    acc[r][v] = vfmaq_f32(acc[r][v], vld1q_f32(q + r * C + 4 * v), coef);
                }
            }
        }
        float* d = dest + static_cast<size_t>(f) * C;
        for (int r = 0; r < 4; r++) {
            for (int v = 0; v < NV; v++) {
                vst1q_f32(d + r * C + 4 * v, acc[r][v]);
            }
        }
    }
    // The last 0..3 frames
    for (; f < outFrames; f++) {
        float32x4_t acc[NV];
        for (int v = 0; v < NV; v++) {
            acc[v] = vdupq_n_f32(0.0f);
        }
        const float* p = src + static_cast<size_t>(f) * C;
        for (int i = 0; i < ilength; i++) {
            const float32x4_t coef = vdupq_n_f32(coeffs[i]);
            for (int v = 0; v < NV; v++) {
                acc[v] = vfmaq_f32(acc[v], vld1q_f32(p + static_cast<size_t>(i) * C + 4 * v), coef);
            }
        }
        for (int v = 0; v < NV; v++) {
            vst1q_f32(dest + static_cast<size_t>(f) * C + 4 * v, acc[v]);
        }
    }
}

// Same as referenceMulti. Channel counts that are a multiple of 4 (4, 8, 12,
// 16) use NEON; false when this channel count is not supported (the caller
// then uses the reference).
inline bool neonMulti(float* dest,
        const float* src,
        unsigned numFrames,
        unsigned numChannels,
        const float* coeffs,
        unsigned length,
        unsigned* outFrames) {
    const int ilength = static_cast<int>(length & ~7u);
    if (ilength <= 0 || numFrames <= static_cast<unsigned>(ilength)) {
        return false;
    }
    const unsigned n = numFrames - ilength;
    switch (numChannels) {
    case 4:
        multiVec<1>(dest, src, n, coeffs, ilength);
        break;
    case 8:
        multiVec<2>(dest, src, n, coeffs, ilength);
        break;
    case 12:
        multiVec<3>(dest, src, n, coeffs, ilength);
        break;
    default:
        return false;
    }
    *outFrames = n;
    return true;
}

#endif // MIXXX_FIR_NEON

} // namespace fir
} // namespace mixxx
