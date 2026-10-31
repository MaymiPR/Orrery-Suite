#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Tuning.h"

// The master limiter (MEASUREMENTS.md 4), identified with probe signals (nulltest/lim_probe.py): a ceiling at 0 dBFS
// followed by a -0.1 dB trim, stereo-linked, with a look-ahead delay. The gain is 1 / env, where env (in units of the
// ceiling, >= 1) is the largest of:
//   - the level of the sample leaving now: its inter-sample (16x interpolated) peak over [n, n + 1);
//   - a straight-line plan through the upcoming peaks. Only local maxima of the level are targets. When a plan ends,
//     the next one heads for the nearest peak within `window` samples ahead, reaching it on arrival. With none, it
//     falls toward 1 over `releaseSteps` samples. A running plan is changed only when a level within its span rises
//     above it. Plans may run under the slow envelope; only an idle limiter rests on it;
//   - a slow envelope charged by the level (tau limSlowAttackMs) that releases toward the current level, or 1 when
//     quiet (tau limSlowReleaseMs), so long overloads release slowly and short peaks quickly.
namespace ap::dsp
{
class Limiter
{
public:
    void prepare (double rate, const Tuning& t)
    {
        fs = rate;
        lookahead = limiterLookahead (fs);
        // window and release are set per rate, not scaled from 48 kHz (single and paired peaks probed at each rate), and
        // so is the inter-sample interpolator: a Kaiser-windowed sinc whose taps and window were fitted on program
        // material at each rate (nulltest/limrate.py). Its effective width is not constant (about a Gaussian of
        // sigma 4.4 samples at 44.1 kHz, 3.5 at 48, 1.3 - 1.8 above), so it is no fixed-length kernel scaled with the rate.
        struct PerRate { double rate; int window, release, taps; double beta; };
        constexpr PerRate perRate[] { { 44100, 19, 20, 28, 10.0 }, { 48000, 21, 22, 24, 12.0 }, { 88200, 39, 40, 12, 11.0 },
                                      { 96000, 43, 44, 12, 14.5 }, { 192000, 86, 87, 10, 16.0 } };
        double w = t.limWindow * fs / 48000.0, rel = t.limReleaseSteps * fs / 48000.0;
        const PerRate* nearest = &perRate[1];
        for (const auto& pr : perRate)
        {
            if (std::abs (fs - pr.rate) < 1.0) { w = pr.window * t.limWindow / 21.0; rel = pr.release * t.limReleaseSteps / 22.0; }
            if (std::abs (fs - pr.rate) < std::abs (fs - nearest->rate)) nearest = &pr;
        }
        if (t.limWindowAt > 0) w = t.limWindowAt;
        if (t.limReleaseAt > 0) rel = t.limReleaseAt;
        nTaps = std::clamp ((t.limTaps > 0 ? (int) t.limTaps : nearest->taps) / 2 * 2, 4, kTaps);
        beta = t.limBeta > 0 ? t.limBeta : nearest->beta;
        window = std::clamp ((int) std::lround (w), 1, lookahead - nTaps / 2 - 1);
        releaseSteps = std::max (1, (int) std::lround (rel));
        slowAtt = 1.0 - std::exp (-1.0 / (t.limSlowAttackMs * 1.0e-3 * fs));
        slowRel = std::exp (-1.0 / (t.limSlowReleaseMs * 1.0e-3 * fs));
        trim = std::pow (10.0, t.limTrimDb / 20.0);
        // interpolation for the inter-sample level: Kaiser-windowed sinc, nTaps taps per phase
        nSub = std::clamp ((int) t.limSub, 1, kSub);
        for (int ph = 0; ph < nSub; ++ph)
        {
            const double frac = ph / (double) nSub;
            double sum = 0;
            for (int i = 0; i < nTaps; ++i)
            {
                const double x = (i - (nTaps / 2 - 1)) - frac;
                const double cx = t.limCut * x;
                const double sinc = std::abs (cx) < 1e-12 ? 1.0 : std::sin (kPiL * cx) / (kPiL * cx);
                const double r = x / (nTaps / 2.0);
                const double win = t.limGauss > 0 ? std::exp (-0.5 * x * x / (t.limGauss * t.limGauss))
                                                  : std::abs (r) >= 1 ? 0.0 : besselI0 (beta * std::sqrt (1 - r * r)) / besselI0 (beta);
                interp[ph][i] = sinc * win; sum += interp[ph][i];
            }
            for (int i = 0; i < nTaps; ++i) interp[ph][i] *= t.limLevelScale / sum;
        }
        size = lookahead + 4;
        level.assign ((size_t) size, 1.0);
        partial.assign ((size_t) size, 1.0);
        {
            // how far a release looks for a level to fall toward (per rate as the window; 22.67 at 48 kHz)
            const double reach = std::min ((double) (lookahead - nTaps / 2 - 1), t.limReach * window / 21.0);
            reachWhole = (int) std::floor (reach);
            reachPhases = (int) std::floor ((reach - reachWhole) * nSub);
            if (t.limReach <= 0) reachWhole = 0;
        }
        sampleAbs.assign ((size_t) size, 0.0);
        peakTest = (int) t.limPeakTest;
        for (auto& d : delay) d.assign ((size_t) size, 0.0);
        reset();
    }
    void reset()
    {
        std::fill (level.begin(), level.end(), 1.0);
        std::fill (partial.begin(), partial.end(), 1.0);
        std::fill (sampleAbs.begin(), sampleAbs.end(), 0.0);
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0);
        for (auto& h : hist) std::fill (std::begin (h), std::end (h), 0.0);
        pos = hpos = 0; slow = 1.0; grMax = 0.0;
        state = 1.0; planSlope = 0.0; planLeft = 0; planTarget = 1.0; planRelease = false;
    }
    int latency() const { return lookahead; }

