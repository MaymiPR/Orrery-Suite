#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "Convolver.h"
#include "FastMath.h"
#if defined (__SSE2__) || defined (_M_X64) || defined (_M_AMD64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <emmintrin.h>
 #define SC_V4_SSE 1
 #if defined (_M_X64) || defined (_M_AMD64) || defined (__x86_64__)
  #include <immintrin.h>
  #define SC_AVX 1               // 8-wide tiles where the processor has AVX (chosen at run time)
  #if defined (_MSC_VER) && ! defined (__clang__)
   #define SC_AVX_FN
  #else
   #define SC_AVX_FN __attribute__ ((target ("avx")))
  #endif
 #endif
#elif defined (__ARM_NEON) || defined (__ARM_NEON__)
 #include <arm_neon.h>
 #define SC_V4_NEON 1
#endif

// A learned module as a small WaveNet (nulltest/train_wn.py): y = Post(u + WaveNet(u)), u = Pre(x). Pre a short FIR,
// the WaveNet dilated causal convolutions (kernel 2) with gated activations, residual and skip connections, Post a
// long FIR (Convolver.h). The weights are ours, trained on the reference's input and output (written into
// LearnedModels.h by nulltest/export_models.py).
// No layer feeds back on itself, so the network runs a block at a time, layer by layer, each loop over time (the
// loops vectorise); each layer keeps its last `dilation` inputs for the next block. The two matrix products run in
// tiles of 4 or 2 outputs by 8 samples held in registers (V4; with AVX, by 16 samples in 8-wide registers), each
// input loaded once per tile, summing over the inputs in the same order as the plain loops (the same results: no
// fused multiply-adds).
namespace sc::dsp
{
#if SC_AVX
// the two matrix products of a layer in 8-wide registers (see LearnedWaveNet::block), over nT (a multiple of 16) samples
SC_AVX_FN inline void waveNetConvAvx (const float* rows, int rowLen, int d, const float* W, const float* B, float* zp, int C, int N, int nT)
{
    for (int o = 0; o < 2 * C; o += 4)
        for (int t = 0; t < nT; t += 16)
        {
            __m256 acc[4][2];
            for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = _mm256_set1_ps (B[o + k]);
            for (int c = 0; c < C; ++c)
            {
                const float* old = rows + c * rowLen + t; const float* now = old + d;
                const __m256 o0 = _mm256_loadu_ps (old), o1 = _mm256_loadu_ps (old + 8), n0 = _mm256_loadu_ps (now), n1 = _mm256_loadu_ps (now + 8);
                for (int k = 0; k < 4; ++k)
                {
                    const __m256 wo = _mm256_broadcast_ss (W + ((o + k) * C + c) * 2), wn = _mm256_broadcast_ss (W + ((o + k) * C + c) * 2 + 1);
                    acc[k][0] = _mm256_add_ps (acc[k][0], _mm256_add_ps (_mm256_mul_ps (wo, o0), _mm256_mul_ps (wn, n0)));
                    acc[k][1] = _mm256_add_ps (acc[k][1], _mm256_add_ps (_mm256_mul_ps (wo, o1), _mm256_mul_ps (wn, n1)));
                }
            }
            for (int k = 0; k < 4; ++k) { _mm256_storeu_ps (zp + (o + k) * N + t, acc[k][0]); _mm256_storeu_ps (zp + (o + k) * N + t + 8, acc[k][1]); }
        }
    _mm256_zeroupper();
}

SC_AVX_FN inline void waveNetResAvx (const float* gp, const float* RW, const float* RB, const float* SW, const float* SB, float* hp, float* sk, int C, int N, int nT)
{
    for (int o = 0; o < C; o += 2)
        for (int t = 0; t < nT; t += 16)
        {
            __m256 hacc[2][2], sacc[2][2];
            for (int k = 0; k < 2; ++k)
                for (int q = 0; q < 2; ++q)
                {
                    hacc[k][q] = _mm256_add_ps (_mm256_loadu_ps (hp + (o + k) * N + t + 8 * q), _mm256_set1_ps (RB[o + k]));
                    sacc[k][q] = _mm256_add_ps (_mm256_loadu_ps (sk + (o + k) * N + t + 8 * q), _mm256_set1_ps (SB[o + k]));
                }
            for (int c = 0; c < C; ++c)
            {
                const __m256 g0 = _mm256_loadu_ps (gp + c * N + t), g1 = _mm256_loadu_ps (gp + c * N + t + 8);
                for (int k = 0; k < 2; ++k)
                {
                    const __m256 rw = _mm256_broadcast_ss (RW + (o + k) * C + c), sw = _mm256_broadcast_ss (SW + (o + k) * C + c);
                    hacc[k][0] = _mm256_add_ps (hacc[k][0], _mm256_mul_ps (rw, g0)); hacc[k][1] = _mm256_add_ps (hacc[k][1], _mm256_mul_ps (rw, g1));
                    sacc[k][0] = _mm256_add_ps (sacc[k][0], _mm256_mul_ps (sw, g0)); sacc[k][1] = _mm256_add_ps (sacc[k][1], _mm256_mul_ps (sw, g1));
                }
            }
            for (int k = 0; k < 2; ++k)
                for (int q = 0; q < 2; ++q)
                {
                    _mm256_storeu_ps (hp + (o + k) * N + t + 8 * q, hacc[k][q]);
                    _mm256_storeu_ps (sk + (o + k) * N + t + 8 * q, sacc[k][q]);
                }
        }
    _mm256_zeroupper();
}
#endif

// four floats: SSE, NEON, or plain
struct V4
{
#if SC_V4_SSE
    __m128 v;
    static V4 load (const float* p) { return { _mm_loadu_ps (p) }; }
    static V4 all (float x) { return { _mm_set1_ps (x) }; }
    void store (float* p) const { _mm_storeu_ps (p, v); }
    friend V4 operator+ (V4 a, V4 b) { return { _mm_add_ps (a.v, b.v) }; }
    friend V4 operator* (V4 a, V4 b) { return { _mm_mul_ps (a.v, b.v) }; }
#elif SC_V4_NEON
    float32x4_t v;
    static V4 load (const float* p) { return { vld1q_f32 (p) }; }
    static V4 all (float x) { return { vdupq_n_f32 (x) }; }
    void store (float* p) const { vst1q_f32 (p, v); }
    friend V4 operator+ (V4 a, V4 b) { return { vaddq_f32 (a.v, b.v) }; }
    friend V4 operator* (V4 a, V4 b) { return { vmulq_f32 (a.v, b.v) }; }
#else
    float v[4];
    static V4 load (const float* p) { return { { p[0], p[1], p[2], p[3] } }; }
    static V4 all (float x) { return { { x, x, x, x } }; }
    void store (float* p) const { for (int k = 0; k < 4; ++k) p[k] = v[k]; }
    friend V4 operator+ (V4 a, V4 b) { for (int k = 0; k < 4; ++k) a.v[k] += b.v[k]; return a; }
    friend V4 operator* (V4 a, V4 b) { for (int k = 0; k < 4; ++k) a.v[k] *= b.v[k]; return a; }
#endif
};

struct WaveNetWeights
{
    int channels;
    std::vector<int> dilations;
    std::vector<float> pre, post;
    std::vector<float> inW, inB;                     // C x (1 + conds), C
    std::vector<std::vector<float>> convW, convB;     // per layer: (2C) x C x 2 (PyTorch layout), 2C
    std::vector<std::vector<float>> resW, resB, skipW, skipB;   // per layer: C x C, C
    std::vector<float> outW; float outB;             // 1 x C
    bool bypass = false;                             // y = Post(u) + WaveNet(u) (the network beside the long filter)
    int conds = 0;                                   // a control's value as an extra input (0 or 1; train_wn.py COND)
};

class LearnedWaveNet
{
public:
    static constexpr int maxBlock = 256;

