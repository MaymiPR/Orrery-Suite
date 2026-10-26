#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Tuning.h"

// The low-level expander that drive, harmonics, compression and lift share (MEASUREMENTS.md 3): below a threshold
// (a sine's peak at ~-58 dBFS) the gain falls 2 dB per dB. It reads the peak of the last ~1.2 s (so it opens at once
// and closes the same time after any louder passage), and the gain closes smoothly.
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
        reset();
    }
    void reset()
    {
        std::fill (peaks.begin(), peaks.end(), 0.0);      // (the reference starts from the signal's own level)
        pos = 0; count = 0; current = 0; gainDb = 0; window = 0.0; started = false;
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
            const double target = below <= 0 ? 0.0 : -t.expSlope * below;
            gainDb = target >= gainDb || ! started ? target : target + (gainDb - target) * release;
            started = true;
            out[i] = std::pow (10.0, gainDb / 20.0);
        }
    }

private:
    double fs = 48000.0, release = 0.0, current = 0, gainDb = 0, window = 1.0;   // window: the peak of the chunks held
    int chunk = 240, pos = 0, count = 0;
    bool started = false;
    std::vector<double> peaks;
};
}
