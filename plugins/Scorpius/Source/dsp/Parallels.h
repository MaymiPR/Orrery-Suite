#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Filters.h"
#include "Tuning.h"

// The parallels (MEASUREMENTS.md 1, 11): four paths fed from the end of the chain, each adding its own contribution
// to it, scaled by its fader's taper w = (e^(2.188 L) - 1) / (e^2.188 - 1) (MEASUREMENTS.md 5).
//   - sub: the lows doubled, through a 6th-order Butterworth low-pass at 140 Hz and high-pass at 15 Hz
//     (nulltest/fit_sub3.py);
//   - loud: upward compression, the gain read from a table against a fast detector on the mid signal; its
//     contribution is (gain - 1) x (nulltest/fit_loud.py).
//   - low: a subharmonic synthesizer, an octave below the bass: a square wave that flips at each falling zero
//     crossing of the low-passed input (half its frequency), times that input's peak envelope, through a 4th-order
//     low-pass at 46 Hz, then v + a v^2 (its DC kept) and a soft limit (nulltest/fit_low2.py).
//   - grit: a learned model (LearnedModules.h).
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

class LoudPath
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { env = envSq = 0; }
    void process (const double* inL, const double* inR, double* outL, double* outR, int n, Ramp& w)
    {
        const auto& t = tuning();
        const double att = std::exp (-1.0 / (t.loudAttackMs * 0.001 * fs)), rel = std::exp (-1.0 / (t.loudReleaseMs * 0.001 * fs));
        for (int i = 0; i < n; ++i)
        {
            const double mid = 0.5 * (inL[i] + inR[i]), a = std::abs (mid);
            env = a > env ? a + (env - a) * att : a + (env - a) * rel;
            envSq = mid * mid > envSq ? mid * mid + (envSq - mid * mid) * att : mid * mid + (envSq - mid * mid) * rel;
            const double level = (1 - t.loudRms) * env + t.loudRms * std::sqrt (2.0 * envSq);
            const double db = 20.0 * std::log10 (std::max (level, 1.0e-9));
            // the gain table: nodes every 6 dB from -72 dBFS
            const double u = std::clamp ((db + 72.0) / 6.0, 0.0, 14.0);
            const int k = std::min (13, (int) u);
            const double g = t.loudCurve[k] + (u - k) * (t.loudCurve[k + 1] - t.loudCurve[k]);
            const double m = w.next() * (std::pow (10.0, g / 20.0) - 1.0);
            outL[i] += m * inL[i];
            outR[i] += m * inR[i];
        }
    }

private:
    double fs = 48000.0, env = 0, envSq = 0;
};

class LowPath
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const auto& t = tuning();
        for (int c = 0; c < 2; ++c)
        {
            detLp[c] = butterworth (2 * (int) std::lround (t.lowDetOrder / 2), t.lowDetHz, fs, false);
            outLp[c] = butterworth (2 * (int) std::lround (t.lowOutOrder / 2), t.lowOutHz, fs, false);
        }
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& b : detLp[c]) b.reset();
            for (auto& b : outLp[c]) b.reset();
            armed[c] = false; state[c] = 0; env[c] = 0;
        }
    }
    void process (const double* inL, const double* inR, double* outL, double* outR, int n, Ramp& w)
    {
        const auto& t = tuning();
        const double* in[2] { inL, inR };
        double* out[2] { outL, outR };
        const double ka = std::exp (-1.0 / (std::max (0.01, t.lowAttackMs) * 0.001 * fs));
        const double kr = std::exp (-1.0 / (std::max (0.01, t.lowReleaseMs) * 0.001 * fs));
        const double h = t.lowHysteresis;
        for (int i = 0; i < n; ++i)
        {
            const double m = w.next();
            for (int c = 0; c < 2; ++c)
            {
                double d = in[c][i];
                for (auto& b : detLp[c]) d = b.process (d);
                // the flip-flop: silent until the first falling zero crossing, then flipping at each one (it keeps its
                // state through silence); the reference's first state is the negative one
                if (d > h) armed[c] = true;
                if (armed[c] && d < -h) { state[c] = state[c] == 0 ? -1.0 : -state[c]; armed[c] = false; }
                const double a = std::abs (d);
                env[c] = a > env[c] ? a + (env[c] - a) * ka : a + (env[c] - a) * kr;
                double s = t.lowGain * state[c] * env[c];
                for (auto& b : outLp[c]) s = b.process (s);
                s += t.lowSquare * s * s;
                out[c][i] += m * t.lowCeiling * std::tanh (s / t.lowCeiling);
            }
        }
    }

private:
    double fs = 48000.0, state[2] {}, env[2] {};
    bool armed[2] {};
    std::vector<Biquad> detLp[2], outLp[2];
};

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
