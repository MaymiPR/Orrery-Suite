#pragma once
#include "Compressor.h"
#include "Learned.h"
#include "LearnedWaveNet.h"
#include "LearnedModels.h"

// The modules the reference runs as trained models of its own, ours learned from its input and output
// (MEASUREMENTS.md 6, 9, 14): the drive (a WaveNet per colour and hi-pass setting, nulltest/train_wn.py) and lift's wet
// path (a GRU, nulltest/train_gru.py;
// lift at amount 0 is a fixed filter, fitted; the amount mixes the two exactly) and the grit parallel (a WaveNet).
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

class Lift
{
public:
    void prepare()
    {
        for (auto& m : wet) m.prepare (models::lift);
        for (auto& d : dry) d.set (fits::liftDry);
    }
    void reset() { for (auto& m : wet) m.reset(); for (auto& d : dry) d.reset(); }
    void process (double* l, double* r, int n, Ramp& amount)
    {
        double* ch[2] { l, r };
        for (int i = 0; i < n; ++i)
        {
            const double a = amount.next();
            for (int c = 0; c < 2; ++c)
            {
                const double x = ch[c][i];
                const double d = dry[c].process (x), w = wet[c].process ((float) x);
                ch[c][i] = d + a * (w - d);
            }
        }
    }

private:
    Learned wet[2];
    ParallelFilter dry[2];
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