    // one stereo sample in place (the input already carries the limiter's input gain)
    inline void process (double& l, double& r)
    {
        // the inter-sample level of the sample nTaps / 2 back (the interpolator's delay, taken from the look-ahead)
        // (each sample written twice, nTaps apart, so the last nTaps are always in a row: no wrapping in the sums)
        hist[0][hpos] = hist[0][hpos + nTaps] = l; hist[1][hpos] = hist[1][hpos + nTaps] = r;
        hpos = hpos + 1 == nTaps ? 0 : hpos + 1;
        double m = 0, mPart = 0;
        for (int c = 0; c < 2; ++c)
            for (int ph = 0; ph < nSub; ++ph)
            {
                double acc = 0;
                const double* x = hist[c] + hpos;
                const double* k = interp[ph];
                for (int i = 0; i < nTaps; ++i) acc += k[i] * x[i];
                m = std::max (m, std::abs (acc));
                if (ph == reachPhases) mPart = std::max (mPart, std::abs (acc));
            }
        const int lv = wrap (pos - nTaps / 2);
        level[(size_t) lv] = std::max (1.0, m);
        partial[(size_t) lv] = std::max (1.0, mPart);      // the level at one point inside the sample interval
        level[(size_t) pos] = 1.0;
        delay[0][(size_t) pos] = l; delay[1][(size_t) pos] = r;
        sampleAbs[(size_t) lv] = std::max (std::abs (delay[0][(size_t) lv]), std::abs (delay[1][(size_t) lv]));

        const int out = wrap (pos - lookahead);
        const double now = level[(size_t) out];
        const double env = std::max (state, std::max (slow, now));
        if (planLeft <= 0 && planSlope == 0.0) state = std::max (state, slow);        // idle: rest on the slow level
        updatePlan (out);
        if (planLeft > 0)
        {
            state += planSlope;
            if (--planLeft == 0) state = planTarget;
            if (planRelease && state <= slow) { state = slow; planLeft = 0; planSlope = 0.0; }
        }
        if (now > slow) slow += (now - slow) * slowAtt;
        else slow = now + (slow - now) * slowRel;                 // toward the current level (1 when quiet)

        const double g = trim / env;
        grMax = std::max (grMax, env);
        l = delay[0][(size_t) out] * g;
        r = delay[1][(size_t) out] * g;
        pos = pos + 1 == size ? 0 : pos + 1;
    }

