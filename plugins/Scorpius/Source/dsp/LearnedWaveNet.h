#pragma once
#include <cmath>
#include <vector>
#include "Convolver.h"
#include "FastMath.h"

// A learned module as a small WaveNet (nulltest/train_wn.py): y = Post(u + WaveNet(u)), u = Pre(x). Pre a short FIR,
// the WaveNet dilated causal convolutions (kernel 2) with gated activations, residual and skip connections, Post a
// long FIR (Convolver.h). The weights are ours, trained on the reference's input and output (written into
// LearnedModels.h by nulltest/export_models.py).
namespace sc::dsp
{
struct WaveNetWeights
{
    int channels;
    std::vector<int> dilations;
    std::vector<float> pre, post;
    std::vector<float> inW, inB;                     // C x 1, C
    std::vector<std::vector<float>> convW, convB;     // per layer: (2C) x C x 2 (PyTorch layout), 2C
    std::vector<std::vector<float>> resW, resB, skipW, skipB;   // per layer: C x C, C
    std::vector<float> outW; float outB;             // 1 x C
};

class LearnedWaveNet
{
public:
    void prepare (const WaveNetWeights& w)
    {
        weights = &w;
        const int C = w.channels;
        post.prepare (w.post);
        pre.assign (w.pre.size(), 0.0f);
        rings.clear();
        for (int d : w.dilations) rings.push_back (std::vector<float> ((size_t) (d + 1) * (size_t) C, 0.0f));
        ringPos.assign (w.dilations.size(), 0);
        h.assign ((size_t) C, 0.0f); skip = h; z.assign ((size_t) (2 * C), 0.0f); g = h;
        // the weights by input channel (columns), so each update runs over contiguous outputs
        colOld.clear(); colNow.clear(); colRes.clear(); colSkip.clear();
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            std::vector<float> o ((size_t) (2 * C * C)), n ((size_t) (2 * C * C)), r ((size_t) (C * C)), k ((size_t) (C * C));
            for (int out = 0; out < 2 * C; ++out)
                for (int c = 0; c < C; ++c)
                {
                    o[(size_t) (c * 2 * C + out)] = w.convW[l][(size_t) (out * C * 2 + 2 * c)];
                    n[(size_t) (c * 2 * C + out)] = w.convW[l][(size_t) (out * C * 2 + 2 * c + 1)];
                }
            for (int out = 0; out < C; ++out)
                for (int c = 0; c < C; ++c)
                {
                    r[(size_t) (c * C + out)] = w.resW[l][(size_t) (out * C + c)];
                    k[(size_t) (c * C + out)] = w.skipW[l][(size_t) (out * C + c)];
                }
            colOld.push_back (o); colNow.push_back (n); colRes.push_back (r); colSkip.push_back (k);
        }
        reset();
    }
    void reset()
    {
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& r : rings) std::fill (r.begin(), r.end(), 0.0f);
        std::fill (ringPos.begin(), ringPos.end(), 0);
        post.reset();
        prePos = 0;
    }
    float process (float x)
    {
        const auto& w = *weights;
        const int K = (int) w.pre.size();
        pre[(size_t) prePos] = x;
        float u = 0;
        int j = prePos;
        for (int k = 0; k < K; ++k) { u += w.pre[(size_t) k] * pre[(size_t) j]; j = j == 0 ? K - 1 : j - 1; }
        prePos = (prePos + 1) % K;
        const float out = w.channels == 8 ? network<8> (u) : network<0> (u);
        return post.process (u + out);
    }

private:
    // the network for a sample; N > 0: the channel count known at compile time (the loops vectorise)
    template <int N>
    float network (float u)
    {
        const auto& w = *weights;
        const int C = N > 0 ? N : w.channels;
        float* hp = h.data(); float* sk = skip.data(); float* zp = z.data(); float* gp = g.data();
        for (int c = 0; c < C; ++c) { hp[c] = w.inW[(size_t) c] * u + w.inB[(size_t) c]; sk[c] = 0; }
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            const int d = w.dilations[l];
            auto& ring = rings[l];
            int& rp = ringPos[l];
            float* now = ring.data() + (size_t) rp * (size_t) C;
            for (int c = 0; c < C; ++c) now[c] = hp[c];
            const float* old = ring.data() + (size_t) ((rp + 1) % (d + 1)) * (size_t) C;
            rp = (rp + 1) % (d + 1);
            const float* B = w.convB[l].data();
            const float* WO = colOld[l].data(); const float* WN = colNow[l].data();
            for (int o = 0; o < 2 * C; ++o) zp[o] = B[o];
            for (int c = 0; c < C; ++c)
            {
                const float xo = old[c], xn = now[c];
                const float* co = WO + (size_t) c * 2 * (size_t) C; const float* cn = WN + (size_t) c * 2 * (size_t) C;
                for (int o = 0; o < 2 * C; ++o) zp[o] += co[o] * xo + cn[o] * xn;
            }
            for (int c = 0; c < C; ++c) gp[c] = fastTanh (zp[c]) * fastSigmoid (zp[C + c]);
            const float* RB = w.resB[l].data(); const float* SB = w.skipB[l].data();
            const float* RW = colRes[l].data(); const float* SW = colSkip[l].data();
            for (int o = 0; o < C; ++o) { hp[o] += RB[o]; sk[o] += SB[o]; }
            for (int c = 0; c < C; ++c)
            {
                const float gc = gp[c];
                const float* cr = RW + (size_t) c * (size_t) C; const float* cs = SW + (size_t) c * (size_t) C;
                for (int o = 0; o < C; ++o) { hp[o] += cr[o] * gc; sk[o] += cs[o] * gc; }
            }
        }
        float out = w.outB;
        for (int c = 0; c < C; ++c) out += w.outW[(size_t) c] * std::max (0.0f, sk[c]);
        return out;
    }

    const WaveNetWeights* weights = nullptr;
    Convolver post;
    std::vector<float> pre, h, skip, z, g;
    std::vector<std::vector<float>> rings, colOld, colNow, colRes, colSkip;
    std::vector<int> ringPos;
    int prePos = 0;
};
}