    void prepare (const WaveNetWeights& w)
    {
        weights = &w;
        const int C = w.channels;
        jassert (C % 2 == 0);                // (the tiles: 4 of the 2C gate outputs, 2 of the C residual outputs)
        post.prepare (w.post);
        pre.assign (w.pre.size(), 0.0f);
        hist.clear();
        for (int d : w.dilations) hist.push_back (std::vector<float> ((size_t) C * (size_t) (d + maxBlock), 0.0f));
        u.assign ((size_t) maxBlock, 0.0f); y = u;
        h.assign ((size_t) (C * maxBlock), 0.0f); skip = h; g = h;
        z.assign ((size_t) (2 * C * maxBlock), 0.0f);
        // the weights repeated four times (a register's worth), so a tile loads each instead of broadcasting it
        auto splat = [] (const std::vector<float>& v) { std::vector<float> s (v.size() * 4); for (size_t i = 0; i < s.size(); ++i) s[i] = v[i / 4]; return s; };
        convS.clear(); resS.clear(); skipS.clear();
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            convS.push_back (splat (w.convW[l])); resS.push_back (splat (w.resW[l])); skipS.push_back (splat (w.skipW[l]));
        }
        reset();
    }
    void reset()
    {
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& r : hist) std::fill (r.begin(), r.end(), 0.0f);
        post.reset();
        prePos = 0;
    }
    // in place allowed
    // cond: the control's value per sample, for a model with conds = 1
    void process (const double* in, double* out, int n, const double* cond = nullptr)
    {
        for (int done = 0; done < n; done += maxBlock)
        {
            const int m = std::min (maxBlock, n - done);
            block (in + done, out + done, m, cond != nullptr ? cond + done : nullptr);
        }
    }