    // deepest reduction (dB, positive) since the last call
    float takeGr()
    {
        const float v = (float) (20.0 * std::log10 (grMax > 1.0 ? grMax : 1.0));
        grMax = 0.0;
        return v;
    }

private:
    static constexpr int kTaps = 32, kSub = 32;
    int nSub = 16, nTaps = 16;
    double beta = 6.0;
    static constexpr double kPiL = 3.14159265358979323846;
    static double besselI0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
    inline int wrap (int i) const { return i < 0 ? i + size : (i >= size ? i - size : i); }

    // a peak k samples ahead of the outgoing sample, or 1 when the level there is not a local maximum
    inline double peakAt (int out, int k) const
    {
        const double v = level[(size_t) wrap (out + k)];
        if (peakTest == 1)          // a local maximum of the sample magnitudes
        {
            const double sv = sampleAbs[(size_t) wrap (out + k)];
            return sv >= sampleAbs[(size_t) wrap (out + k - 1)] && sv >= sampleAbs[(size_t) wrap (out + k + 1)] ? v : 1.0;
        }
        if (peakTest == 2)          // a local maximum over two neighbours each side
            return v >= level[(size_t) wrap (out + k - 1)] && v >= level[(size_t) wrap (out + k + 1)]
                   && v >= level[(size_t) wrap (out + k - 2)] && v >= level[(size_t) wrap (out + k + 2)] ? v : 1.0;
        return v >= level[(size_t) wrap (out + k - 1)] && v >= level[(size_t) wrap (out + k + 1)] ? v : 1.0;
    }

    void updatePlan (int out)
    {
        if (planLeft > 0)
        {
            // a level above the plan's line: re-plan to the steepest such point. A plan toward a peak watches its own
            // span only; a release watches the whole window, its line continued at 1 past its end
            double steep = -1.0e9; int at = 0;
            const int horizon = planRelease ? window : std::min (planLeft, window);
            for (int k = 1; k <= horizon; ++k)
            {
                const double v = peakAt (out, k);
                const double line = k <= planLeft ? state + planSlope * k : planTarget;
                if (v > line + 1.0e-12 && (v - state) / k > steep) { steep = (v - state) / k; at = k; }
            }
            if (at > 0) { planSlope = steep; planLeft = at; planTarget = state + steep * at; planRelease = false; }
            return;
        }
        // a new plan: the nearest peak ahead, or a release toward 1
        for (int k = 1; k <= window; ++k)
        {
            const double v = peakAt (out, k);
            if (v > 1.0) { planSlope = (v - state) / k; planLeft = k; planTarget = v; planRelease = false; return; }
        }
        // a release over releaseSteps: right after a peak toward the level `reach` samples ahead (the flank of a peak
        // still beyond the window; 1 when there is none there), else toward 1. An idle limiter stays idle
        const bool afterPeak = ! planRelease;
        planRelease = true;
        if (state <= std::max (1.0, slow) + 1.0e-9) { planLeft = 0; planSlope = 0.0; planTarget = state; return; }   // idle
        const double to = afterPeak && reachWhole > 0 ? partial[(size_t) wrap (out + reachWhole)] : 1.0;
        planSlope = (to - state) / releaseSteps; planLeft = releaseSteps; planTarget = to;
    }

    double fs = 48000.0, slowAtt = 0, slowRel = 0, trim = 1, slow = 1, grMax = 0;
    double state = 1.0, planSlope = 0.0, planTarget = 1.0;
    bool planRelease = false;
    int lookahead = 38, window = 21, releaseSteps = 22, size = 64, pos = 0, hpos = 0, planLeft = 0;
    double interp[kSub][kTaps] {}, hist[2][2 * kTaps] {};
    std::vector<double> level, partial, sampleAbs, delay[2];
    int reachWhole = 0, reachPhases = 0;
    int peakTest = 0;
};
}
