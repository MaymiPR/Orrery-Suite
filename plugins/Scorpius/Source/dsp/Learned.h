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
        // the recurrent weights by column (input unit), so each update runs over contiguous outputs (no reduction)
        const int H = w.hidden;
        whhT.assign ((size_t) (3 * H * H), 0.0f);
        for (int r = 0; r < 3 * H; ++r)
            for (int c = 0; c < H; ++c) whhT[(size_t) (c * 3 * H + r)] = w.whh[(size_t) (r * H + c)];
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
        float* gip = gi.data(); float* ghp = gh.data(); float* hp = h.data();
        const float* wih = w.wih.data(); const float* bih = w.bih.data(); const float* bhh = w.bhh.data();
        for (int r = 0; r < 3 * H; ++r) { gip[r] = wih[r] * u + bih[r]; ghp[r] = bhh[r]; }
        for (int c = 0; c < H; ++c)
        {
            const float hc = hp[c];
            const float* col = whhT.data() + (size_t) c * 3 * (size_t) H;
            for (int r = 0; r < 3 * H; ++r) ghp[r] += col[r] * hc;
        }
        for (int c = 0; c < H; ++c)
        {
            const float rg = fastSigmoid (gip[c] + ghp[c]);
            const float zg = fastSigmoid (gip[H + c] + ghp[H + c]);
            const float ng = fastTanh (gip[2 * H + c] + rg * ghp[2 * H + c]);
            hp[c] = (1 - zg) * ng + zg * hp[c];
        }
        float out = w.bout;
        for (int c = 0; c < H; ++c) out += w.wout[(size_t) c] * hp[c];
        return post.process (u + out);
    }

private:
    const LearnedWeights* weights = nullptr;
    Convolver post;
    std::vector<float> hist, h, gi, gh, whhT;
    int hpos = 0;
};
}
