#pragma once
#include <cmath>
#include <vector>
#include "Convolver.h"

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
        const int C = w.channels, K = (int) w.pre.size();
        pre[(size_t) prePos] = x;
        float u = 0;
        int j = prePos;
        for (int k = 0; k < K; ++k) { u += w.pre[(size_t) k] * pre[(size_t) j]; j = j == 0 ? K - 1 : j - 1; }
        prePos = (prePos + 1) % K;

        for (int c = 0; c < C; ++c) { h[(size_t) c] = w.inW[(size_t) c] * u + w.inB[(size_t) c]; skip[(size_t) c] = 0; }
        for (size_t l = 0; l < w.dilations.size(); ++l)
        {
            const int d = w.dilations[l];
            auto& ring = rings[l];
            int& rp = ringPos[l];
            // store this layer's input, read the one d samples ago
            float* now = ring.data() + (size_t) rp * (size_t) C;
            for (int c = 0; c < C; ++c) now[c] = h[(size_t) c];
            const int past = (rp - d + d + 1) % (d + 1);
            const float* old = ring.data() + (size_t) past * (size_t) C;
            rp = (rp + 1) % (d + 1);
            const auto& W = w.convW[l]; const auto& B = w.convB[l];
            for (int o = 0; o < 2 * C; ++o)
            {
                float s = B[(size_t) o];
                const float* row = W.data() + (size_t) o * (size_t) C * 2;
                for (int c = 0; c < C; ++c) s += row[2 * c] * old[c] + row[2 * c + 1] * now[c];
                z[(size_t) o] = s;
            }
            for (int c = 0; c < C; ++c) g[(size_t) c] = std::tanh (z[(size_t) c]) / (1.0f + std::exp (-z[(size_t) (C + c)]));
            const auto& RW = w.resW[l]; const auto& RB = w.resB[l]; const auto& SW = w.skipW[l]; const auto& SB = w.skipB[l];
            for (int o = 0; o < C; ++o)
            {
                float r = RB[(size_t) o], s = SB[(size_t) o];
                for (int c = 0; c < C; ++c) { r += RW[(size_t) (o * C + c)] * g[(size_t) c]; s += SW[(size_t) (o * C + c)] * g[(size_t) c]; }
                h[(size_t) o] += r; skip[(size_t) o] += s;
            }
        }
        float out = w.outB;
        for (int c = 0; c < C; ++c) out += w.outW[(size_t) c] * std::max (0.0f, skip[(size_t) c]);
        return post.process (u + out);
    }

private:
    const WaveNetWeights* weights = nullptr;
    Convolver post;
    std::vector<float> pre, h, skip, z, g;
    std::vector<std::vector<float>> rings;
    std::vector<int> ringPos;
    int prePos = 0;
};
}
