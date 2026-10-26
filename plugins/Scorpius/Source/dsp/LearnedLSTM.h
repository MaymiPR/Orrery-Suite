#pragma once
#include <algorithm>
#include <vector>
#include "Convolver.h"
#include "FastMath.h"

// A learned module as a stacked LSTM (nulltest/train_gru.py, CELL=lstm): y = Post(u + W LSTM(u)), u = Pre(x). Pre a
// short FIR, the LSTM PyTorch's equations (gates i, f, g, o), Post a long FIR (Convolver.h). The reference's own
// models are LSTMs of this kind (RESEARCH.md: its drive "LSTM_3_LAYER", its harmonics one layer of 24); the weights
// here are ours, trained on its input and output (written into LearnedModels.h by nulltest/export_models.py).
namespace sc::dsp
{
struct LSTMWeights
{
    int hidden, layers;
    std::vector<float> pre, post;
    std::vector<std::vector<float>> wih, whh, bih, bhh;   // per layer: (4H) x in, (4H) x H, 4H, 4H (PyTorch layout)
    std::vector<float> wout; float bout;                 // 1 x H
};

class LearnedLSTM
{
public:
    void prepare (const LSTMWeights& w)
    {
        weights = &w;
        const int H = w.hidden;
        post.prepare (w.post);
        pre.assign (w.pre.size(), 0.0f);
        // the weights by input (columns), so each update runs over contiguous gate rows (no reduction)
        colIh.clear(); colHh.clear(); bias.clear();
        for (int l = 0; l < w.layers; ++l)
        {
            const int in = l == 0 ? 1 : H;
            std::vector<float> ih ((size_t) (4 * H * in)), hh ((size_t) (4 * H * H)), b ((size_t) (4 * H));
            for (int r = 0; r < 4 * H; ++r)
            {
                for (int c = 0; c < in; ++c) ih[(size_t) (c * 4 * H + r)] = w.wih[(size_t) l][(size_t) (r * in + c)];
                for (int c = 0; c < H; ++c) hh[(size_t) (c * 4 * H + r)] = w.whh[(size_t) l][(size_t) (r * H + c)];
                b[(size_t) r] = w.bih[(size_t) l][(size_t) r] + w.bhh[(size_t) l][(size_t) r];
            }
            colIh.push_back (ih); colHh.push_back (hh); bias.push_back (b);
        }
        h.assign ((size_t) w.layers, std::vector<float> ((size_t) H, 0.0f)); c = h;
        gates.assign ((size_t) (4 * H), 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& v : h) std::fill (v.begin(), v.end(), 0.0f);
        for (auto& v : c) std::fill (v.begin(), v.end(), 0.0f);
        post.reset();
        prePos = 0;
    }
    // in place allowed
    void process (const double* in, double* out, int n)
    {
        const auto& w = *weights;
        const int H = w.hidden, K = (int) w.pre.size(), G = 4 * H;
        float* g = gates.data();
        for (int t = 0; t < n; ++t)
        {
            pre[(size_t) prePos] = (float) in[t];
            float u = 0;
            int j = prePos;
            for (int k = 0; k < K; ++k) { u += w.pre[(size_t) k] * pre[(size_t) j]; j = j == 0 ? K - 1 : j - 1; }
            prePos = (prePos + 1) % K;
            const float* x = &u;
            for (int l = 0; l < w.layers; ++l)
            {
                const int inN = l == 0 ? 1 : H;
                const float* b = bias[(size_t) l].data();
                for (int r = 0; r < G; ++r) g[r] = b[r];
                const float* wi = colIh[(size_t) l].data();
                for (int q = 0; q < inN; ++q)
                {
                    const float xq = x[q]; const float* col = wi + (size_t) q * (size_t) G;
                    for (int r = 0; r < G; ++r) g[r] += col[r] * xq;
                }
                float* hl = h[(size_t) l].data(); float* cl = c[(size_t) l].data();
                const float* wh = colHh[(size_t) l].data();
                for (int q = 0; q < H; ++q)
                {
                    const float hq = hl[q]; const float* col = wh + (size_t) q * (size_t) G;
                    for (int r = 0; r < G; ++r) g[r] += col[r] * hq;
                }
                for (int k = 0; k < H; ++k)
                {
                    const float ig = fastSigmoid (g[k]), fg = fastSigmoid (g[H + k]), gg = fastTanh (g[2 * H + k]), og = fastSigmoid (g[3 * H + k]);
                    cl[k] = fg * cl[k] + ig * gg;
                    hl[k] = og * fastTanh (cl[k]);
                }
                x = hl;
            }
            float o = w.bout;
            for (int k = 0; k < H; ++k) o += w.wout[(size_t) k] * x[k];
            out[t] = post.process (u + o);
        }
    }

private:
    const LSTMWeights* weights = nullptr;
    Convolver post;
    std::vector<float> pre, gates;
    std::vector<std::vector<float>> colIh, colHh, bias, h, c;
    int prePos = 0;
};
}
