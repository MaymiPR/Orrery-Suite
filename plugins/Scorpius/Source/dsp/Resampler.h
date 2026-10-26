#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Streaming sample-rate conversion by a fixed ratio (Kaiser-windowed sinc, a table of phases interpolated linearly),
// for running the DSP at 48 kHz whatever the host's rate, as the reference does (MEASUREMENTS.md 2: its own
// resampler is linear-phase, 31 samples of latency at 44.1 kHz).
namespace sc::dsp
{
class Resampler
{
public:
    static constexpr int half = 16, phases = 512;

    // in and out rates; the cutoff is a share of the lower rate's Nyquist
    void prepare (double inRate, double outRate, double cutoff = 0.92)
    {
        step = inRate / outRate;
        const double fc = cutoff * std::min (1.0, outRate / inRate);       // in input-rate Nyquist units
        table.assign ((size_t) (phases + 1) * 2 * half, 0.0);
        const double beta = 8.0, pi = 3.14159265358979323846;
        auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        for (int p = 0; p <= phases; ++p)
        {
            const double frac = (double) p / phases;
            for (int k = 0; k < 2 * half; ++k)
            {
                const double d = (k - half) + frac;                  // the output instant re tap k (k = 0: the newest input)
                const double x = pi * fc * d;
                const double sinc = std::abs (d) < 1e-12 ? 1.0 : std::sin (x) / x;
                const double r = d / half;
                const double win = std::abs (r) >= 1 ? 0.0 : bessel0 (beta * std::sqrt (1 - r * r)) / bessel0 (beta);
                table[(size_t) p * 2 * half + (size_t) k] = fc * sinc * win;
            }
        }
        history.assign ((size_t) 4 * half, 0.0);
        reset();
    }
    void reset() { std::fill (history.begin(), history.end(), 0.0); pos = offset; filled = 0; }
    // the delay it adds, in input samples: half, less the start offset (0 ... 1) that trims it
    double latencyIn() const { return half - offset; }
    void setOffset (double o) { offset = std::clamp (o, 0.0, 0.999); pos = offset; }

    // converts n input samples, appending the output to out
    void process (const float* in, int n, std::vector<float>& out)
    {
        for (int i = 0; i < n; ++i)
        {
            // keep the last 2 half input samples (a doubled ring for contiguous reads)
            const int h = 2 * half;
            history[(size_t) (filled % h)] = in[i];
            history[(size_t) (filled % h + h)] = in[i];
            ++filled;
            // outputs whose instants fall within the latest input interval
            while (pos < 1.0)
            {
                const int p = std::min (phases - 1, (int) (pos * phases));
                const double u = pos * phases - p;
                const double* a = table.data() + (size_t) p * h;
                const double* b = a + h;
                const double* x = history.data() + (size_t) (filled % h);  // oldest .. newest
                double acc = 0;
                for (int k = 0; k < h; ++k) acc += x[h - 1 - k] * (a[k] + u * (b[k] - a[k]));
                out.push_back ((float) acc);
                pos += step;
            }
            pos -= 1.0;
        }
    }

private:
    std::vector<double> table, history;
    double step = 1.0, pos = 0.0, offset = 0.0;
    long long filled = 0;
};
}
