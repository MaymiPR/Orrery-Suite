#pragma once
#include <algorithm>
#include <cmath>
#include "Filters.h"
#include "LearnedModels.h"
#include "LearnedWaveNet.h"
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

// Compression (MEASUREMENTS.md 8, 14): the wet path is the input times a gain from a fast detector on the mid signal
// (a soft knee, ratio ~5:1), then a fixed colour per mode: the gain before the colour, as the reference. The amount
// mixes it with the dry signal exactly. Modes a and b have their own constants (nulltest/fit_comp.py,
// fit_comp_timing.py). The colour is not linear (even harmonics below the threshold, where the gain stands still:
// nulltest/comp_colour.py), so it is learned per mode from the reference's wet output with our gain in front
// (prep_comp.py, train_wn.py: a WaveNet beside a long filter); the linear fit stays as the fallback (compLearned 0).
class Compressor
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            wet[0][c].set (fits::compWetA); wet[1][c].set (fits::compWetB);
            colour[0][c].prepare (models::compA); colour[1][c].prepare (models::compB);
        }
        for (auto* b : { &uL, &uR, &cL, &cR, &amt }) b->assign ((size_t) std::max (1, maxBlock), 0.0);
        reset();
    }
    void reset()
    {
        for (auto& m : wet) for (auto& f : m) f.reset();
        for (auto& m : colour) for (auto& f : m) f.reset();
        env = 0; envSq = 0; gainDb = 0;
        shownGr = smoothGr = sinceTake = 0;
    }
    // the readout above the knob (dB, positive), as the reference's moves (RESEARCH.md UI round 5, films of tones and
    // music): the gain reduction times the amount, smoothed (up 0.25 s, down 0.2 s); the readout holds the highest
    // value since it last took the smoothed one, and takes it again after every 1.5 s of compression (the clock stops
    // while the smoothed reduction is under 0.3 dB: silence and quiet passages keep the last value)
    double readout() const { return shownGr; }
    // in place; mode 0 = a, 1 = b; the amount (0 ... 1) moving per sample; key: the detector's input (the side-chain,
    // EXT) or null (the signal itself). Returns the largest gain reduction of the block (dB, positive).
    double process (double* l, double* r, int n, int mode, Ramp& amount, const double* keyL = nullptr, const double* keyR = nullptr)
    {
        const auto& t = tuning();
        const int m = mode ? 1 : 0;
        const double att = std::exp (-1.0 / (t.compAttackMs[m] * 0.001 * fs)), rel = std::exp (-1.0 / (t.compReleaseMs[m] * 0.001 * fs));
        const double gAtt = t.compGainAttackMs[m] > 0 ? std::exp (-1.0 / (t.compGainAttackMs[m] * 0.001 * fs)) : 0.0;
        const double gRel = t.compGainReleaseMs[m] > 0 ? std::exp (-1.0 / (t.compGainReleaseMs[m] * 0.001 * fs)) : 0.0;
        const double T = t.compThresholdDb[m], R = t.compRatio[m], W = std::max (0.01, t.compKneeDb[m]), rms = t.compRms[m];
        const bool first = t.compGainFirst > 0.5;
        const double up = std::exp (-1.0 / (0.25 * fs)), down = std::exp (-1.0 / (0.2 * fs)), takeEvery = 1.5 * fs;
        const bool learned = t.compLearned > 0.5 && first;
        double most = 0;
        for (int i = 0; i < n; ++i)
        {
            const double mid = keyL != nullptr ? 0.5 * (keyL[i] + keyR[i]) : 0.5 * (l[i] + r[i]), a = std::abs (mid);
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
            const double am = amount.next();
            if (learned) { uL[(size_t) i] = l[i] * g; uR[(size_t) i] = r[i] * g; amt[(size_t) i] = am; }
            else
            {
                // the gain before the wet filters, as the reference (MEASUREMENTS.md 14); after them is the option
                const double wl = first ? wet[m][0].process (l[i] * g) : wet[m][0].process (l[i]) * g;
                const double wr = first ? wet[m][1].process (r[i] * g) : wet[m][1].process (r[i]) * g;
                l[i] += am * (wl - l[i]);
                r[i] += am * (wr - r[i]);
            }
            const double v = gainDb * am;
            smoothGr = v + (smoothGr - v) * (v > smoothGr ? up : down);
            if (smoothGr > 0.3 && ++sinceTake >= takeEvery) { shownGr = smoothGr; sinceTake = 0; }
            shownGr = std::max (shownGr, smoothGr);
        }
        if (learned)
        {
            colour[m][0].process (uL.data(), cL.data(), n);
            colour[m][1].process (uR.data(), cR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                const double am = amt[(size_t) i];
                l[i] += am * (cL[(size_t) i] - l[i]);
                r[i] += am * (cR[(size_t) i] - r[i]);
            }
        }
        return most;
    }

private:
    double fs = 48000.0, env = 0, envSq = 0, gainDb = 0, shownGr = 0, smoothGr = 0, sinceTake = 0;
    ParallelFilter wet[2][2];
    LearnedWaveNet colour[2][2];             // [mode][channel]
    std::vector<double> uL, uR, cL, cR, amt;
};
}
