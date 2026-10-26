#pragma once
#include "Compressor.h"
#include "Learned.h"
#include "LearnedWaveNet.h"
#include "LearnedModels.h"

// The modules the reference runs as trained models of its own, ours learned from its input and output
// (MEASUREMENTS.md 6, 9, 14): the drive (a WaveNet per colour and hi-pass setting, nulltest/train_wn.py) and lift's wet
// path (a GRU, nulltest/train_gru.py;
// lift at amount 0 is a fixed filter, fitted; the amount mixes the two exactly).
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
        for (int i = 0; i < n; ++i) { l[i] = model[k][0].process ((float) l[i]); r[i] = model[k][1].process ((float) r[i]); }
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
}
