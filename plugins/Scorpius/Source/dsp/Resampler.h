#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Streaming sample-rate conversion by a fixed ratio (Kaiser-windowed sinc, a table of phases interpolated linearly),
// for running the DSP at 48 kHz whatever the host's rate, as the reference does (MEASUREMENTS.md 2: its own
// resampler is linear-phase, 31 samples of latency at 44.1 kHz). At integer ratios (96 / 192 kHz) it can instead run
// the reference's own converters, identified (ConverterFits.h, MEASUREMENTS.md 14): not linear-phase.
namespace sc::dsp
{
class Resampler
{
public:
    static constexpr int phases = 512;

    // in and out rates; the cutoff is a share of the lower rate's Nyquist; the filter's half length (input samples)
    // and its Kaiser window's beta
    void prepare (double inRate, double outRate, double cutoff = 0.92, int halfLength = 16, double beta = 8.0)
    {
        ratio = 0;
        half = std::max (2, halfLength);
        step = inRate / outRate;
        const double fc = cutoff * std::min (1.0, outRate / inRate);       // in input-rate Nyquist units
        table.assign ((size_t) (phases + 1) * 2 * half, 0.0);
        const double pi = 3.14159265358979323846;
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
    // an integer ratio with given filters at the higher rate: down keeps the input's first sample and every ratio-th
    // after, through taps; up gives ratio outputs per input, zero-stuffed through taps. Delay: the filters' own.
    void prepareInteger (int r, bool down, const std::vector<double>& taps)
    {
        ratio = r; decimating = down; fir = taps;
        const int len = (int) fir.size(), keep = down ? len : (len + r - 1) / r;
        fir.resize ((size_t) (down ? len : keep * r), 0.0);
        history.assign ((size_t) (2 * keep), 0.0);
        ringLen = keep;
        reset();
    }
    void reset() { std::fill (history.begin(), history.end(), 0.0); pos = offset; filled = 0; }
    // the delay it adds, in input samples: half, less the start offset (0 ... 1) that trims it
    double latencyIn() const { return half - offset; }
    int halfLength() const { return half; }
    void setOffset (double o) { offset = std::clamp (o, 0.0, 0.999); pos = offset; }

    // converts n input samples, appending the output to out
    void process (const float* in, int n, std::vector<float>& out)
    {
        if (ratio > 0) { processInteger (in, n, out); return; }
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
    void processInteger (const float* in, int n, std::vector<float>& out)
    {
        const int L = ringLen, r = ratio;
        for (int i = 0; i < n; ++i)
        {
            history[(size_t) (filled % L)] = in[i];
            history[(size_t) (filled % L + L)] = in[i];
            const double* x = history.data() + (size_t) (filled % L) + 1;    // oldest .. newest (x[L - 1])
            if (decimating)
            {
                if (filled % r == 0)
                {
                    double acc = 0;
                    for (int j = 0; j < L; ++j) acc += fir[(size_t) j] * x[L - 1 - j];
                    out.push_back ((float) acc);
                }
            }
            else
                for (int m = 0; m < r; ++m)
                {
                    double acc = 0;
                    for (int k = 0; k < L; ++k) acc += fir[(size_t) (m + r * k)] * x[L - 1 - k];
                    out.push_back ((float) acc);
                }
            ++filled;
        }
    }

    std::vector<double> table, history, fir;
    int ratio = 0, ringLen = 0;
    bool decimating = false;
    double step = 1.0, pos = 0.0, offset = 0.0;
    int half = 16;
    long long filled = 0;
};
}
