#pragma once
#include "Compressor.h"
#include "Learned.h"
#include "LearnedWaveNet.h"
#include "LearnedModels.h"

// The modules the reference runs as trained models of its own, ours learned from its input and output
// (MEASUREMENTS.md 6, 7, 9, 14), each a WaveNet (nulltest/train_wn.py): the drive (one per colour and hi-pass
// setting), harmonics' wet path (one per frequency), lift's wet path (lift at amount 0 is a fixed filter, fitted; the
// amount mixes the two exactly) and the grit parallel.
namespace sc::dsp
{
class Drive
{
public:
    void prepare()
    {
        const WaveNetWeights* w[6] { &models::driveA, &models::driveB, &models::driveC, &models::driveAhp, &models::driveBhp, &models::driveChp };
        for (int k = 0; k < 6; ++k) for (auto& m : model[k]) m.prepare (*w[k]);
    }
    void reset() { for (auto& k : model) for (auto& m : k) m.reset(); }
    void process (double* l, double* r, int n, int colour, bool hiPass)
    {
        const int k = (colour < 0 ? 0 : colour > 2 ? 2 : colour) + (hiPass ? 3 : 0);
        if (k != current) { model[k][0].reset(); model[k][1].reset(); current = k; }
        model[k][0].process (l, l, n); model[k][1].process (r, r, n);
    }

private:
    LearnedWaveNet model[6][2];
    int current = -1;
};

// Harmonics: the reference runs a neural model of a fully wet process here and mixes it with the dry signal by the
// amount, a little under linearly (the measured weights), its processing changing a little with the amount too; ours
// is learned at amounts 0.25, 0.5, 0.75 and 1 (capture_nn.py harm80 / harm50 ..., train_wn.py EQUIV: the fully wet
// equivalent of each), the two models either side of the amount interpolated and mixed in by the same weights.
class Harmonics
{
public:
    static constexpr double nodes[4] { 0.25, 0.5, 0.75, 1.0 };

    void prepare (int maxBlock)
    {
        const WaveNetWeights* w[2][4] { { &models::harm80a25, &models::harm80a50, &models::harm80a75, &models::harm80 },
                                        { &models::harm50a25, &models::harm50a50, &models::harm50a75, &models::harm50 } };
        for (int f = 0; f < 2; ++f)
            for (int k = 0; k < 4; ++k)
                for (auto& m : model[f][k]) m.prepare (*w[f][k]);
        for (auto& v : tmp) for (auto& b : v) b.assign ((size_t) std::max (1, maxBlock), 0.0);
        amounts.assign ((size_t) std::max (1, maxBlock), 0.0);
    }
    void reset() { for (auto& f : model) for (auto& k : f) for (auto& m : k) m.reset(); pair = -1; }
    void setMode (int freqHz)
    {
        const int f = freqHz == 50 ? 1 : 0;
        if (f != current) { current = f; pair = -1; }
    }
    // in place: the wet signal mixed with the dry one by the amount (0 ... 1)
    void process (double* l, double* r, int n, Ramp& amount)
    {
        const int f = current < 0 ? 0 : current;
        double* ch[2] { l, r };
        for (int i = 0; i < n; ++i) amounts[(size_t) i] = amount.next();
        // the two models either side of the amount (at the block's middle); one coming in starts from silence
        const double mid = amounts[(size_t) (n / 2)];
        int lo = 0;
        while (lo < 2 && mid > nodes[lo + 1]) ++lo;
        if (lo != pair)
            for (int k = lo; k <= lo + 1; ++k)
                if (pair < 0 || k < pair || k > pair + 1) for (auto& m : model[f][k]) m.reset();
        pair = lo;
        for (int s = 0; s < 2; ++s)
            for (int c = 0; c < 2; ++c) model[f][lo + s][c].process (ch[c], tmp[s][c].data(), n);
        for (int i = 0; i < n; ++i)
        {
            const double a = amounts[(size_t) i], w = weight (a);
            const double t = std::clamp ((a - nodes[lo]) / (nodes[lo + 1] - nodes[lo]), 0.0, 1.0);
            for (int c = 0; c < 2; ++c)
            {
                const double wet = tmp[0][c][(size_t) i] + t * (tmp[1][c][(size_t) i] - tmp[0][c][(size_t) i]);
                ch[c][i] += w * (wet - ch[c][i]);
            }
        }
    }
    // the amount's weight of the wet signal (measured: 0.237, 0.483, 0.738 at 0.25, 0.5, 0.75)
    static double weight (double a)
    {
        static constexpr double at[] { 0, 0.25, 0.5, 0.75, 1 }, w[] { 0, 0.237, 0.483, 0.738, 1 };
        a = a < 0 ? 0 : a > 1 ? 1 : a;
        int i = 1;
        while (i < 4 && a > at[i]) ++i;
        return w[i - 1] + (a - at[i - 1]) / (at[i] - at[i - 1]) * (w[i] - w[i - 1]);
    }

private:
    LearnedWaveNet model[2][4][2];        // [frequency][amount node][channel]
    std::vector<double> tmp[2][2], amounts;
    int current = -1, pair = -1;
};

class Lift
{
public:
    void prepare (int maxBlock)
    {
        for (auto& m : wet) m.prepare (models::lift);
        for (auto& d : dry) d.set (fits::liftDry);
        for (auto& b : tmp) b.assign ((size_t) std::max (1, maxBlock), 0.0);
    }
    void reset() { for (auto& m : wet) m.reset(); for (auto& d : dry) d.reset(); }
    void process (double* l, double* r, int n, Ramp& amount)
    {
        double* ch[2] { l, r };
        for (int c = 0; c < 2; ++c) wet[c].process (ch[c], tmp[c].data(), n);
        for (int i = 0; i < n; ++i)
        {
            const double a = amount.next();
            for (int c = 0; c < 2; ++c)
            {
                const double d = dry[c].process (ch[c][i]);
                ch[c][i] = d + a * (tmp[c][(size_t) i] - d);
            }
        }
    }

private:
    LearnedWaveNet wet[2];
    ParallelFilter dry[2];
    std::vector<double> tmp[2];
};

// The grit parallel: a transient shaper of the mids (MEASUREMENTS.md 9), learned at level 1 on each channel by
// itself (the reference's grit works per channel); its contribution, the model's output less its input, scales
// exactly with the level's fader taper (w).
class Grit
{
public:
    void prepare (int maxBlock)
    {
        for (auto& m : model) m.prepare (models::grit);
        for (auto& b : tmp) b.assign ((size_t) std::max (1, maxBlock), 0.0);
        dirty = true; reset();
    }
    void reset()
    {
        if (! dirty) return;
        for (auto& m : model) m.reset();
        dirty = false;
    }
    void process (const double* inL, const double* inR, double* outL, double* outR, int n, Ramp& w)
    {
        dirty = true;
        model[0].process (inL, tmp[0].data(), n);
        model[1].process (inR, tmp[1].data(), n);
        for (int i = 0; i < n; ++i)
        {
            const double m = w.next();
            outL[i] += m * (tmp[0][(size_t) i] - inL[i]);
            outR[i] += m * (tmp[1][(size_t) i] - inR[i]);
        }
    }

private:
    LearnedWaveNet model[2];
    std::vector<double> tmp[2];
    bool dirty = true;
};
}
