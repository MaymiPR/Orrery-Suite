#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include "Filters.h"
#include "Tuning.h"

// The harmonic EQ (MEASUREMENTS.md 4, 10): five boost-only bands, each an RBJ-cookbook filter of its own (a low shelf
// at 44 Hz, bells with Q 2/3 at 100 Hz, 1 and 3 kHz, a high shelf at 9.7 kHz), all fed from the same input. Each
// band's boost, (H - 1) x, passes a 20 Hz Butterworth high-pass and is saturated before it is added:
// y = x + sum c S(boost / c), S a soft clip with ceiling c (nulltest/fit_eqfinal.py, fit_eqsat.py). A knob's dB
// becomes the filter's gain through the measured law (+7.12 dB at 6), scaled per band.
namespace sc::dsp
{
class HarmonicEq
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        auto& t = tuning();
        for (int b = 0; b < 5; ++b)
        {
            for (auto& h : post[b])
            {
                h = Biquad();
                if (t.eqHpHz[b] > 0) h.set (highPass (t.eqHpHz[b], t.eqHpQ[b]).data());
            }
            const bool chunked = t.ctlChunk > 0 && t.chunkEq > 0.5;
            gain[b].setLength ((int) std::lround ((chunked ? t.glideMs : t.eqRampMs) * 0.001 * fs));
            gain[b].setChunked (chunked ? (int) std::lround (t.ctlChunk) : 0, (int) std::lround (t.paramRampMs * 0.001 * fs));
        }
        reset();
    }
    void reset()
    {
        for (int b = 0; b < 5; ++b)
            for (int c = 0; c < 2; ++c) { band[b][c].reset(); post[b][c].reset(); }
        for (auto& g : gain) g.jump (g.target);
        for (int b = 0; b < 5; ++b) design (b);
    }
    // the knobs (dB, 0 ... 6); jump: take them at once (the start of playback)
    void setGains (const float* db, bool jump)
    {
        for (int b = 0; b < 5; ++b)
        {
            const double g = filterGain (db[b]) * tuning().eqGainScale[b];
            if (jump) { gain[b].jump (g); design (b); } else gain[b].set (g);
        }
    }
    void beginBlock (int n) { for (auto& g : gain) g.beginBlock (n); }
    // in place, both channels
    void process (double* l, double* r, int n)
    {
        const double c = tuning().eqCeiling, p = tuning().eqKnee;
        const bool hpAfter = tuning().eqHpAfter > 0.5;
        for (int i = 0; i < n; ++i)
        {
            double addL = 0, addR = 0;
            for (int b = 0; b < 5; ++b)
            {
                if (gain[b].moving())
                {
                    gain[b].next();
                    if ((i & 15) == 0 || ! gain[b].moving()) design (b);
                }
                if (gain[b].value <= 0.0) { band[b][0].reset(); band[b][1].reset(); continue; }
                double bl = band[b][0].process (l[i]) - l[i], br = band[b][1].process (r[i]) - r[i];
                const double cb = c * tuning().eqCeilingScale[b];
                if (hpAfter)
                {
                    addL += post[b][0].process (cb * shape (bl / cb, p));
                    addR += post[b][1].process (cb * shape (br / cb, p));
                }
                else
                {
                    bl = post[b][0].process (bl); br = post[b][1].process (br);
                    addL += cb * shape (bl / cb, p);
                    addR += cb * shape (br / cb, p);
                }
            }
            l[i] += addL;
            r[i] += addR;
        }
    }
    static double shape (double u, double p) { return u / std::pow (1.0 + std::pow (std::abs (u), p), 1.0 / p); }

    // a knob value (dB) as the filter's gain (dB) before the band's scale (Tuning::eqLaw, fitted by nulls)
    static double filterGain (double knobDb)
    {
        static constexpr double knob[] { 0, 0.5, 1, 2, 3, 4, 5, 6 };
        const double* centre = tuning().eqLaw;
        knobDb = std::clamp (knobDb, 0.0, 6.0);
        int i = 1;
        while (i < 7 && knobDb > knob[i]) ++i;
        const double u = (knobDb - knob[i - 1]) / (knob[i] - knob[i - 1]);
        return centre[i - 1] + u * (centre[i] - centre[i - 1]);
    }

private:
    using Coeffs = std::array<double, 5>;
    Coeffs highPass (double f0, double q) const
    {
        const double w = 2 * pi * f0 / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
        return { (1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - al) / a0 };
    }
    // RBJ cookbook: peaking (kind 0), high shelf (1), low shelf (2)
    void design (int b)
    {
        const auto& t = tuning();
        const double A = std::pow (10.0, gain[b].value / 40.0), w = 2 * pi * t.eqHz[b] / fs;
        const double al = std::sin (w) / (2 * t.eqQ[b]), c = std::cos (w);
        double b0, b1, b2, a0, a1, a2;
        if (exactly (t.eqKind[b], 0))
        {
            b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A;
        }
        else if (exactly (t.eqKind[b], 1))
        {
            const double s = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) + (A - 1) * c + s); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - s);
            a0 = (A + 1) - (A - 1) * c + s; a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - s;
        }
        else
        {
            const double s = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) - (A - 1) * c + s); b1 = 2 * A * ((A - 1) - (A + 1) * c); b2 = A * ((A + 1) - (A - 1) * c - s);
            a0 = (A + 1) + (A - 1) * c + s; a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - s;
        }
        const double k[5] { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        for (auto& f : band[b])
        {
            const double z1 = f.z1, z2 = f.z2;
            f.set (k); f.z1 = z1; f.z2 = z2;
        }
    }

    double fs = 48000.0;
    Biquad band[5][2], post[5][2];
    Ramp gain[5];
};
}
