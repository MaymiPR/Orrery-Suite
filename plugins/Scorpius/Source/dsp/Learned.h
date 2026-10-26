#pragma once
#include <cmath>
#include <vector>
#include "Convolver.h"
#include "FastMath.h"

// A learned module (nulltest/train_gru.py): y = Post(u + W GRU(u)), u = Pre(x). Pre a short FIR, the GRU a small
// recurrent network (PyTorch's equations), Post a long FIR (Convolver.h). The weights are ours, trained on the
// reference's input and output (LearnedModels.h, written by nulltest/export_models.py).
namespace sc::dsp
{
struct LearnedWeights
{
    int hidden;
    std::vector<float> pre, post;
    std::vector<float> wih, whh, bih, bhh;      // (3 hidden) x 1, (3 hidden) x hidden, 3 hidden, 3 hidden: r, z, n
    std::vector<float> wout; float bout;
};

class Learned
{
public:
    void prepare (const LearnedWeights& w)
    {
        weights = &w;
        post.prepare (w.post);
        hist.assign (w.pre.size(), 0.0f);
        h.assign ((size_t) w.hidden, 0.0f);
        gi.assign ((size_t) (3 * w.hidden), 0.0f); gh = gi;
        reset();
    }
    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        std::fill (h.begin(), h.end(), 0.0f);
        post.reset();
        hpos = 0;
    }
    float process (float x)
    {
        const auto& w = *weights;
        const int H = w.hidden, K = (int) w.pre.size();
        hist[(size_t) hpos] = x;
        float u = 0;
        int j = hpos;
        for (int k = 0; k < K; ++k) { u += w.pre[(size_t) k] * hist[(size_t) j]; j = j == 0 ? K - 1 : j - 1; }
        hpos = (hpos + 1) % K;
        // the GRU step
        for (int r = 0; r < 3 * H; ++r)
        {
            gi[(size_t) r] = w.wih[(size_t) r] * u + w.bih[(size_t) r];
            float s = w.bhh[(size_t) r];
            const float* row = w.whh.data() + (size_t) r * (size_t) H;
            for (int c = 0; c < H; ++c) s += row[c] * h[(size_t) c];
            gh[(size_t) r] = s;
        }
        float out = w.bout;
        for (int c = 0; c < H; ++c)
        {
            const float rg = fastSigmoid (gi[(size_t) c] + gh[(size_t) c]);
            const float zg = fastSigmoid (gi[(size_t) (H + c)] + gh[(size_t) (H + c)]);
            const float ng = fastTanh (gi[(size_t) (2 * H + c)] + rg * gh[(size_t) (2 * H + c)]);
            h[(size_t) c] = (1 - zg) * ng + zg * h[(size_t) c];
            out += w.wout[(size_t) c] * h[(size_t) c];
        }
        return post.process (u + out);
    }

private:
    const LearnedWeights* weights = nullptr;
    Convolver post;
    std::vector<float> hist, h, gi, gh;
    int hpos = 0;
};
}
