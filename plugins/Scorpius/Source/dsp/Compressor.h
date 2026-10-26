#pragma once
#include <algorithm>
#include <cmath>
#include "LinearFits.h"
#include "Tuning.h"

// A linear filter in parallel form, d + sum (b0 + b1 z^-1) / (1 + a1 z^-1 + a2 z^-2) (vector fits, LinearFits.h).
namespace sc::dsp
{
class ParallelFilter
{
public:
    void set (const fits::Parallel& p) { d = p.d; secs = p.secs; state.assign (secs.size(), { 0, 0 }); }
    void reset() { for (auto& s : state) s = { 0, 0 }; }
    double process (double x)
    {
        double y = d * x;
        for (size_t k = 0; k < secs.size(); ++k)
        {
            const auto& c = secs[k];
            auto& z = state[k];
            const double out = c[0] * x + z[0];          // transposed direct form II, numerator b0 + b1 z^-1
            z[0] = c[1] * x - c[2] * out + z[1];
            z[1] = -c[3] * out;
            y += out;
        }
        return y;
    }

private:
    double d = 1;
    std::vector<std::array<double, 4>> secs;
    std::vector<std::array<double, 2>> state;
};

// Compression (MEASUREMENTS.md 8): the wet path is the input through a fixed filter (its make-up and gentle shelving,
// fitted) times a gain from a fast detector on the mid signal: a soft knee, ratio ~5:1. The amount mixes it with the
// dry signal exactly. Modes a and b have their own constants (nulltest/fit_comp.py).
class Compressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c) { wet[0][c].set (fits::compWetA); wet[1][c].set (fits::compWetB); }
        reset();
    }
    void reset()
    {
        for (auto& m : wet) for (auto& f : m) f.reset();
        env = 0; envSq = 0; gainDb = 0;
    }
    // in place; mode 0 = a, 1 = b; amount 0 ... 1. Returns the largest gain reduction of the block (dB, positive).
    double process (double* l, double* r, int n, int mode, double amount)
    {
        const auto& t = tuning();
        const int m = mode ? 1 : 0;
        const double att = std::exp (-1.0 / (t.compAttackMs[m] * 0.001 * fs)), rel = std::exp (-1.0 / (t.compReleaseMs[m] * 0.001 * fs));
        const double gAtt = t.compGainAttackMs[m] > 0 ? std::exp (-1.0 / (t.compGainAttackMs[m] * 0.001 * fs)) : 0.0;
        const double gRel = t.compGainReleaseMs[m] > 0 ? std::exp (-1.0 / (t.compGainReleaseMs[m] * 0.001 * fs)) : 0.0;
        const double T = t.compThresholdDb[m], R = t.compRatio[m], W = std::max (0.01, t.compKneeDb[m]), rms = t.compRms[m];
        double most = 0;
        for (int i = 0; i < n; ++i)
        {
            const double mid = 0.5 * (l[i] + r[i]), a = std::abs (mid);
            env = a > env ? a + (env - a) * att : a + (env - a) * rel;
            envSq = mid * mid > envSq ? mid * mid + (envSq - mid * mid) * att : mid * mid + (envSq - mid * mid) * rel;
            const double level = (1 - rms) * env + rms * std::sqrt (2.0 * envSq);      // (a sine's RMS x sqrt 2 reads its peak)
            const double over = 20.0 * std::log10 (std::max (level, 1.0e-9)) - T;
            double target = 0;
            if (over >= W / 2) target = (1 - 1 / R) * over;
            else if (over > -W / 2) target = (1 - 1 / R) * (over + W / 2) * (over + W / 2) / (2 * W);
            gainDb = target > gainDb ? target + (gainDb - target) * gAtt : target + (gainDb - target) * gRel;
            most = std::max (most, gainDb);
            const double g = std::pow (10.0, -gainDb / 20.0);
            const double wl = wet[m][0].process (l[i]) * g, wr = wet[m][1].process (r[i]) * g;
            l[i] += amount * (wl - l[i]);
            r[i] += amount * (wr - r[i]);
        }
        return most;
    }

private:
    double fs = 48000.0, env = 0, envSq = 0, gainDb = 0;
    ParallelFilter wet[2][2];
};
}
