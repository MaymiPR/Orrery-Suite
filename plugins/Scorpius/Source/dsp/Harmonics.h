#pragma once
#include <cmath>
#include "Filters.h"
#include "Tuning.h"

// Harmonics (MEASUREMENTS.md 7): the reference runs a neural model here; ours is the structure its measurements show
// (nulltest/fit_harmlin.py, fit_harm.py): a wide RBJ bell (Q ~0.32, +5.8 dB) at 1.4 times the chosen frequency, its
// boost saturated by a soft clip and then high-passed at 20 Hz, added to the signal: W = x + HP(c S(boost / c)).
// The amount mixes W with the dry signal, a little under linearly (the measured weights).
namespace sc::dsp
{
class Harmonics
{
public:
    void prepare (double sampleRate) { fs = sampleRate; design (80); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { bell[c].reset(); hp[c].reset(); } }
    void setMode (int freqHz) { if (freqHz != mode) design (freqHz); }
    // in place: the wet signal mixed with the dry one by the amount (0 ... 1)
    void process (double* l, double* r, int n, double amount)
    {
        const auto& t = tuning();
        const int m = mode == 50 ? 1 : 0;
        const double c = t.harmCeiling[m], p = t.harmKnee[m], w = weight (amount);
        double* ch[2] { l, r };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                const double x = ch[k][i];
                const double boost = bell[k].process (x) - x;
                const double u = boost / c;
                const double wetAdd = hp[k].process (c * u / std::pow (1.0 + std::pow (std::abs (u), p), 1.0 / p));
                ch[k][i] = x + w * wetAdd;
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
    void design (int freqHz)
    {
        mode = freqHz;
        const auto& t = tuning();
        const int m = mode == 50 ? 1 : 0;
        const double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, t.harmGainDb[m] / 40.0), w = 2 * pi * t.harmHz[m] / fs;
        const double al = std::sin (w) / (2 * t.harmQ[m]), c = std::cos (w), a0 = 1 + al / A;
        const double k[5] { (1 + al * A) / a0, -2 * c / a0, (1 - al * A) / a0, -2 * c / a0, (1 - al / A) / a0 };
        const double wh = 2 * pi * t.harmHpHz[m] / fs, alh = std::sin (wh) / (2 * 0.70710678), ch = std::cos (wh), h0 = 1 + alh;
        const double kh[5] { (1 + ch) / 2 / h0, -(1 + ch) / h0, (1 + ch) / 2 / h0, -2 * ch / h0, (1 - alh) / h0 };
        for (int ch_ = 0; ch_ < 2; ++ch_) { bell[ch_].set (k); hp[ch_].set (kh); }
    }

    double fs = 48000.0;
    int mode = 80;
    Biquad bell[2], hp[2];
};
}
