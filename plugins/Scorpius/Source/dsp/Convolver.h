#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <vector>
#if defined (_M_X64) || defined (_M_AMD64) || defined (__x86_64__)
 #include <immintrin.h>
 #define SC_CONV_WIDE 1
 #if defined (_MSC_VER) && ! defined (__clang__)
  #define SC_CONV_FMA_FN
  #define SC_CONV_512_FN
 #else
  #define SC_CONV_FMA_FN __attribute__ ((target ("avx2,fma")))
  #define SC_CONV_512_FN __attribute__ ((target ("avx512f")))
 #endif
#endif

// A long FIR filter without latency: its first B taps directly, the rest by uniformly partitioned FFT convolution
// in blocks of B (those taps start B samples late, so a block's FFT part is ready in time). For the learned models'
// long linear filters (Learned.h).
// A block of samples at a time (processBlock) the direct taps run across the samples, and the partitions' products
// over separate real and imaginary arrays, so both loops vectorise; the sums are taken in the same order as one sample
// at a time (process), so both give the same results. Where the processor has AVX2 and FMA, or AVX-512, those two
// loops run 8 or 16 wide with fused multiply-adds (the results then differ in the last bits, ~-140 dB).
namespace sc::dsp
{
#if SC_CONV_WIDE
// o[i] += h * x[i] over m samples; the partitions' complex multiply-adds over n bins (a multiple of 16)
SC_CONV_512_FN inline void convTapsWide512 (float* o, const float* head, const float* cur, int B, int m)
{
    int i = 0;
    for (; i + 16 <= m; i += 16)
    {
        __m512 acc = _mm512_setzero_ps();
        for (int k = 0; k < B; ++k) acc = _mm512_fmadd_ps (_mm512_set1_ps (head[k]), _mm512_loadu_ps (cur + i - k), acc);
        _mm512_storeu_ps (o + i, acc);
    }
    for (; i < m; ++i) { float a = 0; for (int k = 0; k < B; ++k) a += head[k] * cur[i - k]; o[i] = a; }
}
SC_CONV_FMA_FN inline void convTapsWideFma (float* o, const float* head, const float* cur, int B, int m)
{
    int i = 0;
    for (; i + 8 <= m; i += 8)
    {
        __m256 acc = _mm256_setzero_ps();
        for (int k = 0; k < B; ++k) acc = _mm256_fmadd_ps (_mm256_broadcast_ss (head + k), _mm256_loadu_ps (cur + i - k), acc);
        _mm256_storeu_ps (o + i, acc);
    }
    for (; i < m; ++i) { float a = 0; for (int k = 0; k < B; ++k) a += head[k] * cur[i - k]; o[i] = a; }
    _mm256_zeroupper();
}
SC_CONV_512_FN inline void convBins512 (float* ar, float* ai, const float* ir, const float* ii, const float* hr, const float* hi, int n)
{
    for (int k = 0; k < n; k += 16)
    {
        const __m512 r = _mm512_loadu_ps (ir + k), i = _mm512_loadu_ps (ii + k), a = _mm512_loadu_ps (hr + k), b = _mm512_loadu_ps (hi + k);
        _mm512_storeu_ps (ar + k, _mm512_add_ps (_mm512_loadu_ps (ar + k), _mm512_fmsub_ps (r, a, _mm512_mul_ps (i, b))));
        _mm512_storeu_ps (ai + k, _mm512_add_ps (_mm512_loadu_ps (ai + k), _mm512_fmadd_ps (r, b, _mm512_mul_ps (i, a))));
    }
}
SC_CONV_FMA_FN inline void convBinsFma (float* ar, float* ai, const float* ir, const float* ii, const float* hr, const float* hi, int n)
{
    for (int k = 0; k < n; k += 8)
    {
        const __m256 r = _mm256_loadu_ps (ir + k), i = _mm256_loadu_ps (ii + k), a = _mm256_loadu_ps (hr + k), b = _mm256_loadu_ps (hi + k);
        _mm256_storeu_ps (ar + k, _mm256_add_ps (_mm256_loadu_ps (ar + k), _mm256_fmsub_ps (r, a, _mm256_mul_ps (i, b))));
        _mm256_storeu_ps (ai + k, _mm256_add_ps (_mm256_loadu_ps (ai + k), _mm256_fmadd_ps (r, b, _mm256_mul_ps (i, a))));
    }
    _mm256_zeroupper();
}
#endif

class Convolver
{
public:
    static constexpr int order = 7, B = 1 << (order - 1), N = 2 * B;      // block 64, FFT 128
    static constexpr int bins = B + 1, binsPadded = (bins + 15) & ~15;

