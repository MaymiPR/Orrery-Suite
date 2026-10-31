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
  #define SC_AVX 1               // 8-wide tiles where the processor has AVX, 8-wide with fused multiply-adds where it has
                                 // AVX2 and FMA, 16-wide where it has AVX-512 (chosen at run time)
  #if defined (_MSC_VER) && ! defined (__clang__)
   #define SC_AVX_FN
   #define SC_FMA_FN
   #define SC_AVX512_FN
  #else
   #define SC_AVX_FN __attribute__ ((target ("avx")))
   #define SC_FMA_FN __attribute__ ((target ("avx2,fma")))
   #define SC_AVX512_FN __attribute__ ((target ("avx512f")))
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
// fused multiply-adds). Where the processor has AVX2 and FMA, or AVX-512, the same tiles run with fused multiply-adds
// (16 or 32 samples wide), and the gated activations too: about twice and four times as fast, the output differing
// from the plain loops' only in the last bits of each float.
namespace sc::dsp
{
#if SC_AVX
// the two matrix products of a layer in 8-wide registers (see LearnedWaveNet::block), over nT (a multiple of 16) samples
SC_AVX_FN inline void waveNetConvAvx (const float* rows, int rowLen, int d, int K, const float* W, const float* B, float* zp, int C, int N, int nT)
{
    for (int o = 0; o < 2 * C; o += 4)
        for (int t = 0; t < nT; t += 16)
        {
            __m256 acc[4][2];
            for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = _mm256_set1_ps (B[o + k]);
            for (int c = 0; c < C; ++c)
            {
                __m256 x[3][2];
                for (int j = 0; j < K; ++j)
                {
                    const float* p = rows + c * rowLen + t + j * d;
                    x[j][0] = _mm256_loadu_ps (p); x[j][1] = _mm256_loadu_ps (p + 8);
                }
                for (int k = 0; k < 4; ++k)
                {
                    const float* wk = W + ((o + k) * C + c) * K;
                    for (int q = 0; q < 2; ++q)
                    {
                        __m256 s = _mm256_add_ps (_mm256_mul_ps (_mm256_broadcast_ss (wk), x[0][q]), _mm256_mul_ps (_mm256_broadcast_ss (wk + 1), x[1][q]));
                        if (K > 2) s = _mm256_add_ps (s, _mm256_mul_ps (_mm256_broadcast_ss (wk + 2), x[2][q]));
                        acc[k][q] = _mm256_add_ps (acc[k][q], s);
                    }
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

// the same with fused multiply-adds (each product added in one rounding instead of two: the results differ from the
// plain loops' only in the last bits, ~-140 dB), 8 wide (AVX2 and FMA) or 16 wide (AVX-512); the gated activations too
#if SC_AVX
SC_FMA_FN inline void waveNetConvFma (const float* rows, int rowLen, int d, int K, const float* W, const float* B, float* zp, int C, int N, int nT)
{
    if (K == 3)
    {
        // (kernel 3, the drive's and the compressor's: the same, three taps)
        for (int o = 0; o < 2 * C; o += 4)
            for (int t = 0; t < nT; t += 16)
            {
                __m256 a0 = _mm256_set1_ps (B[o]), b0 = a0, a1 = _mm256_set1_ps (B[o + 1]), b1 = a1;
                __m256 a2 = _mm256_set1_ps (B[o + 2]), b2 = a2, a3 = _mm256_set1_ps (B[o + 3]), b3 = a3;
                for (int c = 0; c < C; ++c)
                {
                    const float* p = rows + c * rowLen + t;
                    const __m256 x0 = _mm256_loadu_ps (p), y0 = _mm256_loadu_ps (p + 8), x1 = _mm256_loadu_ps (p + d), y1 = _mm256_loadu_ps (p + d + 8), x2 = _mm256_loadu_ps (p + 2 * d), y2 = _mm256_loadu_ps (p + 2 * d + 8);
                    const float* w0 = W + (o * C + c) * 3; const float* w1 = w0 + 3 * C; const float* w2 = w1 + 3 * C; const float* w3 = w2 + 3 * C;
                    { const __m256 w = _mm256_broadcast_ss (w0 + 0); a0 = _mm256_fmadd_ps (w, x0, a0); b0 = _mm256_fmadd_ps (w, y0, b0); }
                    { const __m256 w = _mm256_broadcast_ss (w0 + 1); a0 = _mm256_fmadd_ps (w, x1, a0); b0 = _mm256_fmadd_ps (w, y1, b0); }
                    { const __m256 w = _mm256_broadcast_ss (w0 + 2); a0 = _mm256_fmadd_ps (w, x2, a0); b0 = _mm256_fmadd_ps (w, y2, b0); }
                    { const __m256 w = _mm256_broadcast_ss (w1 + 0); a1 = _mm256_fmadd_ps (w, x0, a1); b1 = _mm256_fmadd_ps (w, y0, b1); }
                    { const __m256 w = _mm256_broadcast_ss (w1 + 1); a1 = _mm256_fmadd_ps (w, x1, a1); b1 = _mm256_fmadd_ps (w, y1, b1); }
                    { const __m256 w = _mm256_broadcast_ss (w1 + 2); a1 = _mm256_fmadd_ps (w, x2, a1); b1 = _mm256_fmadd_ps (w, y2, b1); }
                    { const __m256 w = _mm256_broadcast_ss (w2 + 0); a2 = _mm256_fmadd_ps (w, x0, a2); b2 = _mm256_fmadd_ps (w, y0, b2); }
                    { const __m256 w = _mm256_broadcast_ss (w2 + 1); a2 = _mm256_fmadd_ps (w, x1, a2); b2 = _mm256_fmadd_ps (w, y1, b2); }
                    { const __m256 w = _mm256_broadcast_ss (w2 + 2); a2 = _mm256_fmadd_ps (w, x2, a2); b2 = _mm256_fmadd_ps (w, y2, b2); }
                    { const __m256 w = _mm256_broadcast_ss (w3 + 0); a3 = _mm256_fmadd_ps (w, x0, a3); b3 = _mm256_fmadd_ps (w, y0, b3); }
                    { const __m256 w = _mm256_broadcast_ss (w3 + 1); a3 = _mm256_fmadd_ps (w, x1, a3); b3 = _mm256_fmadd_ps (w, y1, b3); }
                    { const __m256 w = _mm256_broadcast_ss (w3 + 2); a3 = _mm256_fmadd_ps (w, x2, a3); b3 = _mm256_fmadd_ps (w, y2, b3); }
                }
                float* z = zp + o * N + t;
                _mm256_storeu_ps (z, a0); _mm256_storeu_ps (z + 8, b0); z += N;
                _mm256_storeu_ps (z, a1); _mm256_storeu_ps (z + 8, b1); z += N;
                _mm256_storeu_ps (z, a2); _mm256_storeu_ps (z + 8, b2); z += N;
                _mm256_storeu_ps (z, a3); _mm256_storeu_ps (z + 8, b3);
            }
        _mm256_zeroupper();
        return;
    }
    if (K == 2)
    {
        for (int o = 0; o < 2 * C; o += 4)
            for (int t = 0; t < nT; t += 16)
            {
                __m256 a0 = _mm256_set1_ps (B[o]), b0 = a0, a1 = _mm256_set1_ps (B[o + 1]), b1 = a1;
                __m256 a2 = _mm256_set1_ps (B[o + 2]), b2 = a2, a3 = _mm256_set1_ps (B[o + 3]), b3 = a3;
                for (int c = 0; c < C; ++c)
                {
                    const float* p = rows + c * rowLen + t;
                    const __m256 x0 = _mm256_loadu_ps (p), y0 = _mm256_loadu_ps (p + 8), x1 = _mm256_loadu_ps (p + d), y1 = _mm256_loadu_ps (p + d + 8);
                    const float* w0 = W + (o * C + c) * 2; const float* w1 = w0 + 2 * C; const float* w2 = w1 + 2 * C; const float* w3 = w2 + 2 * C;
                    __m256 w = _mm256_broadcast_ss (w0); a0 = _mm256_fmadd_ps (w, x0, a0); b0 = _mm256_fmadd_ps (w, y0, b0);
                    w = _mm256_broadcast_ss (w0 + 1); a0 = _mm256_fmadd_ps (w, x1, a0); b0 = _mm256_fmadd_ps (w, y1, b0);
                    w = _mm256_broadcast_ss (w1); a1 = _mm256_fmadd_ps (w, x0, a1); b1 = _mm256_fmadd_ps (w, y0, b1);
                    w = _mm256_broadcast_ss (w1 + 1); a1 = _mm256_fmadd_ps (w, x1, a1); b1 = _mm256_fmadd_ps (w, y1, b1);
                    w = _mm256_broadcast_ss (w2); a2 = _mm256_fmadd_ps (w, x0, a2); b2 = _mm256_fmadd_ps (w, y0, b2);
                    w = _mm256_broadcast_ss (w2 + 1); a2 = _mm256_fmadd_ps (w, x1, a2); b2 = _mm256_fmadd_ps (w, y1, b2);
                    w = _mm256_broadcast_ss (w3); a3 = _mm256_fmadd_ps (w, x0, a3); b3 = _mm256_fmadd_ps (w, y0, b3);
                    w = _mm256_broadcast_ss (w3 + 1); a3 = _mm256_fmadd_ps (w, x1, a3); b3 = _mm256_fmadd_ps (w, y1, b3);
                }
                float* z = zp + o * N + t;
                _mm256_storeu_ps (z, a0); _mm256_storeu_ps (z + 8, b0); z += N;
                _mm256_storeu_ps (z, a1); _mm256_storeu_ps (z + 8, b1); z += N;
                _mm256_storeu_ps (z, a2); _mm256_storeu_ps (z + 8, b2); z += N;
                _mm256_storeu_ps (z, a3); _mm256_storeu_ps (z + 8, b3);
            }
        _mm256_zeroupper();
        return;
    }
    for (int o = 0; o < 2 * C; o += 4)
        for (int t = 0; t < nT; t += 16)
        {
            __m256 acc[4][2];
            for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = _mm256_set1_ps (B[o + k]);
            for (int c = 0; c < C; ++c)
            {
                __m256 x[3][2];
                for (int j = 0; j < K; ++j)
                {
                    const float* p = rows + c * rowLen + t + j * d;
                    x[j][0] = _mm256_loadu_ps (p); x[j][1] = _mm256_loadu_ps (p + 8);
                }
                for (int k = 0; k < 4; ++k)
                {
                    const float* wk = W + ((o + k) * C + c) * K;
                    for (int j = 0; j < K; ++j)
                    {
                        const __m256 wj = _mm256_broadcast_ss (wk + j);
                        acc[k][0] = _mm256_fmadd_ps (wj, x[j][0], acc[k][0]);
                        acc[k][1] = _mm256_fmadd_ps (wj, x[j][1], acc[k][1]);
                    }
                }
            }
            for (int k = 0; k < 4; ++k) { _mm256_storeu_ps (zp + (o + k) * N + t, acc[k][0]); _mm256_storeu_ps (zp + (o + k) * N + t + 8, acc[k][1]); }
        }
    _mm256_zeroupper();
}

SC_FMA_FN inline void waveNetResFma (const float* gp, const float* RW, const float* RB, const float* SW, const float* SB, float* hp, float* sk, int C, int N, int nT)
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
                    hacc[k][0] = _mm256_fmadd_ps (rw, g0, hacc[k][0]); hacc[k][1] = _mm256_fmadd_ps (rw, g1, hacc[k][1]);
                    sacc[k][0] = _mm256_fmadd_ps (sw, g0, sacc[k][0]); sacc[k][1] = _mm256_fmadd_ps (sw, g1, sacc[k][1]);
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

// the gate, g = tanh (a) sigmoid (b), 8 at a time (FastMath.h's approximations)
SC_FMA_FN inline __m256 tanh8 (__m256 x)
{
    x = _mm256_min_ps (_mm256_max_ps (x, _mm256_set1_ps (-4.97f)), _mm256_set1_ps (4.97f));
    const __m256 x2 = _mm256_mul_ps (x, x);
    const __m256 num = _mm256_mul_ps (x, _mm256_fmadd_ps (x2, _mm256_fmadd_ps (x2, _mm256_add_ps (_mm256_set1_ps (378.0f), x2), _mm256_set1_ps (17325.0f)), _mm256_set1_ps (135135.0f)));
    const __m256 den = _mm256_fmadd_ps (x2, _mm256_fmadd_ps (x2, _mm256_fmadd_ps (x2, _mm256_set1_ps (28.0f), _mm256_set1_ps (3150.0f)), _mm256_set1_ps (62370.0f)), _mm256_set1_ps (135135.0f));
    return _mm256_div_ps (num, den);
}

SC_FMA_FN inline void waveNetGateFma (const float* zp, float* gp, int C, int N, int nT)
{
    const __m256 half = _mm256_set1_ps (0.5f);
    for (int c = 0; c < C; ++c)
        for (int t = 0; t < nT; t += 8)
        {
            const __m256 a = tanh8 (_mm256_loadu_ps (zp + c * N + t));
            const __m256 b = _mm256_fmadd_ps (half, tanh8 (_mm256_mul_ps (half, _mm256_loadu_ps (zp + (C + c) * N + t))), half);
            _mm256_storeu_ps (gp + c * N + t, _mm256_mul_ps (a, b));
        }
    _mm256_zeroupper();
}

SC_AVX512_FN inline void waveNetConv512 (const float* rows, int rowLen, int d, int K, const float* W, const float* B, float* zp, int C, int N, int nT)
{
    if (K == 3)
    {
        // (kernel 3, the drive's and the compressor's: the same, three taps)
        for (int o = 0; o < 2 * C; o += 4)
            for (int t = 0; t < nT; t += 32)
            {
                __m512 a0 = _mm512_set1_ps (B[o]), b0 = a0, a1 = _mm512_set1_ps (B[o + 1]), b1 = a1;
                __m512 a2 = _mm512_set1_ps (B[o + 2]), b2 = a2, a3 = _mm512_set1_ps (B[o + 3]), b3 = a3;
                for (int c = 0; c < C; ++c)
                {
                    const float* p = rows + c * rowLen + t;
                    const __m512 x0 = _mm512_loadu_ps (p), y0 = _mm512_loadu_ps (p + 16), x1 = _mm512_loadu_ps (p + d), y1 = _mm512_loadu_ps (p + d + 16), x2 = _mm512_loadu_ps (p + 2 * d), y2 = _mm512_loadu_ps (p + 2 * d + 16);
                    const float* w0 = W + (o * C + c) * 3; const float* w1 = w0 + 3 * C; const float* w2 = w1 + 3 * C; const float* w3 = w2 + 3 * C;
                    { const __m512 w = _mm512_set1_ps (*(w0 + 0)); a0 = _mm512_fmadd_ps (w, x0, a0); b0 = _mm512_fmadd_ps (w, y0, b0); }
                    { const __m512 w = _mm512_set1_ps (*(w0 + 1)); a0 = _mm512_fmadd_ps (w, x1, a0); b0 = _mm512_fmadd_ps (w, y1, b0); }
                    { const __m512 w = _mm512_set1_ps (*(w0 + 2)); a0 = _mm512_fmadd_ps (w, x2, a0); b0 = _mm512_fmadd_ps (w, y2, b0); }
                    { const __m512 w = _mm512_set1_ps (*(w1 + 0)); a1 = _mm512_fmadd_ps (w, x0, a1); b1 = _mm512_fmadd_ps (w, y0, b1); }
                    { const __m512 w = _mm512_set1_ps (*(w1 + 1)); a1 = _mm512_fmadd_ps (w, x1, a1); b1 = _mm512_fmadd_ps (w, y1, b1); }
                    { const __m512 w = _mm512_set1_ps (*(w1 + 2)); a1 = _mm512_fmadd_ps (w, x2, a1); b1 = _mm512_fmadd_ps (w, y2, b1); }
                    { const __m512 w = _mm512_set1_ps (*(w2 + 0)); a2 = _mm512_fmadd_ps (w, x0, a2); b2 = _mm512_fmadd_ps (w, y0, b2); }
                    { const __m512 w = _mm512_set1_ps (*(w2 + 1)); a2 = _mm512_fmadd_ps (w, x1, a2); b2 = _mm512_fmadd_ps (w, y1, b2); }
                    { const __m512 w = _mm512_set1_ps (*(w2 + 2)); a2 = _mm512_fmadd_ps (w, x2, a2); b2 = _mm512_fmadd_ps (w, y2, b2); }
                    { const __m512 w = _mm512_set1_ps (*(w3 + 0)); a3 = _mm512_fmadd_ps (w, x0, a3); b3 = _mm512_fmadd_ps (w, y0, b3); }
                    { const __m512 w = _mm512_set1_ps (*(w3 + 1)); a3 = _mm512_fmadd_ps (w, x1, a3); b3 = _mm512_fmadd_ps (w, y1, b3); }
                    { const __m512 w = _mm512_set1_ps (*(w3 + 2)); a3 = _mm512_fmadd_ps (w, x2, a3); b3 = _mm512_fmadd_ps (w, y2, b3); }
                }
                float* z = zp + o * N + t;
                _mm512_storeu_ps (z, a0); _mm512_storeu_ps (z + 16, b0); z += N;
                _mm512_storeu_ps (z, a1); _mm512_storeu_ps (z + 16, b1); z += N;
                _mm512_storeu_ps (z, a2); _mm512_storeu_ps (z + 16, b2); z += N;
                _mm512_storeu_ps (z, a3); _mm512_storeu_ps (z + 16, b3);
            }
        return;
    }
    if (K == 2)
    {
        // (kernel 2, every model: the tile in named registers, which the compiler keeps there)
        for (int o = 0; o < 2 * C; o += 4)
            for (int t = 0; t < nT; t += 32)
            {
                __m512 a0 = _mm512_set1_ps (B[o]), b0 = a0, a1 = _mm512_set1_ps (B[o + 1]), b1 = a1;
                __m512 a2 = _mm512_set1_ps (B[o + 2]), b2 = a2, a3 = _mm512_set1_ps (B[o + 3]), b3 = a3;
                for (int c = 0; c < C; ++c)
                {
                    const float* p = rows + c * rowLen + t;
                    const __m512 x0 = _mm512_loadu_ps (p), y0 = _mm512_loadu_ps (p + 16), x1 = _mm512_loadu_ps (p + d), y1 = _mm512_loadu_ps (p + d + 16);
                    const float* w0 = W + (o * C + c) * 2; const float* w1 = w0 + 2 * C; const float* w2 = w1 + 2 * C; const float* w3 = w2 + 2 * C;
                    __m512 w = _mm512_set1_ps (w0[0]); a0 = _mm512_fmadd_ps (w, x0, a0); b0 = _mm512_fmadd_ps (w, y0, b0);
                    w = _mm512_set1_ps (w0[1]); a0 = _mm512_fmadd_ps (w, x1, a0); b0 = _mm512_fmadd_ps (w, y1, b0);
                    w = _mm512_set1_ps (w1[0]); a1 = _mm512_fmadd_ps (w, x0, a1); b1 = _mm512_fmadd_ps (w, y0, b1);
                    w = _mm512_set1_ps (w1[1]); a1 = _mm512_fmadd_ps (w, x1, a1); b1 = _mm512_fmadd_ps (w, y1, b1);
                    w = _mm512_set1_ps (w2[0]); a2 = _mm512_fmadd_ps (w, x0, a2); b2 = _mm512_fmadd_ps (w, y0, b2);
                    w = _mm512_set1_ps (w2[1]); a2 = _mm512_fmadd_ps (w, x1, a2); b2 = _mm512_fmadd_ps (w, y1, b2);
                    w = _mm512_set1_ps (w3[0]); a3 = _mm512_fmadd_ps (w, x0, a3); b3 = _mm512_fmadd_ps (w, y0, b3);
                    w = _mm512_set1_ps (w3[1]); a3 = _mm512_fmadd_ps (w, x1, a3); b3 = _mm512_fmadd_ps (w, y1, b3);
                }
                float* z = zp + o * N + t;
                _mm512_storeu_ps (z, a0); _mm512_storeu_ps (z + 16, b0); z += N;
                _mm512_storeu_ps (z, a1); _mm512_storeu_ps (z + 16, b1); z += N;
                _mm512_storeu_ps (z, a2); _mm512_storeu_ps (z + 16, b2); z += N;
                _mm512_storeu_ps (z, a3); _mm512_storeu_ps (z + 16, b3);
            }
        return;
    }
    for (int o = 0; o < 2 * C; o += 4)
        for (int t = 0; t < nT; t += 32)
        {
            __m512 acc[4][2];
            for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = _mm512_set1_ps (B[o + k]);
            for (int c = 0; c < C; ++c)
            {
                __m512 x[3][2];
                for (int j = 0; j < K; ++j)
                {
                    const float* p = rows + c * rowLen + t + j * d;
                    x[j][0] = _mm512_loadu_ps (p); x[j][1] = _mm512_loadu_ps (p + 16);
                }
                for (int k = 0; k < 4; ++k)
                {
                    const float* wk = W + ((o + k) * C + c) * K;
                    for (int j = 0; j < K; ++j)
                    {
                        const __m512 wj = _mm512_set1_ps (wk[j]);
                        acc[k][0] = _mm512_fmadd_ps (wj, x[j][0], acc[k][0]);
                        acc[k][1] = _mm512_fmadd_ps (wj, x[j][1], acc[k][1]);
                    }
                }
            }
            for (int k = 0; k < 4; ++k) { _mm512_storeu_ps (zp + (o + k) * N + t, acc[k][0]); _mm512_storeu_ps (zp + (o + k) * N + t + 16, acc[k][1]); }
        }
}

SC_AVX512_FN inline void waveNetRes512 (const float* gp, const float* RW, const float* RB, const float* SW, const float* SB, float* hp, float* sk, int C, int N, int nT)
{
    for (int o = 0; o < C; o += 2)
        for (int t = 0; t < nT; t += 32)
        {
            float* h0 = hp + o * N + t; float* h1 = h0 + N; float* s0 = sk + o * N + t; float* s1 = s0 + N;
            __m512 ha = _mm512_add_ps (_mm512_loadu_ps (h0), _mm512_set1_ps (RB[o])), hb = _mm512_add_ps (_mm512_loadu_ps (h0 + 16), _mm512_set1_ps (RB[o]));
            __m512 hc = _mm512_add_ps (_mm512_loadu_ps (h1), _mm512_set1_ps (RB[o + 1])), hd = _mm512_add_ps (_mm512_loadu_ps (h1 + 16), _mm512_set1_ps (RB[o + 1]));
            __m512 sa = _mm512_add_ps (_mm512_loadu_ps (s0), _mm512_set1_ps (SB[o])), sb = _mm512_add_ps (_mm512_loadu_ps (s0 + 16), _mm512_set1_ps (SB[o]));
            __m512 sc = _mm512_add_ps (_mm512_loadu_ps (s1), _mm512_set1_ps (SB[o + 1])), sd = _mm512_add_ps (_mm512_loadu_ps (s1 + 16), _mm512_set1_ps (SB[o + 1]));
            for (int c = 0; c < C; ++c)
            {
                const __m512 g0 = _mm512_loadu_ps (gp + c * N + t), g1 = _mm512_loadu_ps (gp + c * N + t + 16);
                const __m512 r0 = _mm512_set1_ps (RW[o * C + c]), r1 = _mm512_set1_ps (RW[(o + 1) * C + c]);
                const __m512 w0 = _mm512_set1_ps (SW[o * C + c]), w1 = _mm512_set1_ps (SW[(o + 1) * C + c]);
                ha = _mm512_fmadd_ps (r0, g0, ha); hb = _mm512_fmadd_ps (r0, g1, hb);
                sa = _mm512_fmadd_ps (w0, g0, sa); sb = _mm512_fmadd_ps (w0, g1, sb);
                hc = _mm512_fmadd_ps (r1, g0, hc); hd = _mm512_fmadd_ps (r1, g1, hd);
                sc = _mm512_fmadd_ps (w1, g0, sc); sd = _mm512_fmadd_ps (w1, g1, sd);
            }
            _mm512_storeu_ps (h0, ha); _mm512_storeu_ps (h0 + 16, hb); _mm512_storeu_ps (h1, hc); _mm512_storeu_ps (h1 + 16, hd);
            _mm512_storeu_ps (s0, sa); _mm512_storeu_ps (s0 + 16, sb); _mm512_storeu_ps (s1, sc); _mm512_storeu_ps (s1 + 16, sd);
        }
}

// the input mixed into a layer's 2C pre-activations: z[o] += m[o] u, 16 at a time
SC_AVX512_FN inline void waveNetMix512 (float* zp, const float* M, const float* up, int C2, int N, int nT)
{
    for (int o = 0; o < C2; ++o)
    {
        const __m512 m = _mm512_set1_ps (M[o]);
        float* zo = zp + o * N;
        for (int t = 0; t < nT; t += 16) _mm512_storeu_ps (zo + t, _mm512_fmadd_ps (m, _mm512_loadu_ps (up + t), _mm512_loadu_ps (zo + t)));
    }
}

SC_AVX512_FN inline __m512 tanh16 (__m512 x)
{
    x = _mm512_min_ps (_mm512_max_ps (x, _mm512_set1_ps (-4.97f)), _mm512_set1_ps (4.97f));
    const __m512 x2 = _mm512_mul_ps (x, x);
    const __m512 num = _mm512_mul_ps (x, _mm512_fmadd_ps (x2, _mm512_fmadd_ps (x2, _mm512_add_ps (_mm512_set1_ps (378.0f), x2), _mm512_set1_ps (17325.0f)), _mm512_set1_ps (135135.0f)));
    const __m512 den = _mm512_fmadd_ps (x2, _mm512_fmadd_ps (x2, _mm512_fmadd_ps (x2, _mm512_set1_ps (28.0f), _mm512_set1_ps (3150.0f)), _mm512_set1_ps (62370.0f)), _mm512_set1_ps (135135.0f));
    // (num / den as num times 1 / den: an estimate good to 14 bits, refined once, to within a few units in the last place)
    __m512 r = _mm512_rcp14_ps (den);
    r = _mm512_mul_ps (r, _mm512_fnmadd_ps (den, r, _mm512_set1_ps (2.0f)));
    return _mm512_mul_ps (num, r);
}

SC_AVX512_FN inline void waveNetGate512 (const float* zp, float* gp, int C, int N, int nT)
{
    const __m512 half = _mm512_set1_ps (0.5f);
    for (int c = 0; c < C; ++c)
        for (int t = 0; t < nT; t += 16)
        {
            const __m512 a = tanh16 (_mm512_loadu_ps (zp + c * N + t));
            const __m512 b = _mm512_fmadd_ps (half, tanh16 (_mm512_mul_ps (half, _mm512_loadu_ps (zp + (C + c) * N + t))), half);
            _mm512_storeu_ps (gp + c * N + t, _mm512_mul_ps (a, b));
        }
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
    // (convW: per layer (2C) x C x kernel)
    std::vector<std::vector<float>> convW, convB;     // per layer: (2C) x C x 2 (PyTorch layout), 2C
    std::vector<std::vector<float>> resW, resB, skipW, skipB;   // per layer: C x C, C
    std::vector<float> outW; float outB;             // 1 x C
    bool bypass = false;                             // y = Post(u) + WaveNet(u) (the network beside the long filter)
    int conds = 0;                                   // a control's value as an extra input (0 or 1; train_wn.py COND)
    int kernel = 2;                                  // the dilated convolutions' kernel size (2 or 3; train_wn.py KERNEL)
    std::vector<std::vector<float>> mixW;            // per layer 2C: the input mixed into it (train_wn.py MIXIN), or none
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
        preLine.assign (w.pre.size() - 1 + (size_t) maxBlock, 0.0f);
        hist.clear();
        jassert (w.kernel == 2 || w.kernel == 3);
        for (int d : w.dilations) hist.push_back (std::vector<float> ((size_t) C * (size_t) ((w.kernel - 1) * d + span), 0.0f));
        writeAt.assign (w.dilations.size(), 0);
        u.assign ((size_t) maxBlock, 0.0f); y = u; v = u;
        h.assign ((size_t) (C * maxBlock), 0.0f); skip = h; g = h;
        z.assign ((size_t) (2 * C * maxBlock), 0.0f);
        // the weights repeated four times (a register's worth), so a tile loads each instead of broadcasting it
        auto splat = [] (const std::vector<float>& v) { std::vector<float> s (v.size() * 4); for (size_t i = 0; i < s.size(); ++i) s[i] = v[i / 4]; return s; };
        convS.clear(); resS.clear(); skipS.clear();
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            convS.push_back (splat (w.convW[l])); resS.push_back (splat (w.resW[l])); skipS.push_back (splat (w.skipW[l]));
        }
        // the state after silence, which reset() returns to: a network that has heard nothing holds its biases'
        // response in every layer, not zeros, and the reference's models restart as if from silence
        // (MEASUREMENTS.md 12; zeros are a state the network never saw in training)
        std::fill (preLine.begin(), preLine.end(), 0.0f);
        for (auto& r : hist) std::fill (r.begin(), r.end(), 0.0f);
        for (size_t l = 0; l < w.dilations.size(); ++l) writeAt[l] = (w.kernel - 1) * w.dilations[l];
        post.reset();
        std::vector<double> quiet ((size_t) maxBlock, 0.0), heard ((size_t) maxBlock);
        for (int done = 0; done < memory(); done += maxBlock) block (quiet.data(), heard.data(), maxBlock, w.conds > 0 ? quiet.data() : nullptr);
        savedPre = preLine; savedHist = hist; savedWriteAt = writeAt;
        post.save();
    }
    // how many past input samples the output depends on (the network has no other state)
    int memory() const
    {
        int m = (int) weights->pre.size() + (int) weights->post.size();
        for (int d : weights->dilations) m += (weights->kernel - 1) * d;
        return m;
    }
    // back to the state after silence (prepare)
    void reset()
    {
        std::copy (savedPre.begin(), savedPre.end(), preLine.begin());
        for (size_t l = 0; l < hist.size(); ++l) std::copy (savedHist[l].begin(), savedHist[l].end(), hist[l].begin());
        std::copy (savedWriteAt.begin(), savedWriteAt.end(), writeAt.begin());
        post.restore();
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
        const int C = w.channels, PL = (int) w.pre.size(), N = maxBlock;
#if SC_AVX
        // (0: SSE / NEON / plain, 1: AVX, 2: AVX2 and FMA, 3: AVX-512)
        static const int wide = juce::SystemStats::hasAVX512F() ? 3
                              : juce::SystemStats::hasAVX2() && juce::SystemStats::hasFMA3() ? 2
                              : juce::SystemStats::hasAVX() ? 1 : 0;
        const bool avx = wide == 1;
#else
        constexpr int wide = 0;
        constexpr bool avx = false;
#endif
        // the tiles run past n to a multiple of their width (on stale, finite values; unused)
        const int n8 = wide == 3 ? (n + 31) & ~31 : wide > 0 ? (n + 15) & ~15 : (n + 7) & ~7;
        float* up = u.data(); float* hp = h.data(); float* sk = skip.data(); float* zp = z.data(); float* gp = g.data();
        // the pre-filter across the block (the taps summed in the same order as one sample at a time)
        {
            float* line = preLine.data();
            float* cur = line + (PL - 1);
            for (int t = 0; t < n; ++t) cur[t] = (float) in[t];
            std::fill (up, up + n, 0.0f);
            for (int k = 0; k < PL; ++k)
            {
                const float c = w.pre[(size_t) k];
                const float* x = cur - k;
                for (int t = 0; t < n; ++t) up[t] += c * x[t];
            }
            std::memmove (line, line + n, sizeof (float) * (size_t) (PL - 1));
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
            // (each row: its last (K - 1) d inputs, then this block, at writeAt; the rows hold several blocks, so the
            // history is moved back to their start only once every few blocks, not after each)
            const int d = w.dilations[l], K = w.kernel, back = (K - 1) * d, rowLen = back + span, at = writeAt[l];
            float* rows = hist[l].data() + (at - back);
            // the layer's input after its last (K - 1) d inputs: row c = [x(t - (K - 1) d) ... x(t - 1) | this block]
            for (int c = 0; c < C; ++c) std::memcpy (rows + c * rowLen + back, hp + c * N, sizeof (float) * (size_t) n);
            const float* W = convS[l].data(); const float* B = w.convB[l].data();
#if SC_AVX
            if (wide == 3) waveNetConv512 (rows, rowLen, d, K, w.convW[l].data(), B, zp, C, N, n8);
            else if (wide == 2) waveNetConvFma (rows, rowLen, d, K, w.convW[l].data(), B, zp, C, N, n8);
            else if (avx) waveNetConvAvx (rows, rowLen, d, K, w.convW[l].data(), B, zp, C, N, n8);
            else
#endif
            for (int o = 0; o < 2 * C; o += 4)
                for (int t = 0; t < n8; t += 8)
                {
                    V4 acc[4][2];
                    for (int k = 0; k < 4; ++k) acc[k][0] = acc[k][1] = V4::all (B[o + k]);
                    for (int c = 0; c < C; ++c)
                    {
                        V4 x[3][2];                  // the taps, oldest first: x(t - (K - 1) d) ... x(t)
                        for (int j = 0; j < K; ++j) { const float* p = rows + c * rowLen + t + j * d; x[j][0] = V4::load (p); x[j][1] = V4::load (p + 4); }
                        for (int k = 0; k < 4; ++k)
                        {
                            const float* wk = W + ((o + k) * C + c) * K * 4;
                            const V4 w0 = V4::load (wk), w1 = V4::load (wk + 4);
                            for (int q = 0; q < 2; ++q)
                            {
                                V4 s = w0 * x[0][q] + w1 * x[1][q];
                                if (K > 2) s = s + V4::load (wk + 8) * x[2][q];
                                acc[k][q] = acc[k][q] + s;
                            }
                        }
                    }
                    for (int k = 0; k < 4; ++k) { acc[k][0].store (zp + (o + k) * N + t); acc[k][1].store (zp + (o + k) * N + t + 4); }
                }
#if SC_AVX
            if (! w.mixW.empty() && wide == 3) waveNetMix512 (zp, w.mixW[l].data(), up, 2 * C, N, n8);
            else
#endif
            if (! w.mixW.empty())
            {
                const float* M = w.mixW[l].data();
                for (int o = 0; o < 2 * C; ++o) { float* zo = zp + o * N; const float m = M[o]; for (int t = 0; t < n8; ++t) zo[t] += m * up[t]; }
            }
#if SC_AVX
            if (wide == 3) waveNetGate512 (zp, gp, C, N, n8);
            else if (wide == 2) waveNetGateFma (zp, gp, C, N, n8);
            else
#endif
            for (int c = 0; c < C; ++c)
            {
                const float* za = zp + c * N; const float* zb = zp + (C + c) * N; float* gc = gp + c * N;
                for (int t = 0; t < n8; ++t) gc[t] = fastTanh (za[t]) * fastSigmoid (zb[t]);
            }
            const float* RW = resS[l].data(); const float* RB = w.resB[l].data();
            const float* SW = skipS[l].data(); const float* SB = w.skipB[l].data();
#if SC_AVX
            if (wide == 3) waveNetRes512 (gp, w.resW[l].data(), RB, w.skipW[l].data(), SB, hp, sk, C, N, n8);
            else if (wide == 2) waveNetResFma (gp, w.resW[l].data(), RB, w.skipW[l].data(), SB, hp, sk, C, N, n8);
            else if (avx) waveNetResAvx (gp, w.resW[l].data(), RB, w.skipW[l].data(), SB, hp, sk, C, N, n8);
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
            writeAt[l] = at + n;
            if (writeAt[l] + maxBlock > rowLen)
            {
                float* start = hist[l].data();
                for (int c = 0; c < C; ++c) std::memmove (start + c * rowLen, start + c * rowLen + writeAt[l] - back, sizeof (float) * (size_t) back);
                writeAt[l] = back;
            }
        }
        float* yp = y.data();
        for (int t = 0; t < n; ++t) yp[t] = w.outB;
        for (int c = 0; c < C; ++c)
        {
            const float a = w.outW[(size_t) c]; const float* sc = sk + c * N;
            for (int t = 0; t < n; ++t) yp[t] += a * std::max (0.0f, sc[t]);
        }
        float* vp = v.data();
        if (w.bypass) std::copy (up, up + n, vp);
        else for (int t = 0; t < n; ++t) vp[t] = up[t] + yp[t];
        post.processBlock (vp, vp, n);
        if (w.bypass) for (int t = 0; t < n; ++t) out[t] = vp[t] + yp[t];
        else for (int t = 0; t < n; ++t) out[t] = vp[t];
    }

    const WaveNetWeights* weights = nullptr;
    Convolver post;
    std::vector<float> preLine, u, y, v, h, skip, z, g;
    std::vector<std::vector<float>> hist, convS, resS, skipS;
    std::vector<float> savedPre;
    std::vector<std::vector<float>> savedHist;
    std::vector<int> writeAt, savedWriteAt;                 // where each layer's rows take this block
    static constexpr int span = 8 * maxBlock;             // (the room for blocks after a layer's history)
};
}
