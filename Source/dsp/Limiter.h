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
        // window and release are set per rate, not scaled from 48 kHz (single and paired peaks probed at each rate)
        struct PerRate { double rate; int window, release; };
        constexpr PerRate perRate[] { { 44100, 19, 20 }, { 48000, 21, 22 }, { 88200, 39, 40 }, { 96000, 43, 44 }, { 192000, 86, 87 } };
        double w = t.limWindow * fs / 48000.0, rel = t.limReleaseSteps * fs / 48000.0;
        for (const auto& pr : perRate)
            if (std::abs (fs - pr.rate) < 1.0) { w = pr.window * t.limWindow / 21.0; rel = pr.release * t.limReleaseSteps / 22.0; }
        if (t.limWindowAt > 0) w = t.limWindowAt;
        if (t.limReleaseAt > 0) rel = t.limReleaseAt;
        window = std::clamp ((int) std::lround (w), 1, lookahead - kTaps / 2 - 1);
        releaseSteps = std::max (1, (int) std::lround (rel));
        slowAtt = 1.0 - std::exp (-1.0 / (t.limSlowAttackMs * 1.0e-3 * fs));
        slowRel = std::exp (-1.0 / (t.limSlowReleaseMs * 1.0e-3 * fs));
        trim = std::pow (10.0, t.limTrimDb / 20.0);
        // interpolation for the inter-sample level: Kaiser-windowed sinc, kTaps taps per phase
        for (int ph = 0; ph < kSub; ++ph)
        {
            const double frac = ph / (double) kSub;
            double sum = 0;
            for (int i = 0; i < kTaps; ++i)
            {
                const double x = (i - (kTaps / 2 - 1)) - frac;
                const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPiL * x) / (kPiL * x);
                const double r = x / (kTaps / 2.0), w = std::abs (r) >= 1 ? 0.0 : besselI0 (6.0 * std::sqrt (1 - r * r)) / besselI0 (6.0);
                interp[ph][i] = sinc * w; sum += interp[ph][i];
            }
            for (auto& c : interp[ph]) c /= sum;
        }
        size = lookahead + 4;
        level.assign ((size_t) size, 1.0);
        for (auto& d : delay) d.assign ((size_t) size, 0.0);
        reset();
    }
    void reset()
    {
        std::fill (level.begin(), level.end(), 1.0);
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0);
        for (auto& h : hist) std::fill (std::begin (h), std::end (h), 0.0);
        pos = hpos = 0; slow = 1.0; grMax = 0.0;
        state = 1.0; planSlope = 0.0; planLeft = 0; planTarget = 1.0; planRelease = false;
    }
    int latency() const { return lookahead; }

    // one stereo sample in place (the input already carries the limiter's input gain)
    inline void process (double& l, double& r)
    {
        // the inter-sample level of the sample kTaps / 2 back (the interpolator's delay, taken from the look-ahead)
        hist[0][hpos] = l; hist[1][hpos] = r;
        hpos = (hpos + 1) % kTaps;
        double m = 0;
        for (int c = 0; c < 2; ++c)
            for (int ph = 0; ph < kSub; ++ph)
            {
                double acc = 0;
                for (int i = 0; i < kTaps; ++i) acc += interp[ph][i] * hist[c][(hpos + i) % kTaps];
                m = std::max (m, std::abs (acc));
            }
        level[(size_t) wrap (pos - kTaps / 2)] = std::max (1.0, m);
        level[(size_t) pos] = 1.0;
        delay[0][(size_t) pos] = l; delay[1][(size_t) pos] = r;

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
    static constexpr int kTaps = 16, kSub = 16;
    static constexpr double kPiL = 3.14159265358979323846;
    static double besselI0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
    inline int wrap (int i) const { return i < 0 ? i + size : (i >= size ? i - size : i); }

    // a peak k samples ahead of the outgoing sample, or 1 when the level there is not a local maximum
    inline double peakAt (int out, int k) const
    {
        const double v = level[(size_t) wrap (out + k)];
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
        planRelease = true;
        if (state <= std::max (1.0, slow) + 1.0e-9) { planLeft = 0; planSlope = 0.0; planTarget = state; }   // idle
        else { planSlope = (1.0 - state) / releaseSteps; planLeft = releaseSteps; planTarget = 1.0; }
    }

    double fs = 48000.0, slowAtt = 0, slowRel = 0, trim = 1, slow = 1, grMax = 0;
    double state = 1.0, planSlope = 0.0, planTarget = 1.0;
    bool planRelease = false;
    int lookahead = 38, window = 21, releaseSteps = 22, size = 64, pos = 0, hpos = 0, planLeft = 0;
    double interp[kSub][kTaps] {}, hist[2][kTaps] {};
    std::vector<double> level, delay[2];
};
}
