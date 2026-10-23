#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Tuning.h"

// The master limiter (MEASUREMENTS.md 4): sample-peak ceiling at 0 dBFS followed by a -0.1 dB trim, stereo-linked,
// with a look-ahead delay. The detector level (in units of the ceiling, >= 1) is the largest of
//   - a triangle around every sample's level: the gain ramps linearly in 1 / gain over the ramp before a peak and
//     falls back the same way after it,
//   - a slow envelope that charges toward it (tau ~ 25 ms) and relaxes to 1 with tau ~ 0.378 s, so long overloads
//     release slowly and short peaks quickly.
namespace ap::dsp
{
class Limiter
{
public:
    void prepare (double rate, const Tuning& t)
    {
        fs = rate;
        lookahead = limiterLookahead (fs);
        rampA = std::max (1.0, t.limRampMs * 1.0e-3 * fs);
        rampR = std::max (1.0, t.limReleaseRampMs * 1.0e-3 * fs);
        holdA = std::max (0.0, t.limHoldAhead); holdR = std::max (0.0, t.limHoldBehind);
        slopeMode = t.limSlopeMode > 0.5;
        slopeA = t.limSlopeAhead * 48000.0 / fs; slopeR = t.limSlopeBehind * 48000.0 / fs;
        ahead = slopeMode ? lookahead : std::min (lookahead, (int) std::ceil (rampA + holdA));
        behind = slopeMode ? lookahead : (int) std::ceil (rampR + holdR);
        slowAtt = 1.0 - std::exp (-1.0 / (t.limSlowAttackMs * 1.0e-3 * fs));
        slowRel = std::exp (-1.0 / (t.limSlowReleaseMs * 1.0e-3 * fs));
        trim = std::pow (10.0, t.limTrimDb / 20.0);
        slowFromLevel = t.limSlowFromLevel > 0.5;
        size = lookahead + behind + 2;
        level.assign ((size_t) size, 1.0);
        for (auto& d : delay) d.assign ((size_t) size, 0.0);
        reset();
    }
    void reset()
    {
        std::fill (level.begin(), level.end(), 1.0);
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0);
        pos = 0; slow = 1.0; grMax = 0.0;
    }
    int latency() const { return lookahead; }

    // one stereo sample in place (the input already carries the limiter's input gain)
    inline void process (double& l, double& r)
    {
        level[(size_t) pos] = std::max (1.0, std::max (std::abs (l), std::abs (r)));
        delay[0][(size_t) pos] = l; delay[1][(size_t) pos] = r;
        // the sample leaving now is `lookahead` behind; every level within the ramps around it pulls the detector up
        const int out = wrap (pos - lookahead);
        double env = 1.0;
        for (int k = -behind; k <= ahead; ++k)
        {
            const double v = level[(size_t) wrap (out + k)];
            if (v <= 1.0) continue;
            if (slopeMode)
                env = std::max (env, v - (k >= 0 ? std::max (0.0, k - holdA) * slopeA : std::max (0.0, -k - holdR) * slopeR));
            else
            {
                const double f = k >= 0 ? (rampA + holdA - k) / rampA : (rampR + holdR + k) / rampR;
                if (f > 0.0) env = std::max (env, 1.0 + (v - 1.0) * std::min (1.0, f));
            }
        }
        const double drive = slowFromLevel ? level[(size_t) out] : env;
        slow = drive > slow ? slow + (drive - slow) * slowAtt : 1.0 + (slow - 1.0) * slowRel;
        env = std::max (env, slow);
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
    inline int wrap (int i) const { return i < 0 ? i + size : (i >= size ? i - size : i); }
    double fs = 48000.0, slowAtt = 0, slowRel = 0, trim = 1, slow = 1, grMax = 0;
    double rampA = 21, rampR = 21, holdA = 0, holdR = 0;
    bool slowFromLevel = true, slopeMode = true;
    double slopeA = 0.17, slopeR = 0.2;
    int lookahead = 38, ahead = 21, behind = 21, size = 64, pos = 0;
    std::vector<double> level, delay[2];
};
}