    void prepare (const std::vector<float>& taps)
    {
        head.assign (taps.begin(), taps.begin() + (std::ptrdiff_t) std::min<size_t> ((size_t) B, taps.size()));
        head.resize ((size_t) B, 0.0f);
        const int rest = (int) taps.size() - B;
        parts = std::max (0, (rest + B - 1) / B);
        spectraRe.assign ((size_t) parts, std::vector<float> ((size_t) binsPadded, 0.0f));
        spectraIm = spectraRe;
        for (int p = 0; p < parts; ++p)
        {
            std::vector<float> buf ((size_t) 2 * N, 0.0f);
            for (int k = 0; k < B; ++k)
            {
                const size_t idx = (size_t) (B + p * B + k);
                if (idx < taps.size()) buf[(size_t) k] = taps[idx];
            }
            fft.performRealOnlyForwardTransform (buf.data(), true);
            for (int k = 0; k < bins; ++k) { spectraRe[(size_t) p][(size_t) k] = buf[(size_t) (2 * k)]; spectraIm[(size_t) p][(size_t) k] = buf[(size_t) (2 * k + 1)]; }
        }
        inputRe.assign ((size_t) std::max (1, parts), std::vector<float> ((size_t) binsPadded, 0.0f));
        inputIm = inputRe;
        work.assign ((size_t) 2 * N, 0.0f);
        accRe.assign ((size_t) binsPadded, 0.0f); accIm = accRe;
        out.assign ((size_t) B, 0.0f);
        reset();
    }
    void reset()
    {
        block.assign ((size_t) N, 0.0f);
        tail.assign ((size_t) B, 0.0f);
        for (auto* s : { &inputRe, &inputIm }) for (auto& v : *s) std::fill (v.begin(), v.end(), 0.0f);
        pos = 0; newest = 0;
    }
    // keeps the current state (prepare time), and goes back to it without allocating (the audio thread)
    void save() { saved = { block, tail, inputRe, inputIm, pos, newest }; }
    void restore()
    {
        std::copy (saved.block.begin(), saved.block.end(), block.begin());
        std::copy (saved.tail.begin(), saved.tail.end(), tail.begin());
        for (size_t p = 0; p < inputRe.size(); ++p)
        {
            std::copy (saved.re[p].begin(), saved.re[p].end(), inputRe[p].begin());
            std::copy (saved.im[p].begin(), saved.im[p].end(), inputIm[p].begin());
        }
        pos = saved.pos; newest = saved.newest;
    }
    float process (float x)
    {
        // (one sample: the plain loop, in the same order)
        block[(size_t) (B + pos)] = x;
        const float* cur = block.data() + B + pos;
        float y = 0;
        for (int k = 0; k < B; ++k) y += head[(size_t) k] * cur[-k];
        y += tail[(size_t) pos];
        if (++pos == B)
        {
            pos = 0;
            runBlock();
            std::copy (block.begin() + B, block.end(), block.begin());
        }
        return y;
    }
    // in and out may be the same
    void processBlock (const float* in, float* y, int n)
    {
        for (int done = 0; done < n;)
        {
            // up to the next block boundary: the direct taps over this stretch, then the partitions' share
            const int m = std::min (n - done, B - pos);
            float* cur = block.data() + B + pos;          // block: the last block, then this one so far
            std::copy (in + done, in + done + m, cur);
            float* o = out.data();
#if SC_CONV_WIDE
            if (wide () == 3) convTapsWide512 (o, head.data(), cur, B, m);
            else if (wide () == 2) convTapsWideFma (o, head.data(), cur, B, m);
            else
#endif
            {
                std::fill (o, o + m, 0.0f);
                for (int k = 0; k < B; ++k)
                {
                    const float h = head[(size_t) k];
                    const float* x = cur - k;
                    for (int i = 0; i < m; ++i) o[i] += h * x[i];
                }
            }
            for (int i = 0; i < m; ++i) y[done + i] = o[i] + tail[(size_t) (pos + i)];
            pos += m; done += m;
            if (pos == B)
            {
                pos = 0;
                runBlock();
                std::copy (block.begin() + B, block.end(), block.begin());     // (overlap-save: keep the last block)
            }
        }
    }

private:
    // (0: plain, 2: AVX2 and FMA, 3: AVX-512)
    static int wide()
    {
#if SC_CONV_WIDE
        static const int w = juce::SystemStats::hasAVX512F() ? 3 : juce::SystemStats::hasAVX2() && juce::SystemStats::hasFMA3() ? 2 : 0;
        return w;
#else
        return 0;
#endif
    }