private:
    void block (const double* in, double* out, int n, const double* cond)
    {
        const auto& w = *weights;
        const int C = w.channels, K = (int) w.pre.size(), N = maxBlock;
#if SC_AVX
        static const bool avx = juce::SystemStats::hasAVX();
#else
        constexpr bool avx = false;
#endif
        // the tiles run past n to a multiple of their width (on stale, finite values; unused)
        const int n8 = avx ? (n + 15) & ~15 : (n + 7) & ~7;
        float* up = u.data(); float* hp = h.data(); float* sk = skip.data(); float* zp = z.data(); float* gp = g.data();
        for (int t = 0; t < n; ++t)
        {
            pre[(size_t) prePos] = (float) in[t];
            float s = 0;
            int j = prePos;
            for (int k = 0; k < K; ++k) { s += w.pre[(size_t) k] * pre[(size_t) j]; j = j == 0 ? K - 1 : j - 1; }
            prePos = (prePos + 1) % K;
            up[t] = s;
        }
        for (int c = 0; c < C; ++c)
        {
            float* hc = hp + c * N; float* sc = sk + c * N;
            const int I = 1 + w.conds;
            const float a = w.inW[(size_t) (c * I)], b = w.inB[(size_t) c];
            for (int t = 0; t < n8; ++t) { hc[t] = a * up[t] + b; sc[t] = 0; }
            if (w.conds > 0)
            {
                const float k = w.inW[(size_t) (c * I + 1)];
                for (int t = 0; t < n; ++t) hc[t] += k * (cond != nullptr ? (float) cond[t] : 0.0f);
            }
        }
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            const int d = w.dilations[l], rowLen = d + maxBlock;
            float* rows = hist[l].data();
            // the layer's input after its last `d` inputs: row c = [x(t - d) ... x(t - 1) | this block]
            for (int c = 0; c < C; ++c) std::memcpy (rows + c * rowLen + d, hp + c * N, sizeof (float) * (size_t) n);
            const float* W = convS[l].data(); const float* B = w.convB[l].data();
#if SC_AVX
            if (avx) waveNetConvAvx (rows, rowLen, d, w.convW[l].data(), B, zp, C, N, n8);
            else
#endif
            for (int o = 0; o < 2 * C; o += 4)
                for (int t = 0; t < n8; t += 8)
                {
                    V4 acc[4][2];
                    for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = V4::all (B[o + k]);
                    for (int c = 0; c < C; ++c)
                    {
                        const float* old = rows + c * rowLen + t; const float* now = old + d;
                        const V4 o0 = V4::load (old), o1 = V4::load (old + 4), n0 = V4::load (now), n1 = V4::load (now + 4);
                        for (int k = 0; k < 4; ++k)
                        {
                            const V4 wo = V4::load (W + ((o + k) * C + c) * 8), wn = V4::load (W + ((o + k) * C + c) * 8 + 4);
                            acc[k][0] = acc[k][0] + (wo * o0 + wn * n0);
                            acc[k][1] = acc[k][1] + (wo * o1 + wn * n1);
                        }
                    }
                    for (int k = 0; k < 4; ++k) { acc[k][0].store (zp + (o + k) * N + t); acc[k][1].store (zp + (o + k) * N + t + 4); }
                }
            for (int c = 0; c < C; ++c)
            {
                const float* za = zp + c * N; const float* zb = zp + (C + c) * N; float* gc = gp + c * N;
                for (int t = 0; t < n8; ++t) gc[t] = fastTanh (za[t]) * fastSigmoid (zb[t]);
            }
            const float* RW = resS[l].data(); const float* RB = w.resB[l].data();
            const float* SW = skipS[l].data(); const float* SB = w.skipB[l].data();
#if SC_AVX
            if (avx) waveNetResAvx (gp, w.resW[l].data(), RB, w.skipW[l].data(), SB, hp, sk, C, N, n8);
            else
#endif
            for (int o = 0; o < C; o += 2)
                for (int t = 0; t < n8; t += 8)
                {
                    V4 hacc[2][2], sacc[2][2];
                    for (int k = 0; k < 2; ++k)
                        for (int q = 0; q < 2; ++q)
                        {
                            hacc[k][q] = V4::load (hp + (o + k) * N + t + 4 * q) + V4::all (RB[o + k]);
                            sacc[k][q] = V4::load (sk + (o + k) * N + t + 4 * q) + V4::all (SB[o + k]);
                        }
                    for (int c = 0; c < C; ++c)
                    {
                        const V4 g0 = V4::load (gp + c * N + t), g1 = V4::load (gp + c * N + t + 4);
                        for (int k = 0; k < 2; ++k)
                        {
                            const V4 rw = V4::load (RW + ((o + k) * C + c) * 4), sw = V4::load (SW + ((o + k) * C + c) * 4);
                            hacc[k][0] = hacc[k][0] + rw * g0; hacc[k][1] = hacc[k][1] + rw * g1;
                            sacc[k][0] = sacc[k][0] + sw * g0; sacc[k][1] = sacc[k][1] + sw * g1;
                        }
                    }
                    for (int k = 0; k < 2; ++k)
                        for (int q = 0; q < 2; ++q)
                        {
                            hacc[k][q].store (hp + (o + k) * N + t + 4 * q);
                            sacc[k][q].store (sk + (o + k) * N + t + 4 * q);
                        }
                }
            // keep the last d inputs for the next block
            for (int c = 0; c < C; ++c) std::memmove (rows + c * rowLen, rows + c * rowLen + n, sizeof (float) * (size_t) d);
        }
        float* yp = y.data();
        for (int t = 0; t < n; ++t) yp[t] = w.outB;
        for (int c = 0; c < C; ++c)
        {
            const float a = w.outW[(size_t) c]; const float* sc = sk + c * N;
            for (int t = 0; t < n; ++t) yp[t] += a * std::max (0.0f, sc[t]);
        }
        if (w.bypass) for (int t = 0; t < n; ++t) out[t] = post.process (up[t]) + yp[t];
        else for (int t = 0; t < n; ++t) out[t] = post.process (up[t] + yp[t]);
    }

    const WaveNetWeights* weights = nullptr;
    Convolver post;
    std::vector<float> pre, u, y, h, skip, z, g;
    std::vector<std::vector<float>> hist, convS, resS, skipS;
    int prePos = 0;
};
}
