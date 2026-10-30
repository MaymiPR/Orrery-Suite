#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Tuning.h"

// The low-level expander that drive, harmonics, compression and lift share (MEASUREMENTS.md 3): below a threshold
// (a sine's peak at ~-58 dBFS) the gain falls 2 dB per dB, to no lower than -44.5 dB. It reads the peak of the last
// ~1.2 s (so it opens as soon as the level rises and closes the same time after any louder passage); the gain opens
// with a 1 ms one-pole on its linear value and closes smoothly in dB. Reset, it is closed, as after silence (the
// reference's modules start that way when switched on; MEASUREMENTS.md 3, 12).
namespace sc::dsp
{
class Expander
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const auto& t = tuning();
        chunk = std::max (1, (int) std::lround (0.005 * fs));
        peaks.assign ((size_t) std::max (1, (int) std::lround (t.expHoldMs * 0.001 * fs / chunk)), 0.0);
        release = std::exp (-1.0 / (t.expReleaseMs * 0.001 * fs));
        attack = std::exp (-1.0 / (t.expAttackMs * 0.001 * fs));
        reset();
    }
    void reset()
    {
        std::fill (peaks.begin(), peaks.end(), 0.0);
        pos = 0; count = 0; current = 0; window = 0.0;
        gainDb = tuning().expFloorDb; gain = std::pow (10.0, gainDb / 20.0);
    }
    // the gain (linear) for each sample of a stereo block, from its own input
    void gains (const double* l, const double* r, double* out, int n)
    {
        const auto& t = tuning();
        for (int i = 0; i < n; ++i)
        {
            current = std::max (current, std::max (std::abs (l[i]), std::abs (r[i])));
            const double held = std::max (current, window);
            if (++count >= chunk)
            {
                peaks[(size_t) pos] = current;
                pos = (pos + 1) % (int) peaks.size();
                current = 0; count = 0;
                window = *std::max_element (peaks.begin(), peaks.end());
            }
            const double levelDb = 20.0 * std::log10 (std::max (held, 1.0e-9));
            const double below = t.expThresholdDb - levelDb;
            const double target = below <= 0 ? 0.0 : std::max (t.expFloorDb, -t.expSlope * below);
            if (target >= gainDb)
            {
                gain += (1.0 - attack) * (std::pow (10.0, target / 20.0) - gain);
                gainDb = 20.0 * std::log10 (gain);
            }
            else
            {
                gainDb = target + (gainDb - target) * release;
                gain = std::pow (10.0, gainDb / 20.0);
            }
            out[i] = gain;
        }
    }

private:
    double fs = 48000.0, release = 0.0, attack = 0.0, current = 0, gainDb = 0, gain = 1.0, window = 1.0;   // window: the peak of the chunks held
    int chunk = 240, pos = 0, count = 0;
    std::vector<double> peaks;
};
}