    void runBlock()
    {
        if (parts == 0) { std::fill (tail.begin(), tail.end(), 0.0f); return; }
        auto& buf = work;                                                // (no allocation on the audio thread)
        std::fill (buf.begin(), buf.end(), 0.0f);
        std::copy (block.begin(), block.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        newest = (newest + 1) % parts;
        float* sr = inputRe[(size_t) newest].data(); float* si = inputIm[(size_t) newest].data();
        for (int k = 0; k < bins; ++k) { sr[k] = buf[(size_t) (2 * k)]; si[k] = buf[(size_t) (2 * k + 1)]; }
        float* ar = accRe.data(); float* ai = accIm.data();
        std::fill (ar, ar + binsPadded, 0.0f); std::fill (ai, ai + binsPadded, 0.0f);
        for (int p = 0; p < parts; ++p)
        {
            const int q = (newest - p + parts) % parts;
            const float* ir = inputRe[(size_t) q].data(); const float* ii = inputIm[(size_t) q].data();
            const float* hr = spectraRe[(size_t) p].data(); const float* hi = spectraIm[(size_t) p].data();
#if SC_CONV_WIDE
            if (wide () == 3) { convBins512 (ar, ai, ir, ii, hr, hi, binsPadded); continue; }
            if (wide () == 2) { convBinsFma (ar, ai, ir, ii, hr, hi, binsPadded); continue; }
#endif
            for (int k = 0; k < binsPadded; ++k)
            {
                ar[k] += ir[k] * hr[k] - ii[k] * hi[k];
                ai[k] += ir[k] * hi[k] + ii[k] * hr[k];
            }
        }
        for (int k = 0; k < bins; ++k) { buf[(size_t) (2 * k)] = ar[k]; buf[(size_t) (2 * k + 1)] = ai[k]; }
        fft.performRealOnlyInverseTransform (buf.data());
        // overlap-save: the second half is valid; it is the partitions' output for the next B samples
        for (int k = 0; k < B; ++k) tail[(size_t) k] = buf[(size_t) (B + k)];
    }

    juce::dsp::FFT fft { order };
    std::vector<float> head, block, tail, work, accRe, accIm, out;
    std::vector<std::vector<float>> spectraRe, spectraIm, inputRe, inputIm;
    int parts = 0, pos = 0, newest = 0;
    struct State
    {
        std::vector<float> block, tail;
        std::vector<std::vector<float>> re, im;
        int pos = 0, newest = 0;
    } saved;
};
}
