#pragma once
#include <cmath>
#include <vector>
#include "Filters.h"
#include "Tuning.h"

// The parallels (MEASUREMENTS.md 1, 11): four paths fed from the end of the chain, each adding its own contribution
// to it, scaled by its fader's taper w = (e^(2.188 L) - 1) / (e^2.188 - 1) (MEASUREMENTS.md 5).
//   - sub: the lows doubled, through a 6th-order Butterworth low-pass at 140 Hz and high-pass at 15 Hz
//     (nulltest/fit_sub3.py).
// The others follow.
namespace sc::dsp
{
// Butterworth low- or high-pass of an even order as a cascade of RBJ-cookbook biquads (the same as the bilinear,
// prewarped design)
inline std::vector<Biquad> butterworth (int order, double fc, double fs, bool highPass)
{
    std::vector<Biquad> out;
    const double pi = 3.14159265358979323846;
    for (int k = 0; k < order / 2; ++k)
    {
        const double q = 1.0 / (2.0 * std::sin ((2 * k + 1) * pi / (2.0 * order)));
        const double w = 2 * pi * fc / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
        const double b0 = highPass ? (1 + c) / 2 : (1 - c) / 2, b1 = highPass ? -(1 + c) : 1 - c;
        const double coeffs[5] { b0 / a0, b1 / a0, b0 / a0, -2 * c / a0, (1 - al) / a0 };
        Biquad b; b.set (coeffs); out.push_back (b);
    }
    return out;
}

inline double faderTaper (double level)
{
    return (std::exp (2.188 * level) - 1.0) / (std::exp (2.188) - 1.0);
}

class SubPath
{
public:
    void prepare (double fs)
    {
        const auto& t = tuning();
        for (auto& ch : chain)
        {
            ch = butterworth (6, t.subLowPassHz, fs, false);
            const auto hp = butterworth (6, t.subHighPassHz, fs, true);
            ch.insert (ch.end(), hp.begin(), hp.end());
        }
    }
    void reset() { for (auto& ch : chain) for (auto& b : ch) b.reset(); }
    // adds w times the path's contribution to out (in: the chain's output)
    void process (const double* inL, const double* inR, double* outL, double* outR, int n, Ramp& w)
    {
        const double g = tuning().subGain;
        for (int i = 0; i < n; ++i)
        {
            double l = inL[i], r = inR[i];
            for (auto& b : chain[0]) l = b.process (l);
            for (auto& b : chain[1]) r = b.process (r);
            const double m = w.next();
            outL[i] += m * g * l;
            outR[i] += m * g * r;
        }
    }

private:
    std::vector<Biquad> chain[2];
};
}
