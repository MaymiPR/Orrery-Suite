#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Filters.h"

// Streaming sample-rate conversion by a fixed ratio (Kaiser-windowed sinc, a table of phases interpolated linearly),
// for running the DSP at 48 kHz whatever the host's rate, as the reference does (MEASUREMENTS.md 2: its own
// resampler is linear-phase, 31 samples of latency at 44.1 kHz). Above 48 kHz it can instead run the reference's own
// converters, identified (ConverterFits.h, MEASUREMENTS.md 14): rational polyphase filters, not linear-phase.
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
        rational = false;
        half = std::max (2, halfLength);
        step = inRate / outRate;
        const double fc = cutoff * std::min (1.0, outRate / inRate);       // in input-rate Nyquist units
        table.assign ((size_t) ((phases + 1) * 2 * half), 0.0);
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
                table[(size_t) (p * 2 * half + k)] = fc * sinc * win;
            }
        }
        history.assign ((size_t) (4 * half), 0.0);
        reset();
    }
    // a rational ratio, the lower rate's samples p / q of the higher rate's apart (96 kHz: 2 / 1, 88.2 kHz: 147 / 80),
    // with taps on the grid of 1 / q higher-rate samples. Down: v[k] = sum over j of x[j] taps[p k - q j] (the input's
    // first sample is the first output's newest); up: y[n] = sum over k of v[k] taps[q n - p k]. Both causal; the
    // delay is the taps' own.
    void prepareRational (int p, int q, bool down, const std::vector<double>& taps)
    {
        rows = down ? q : p; advance = down ? p : q;
        ringLen = ((int) taps.size() + rows - 1) / rows;
        poly.assign ((size_t) rows * (size_t) ringLen, 0.0);         // row r: taps[r], taps[r + rows], ...
        for (size_t i = 0; i < taps.size(); ++i) poly[(i % (size_t) rows) * (size_t) ringLen + i / (size_t) rows] = taps[i];
        history.assign ((size_t) (2 * ringLen), 0.0);
        rational = true;
        reset();
    }
    void reset() { std::fill (history.begin(), history.end(), 0.0); pos = offset; filled = 0; acc = 0; }
    // the delay it adds, in input samples: half, less the start offset (0 ... 1) that trims it
    double latencyIn() const { return half - offset; }
    int halfLength() const { return half; }
    void setOffset (double o) { offset = std::clamp (o, 0.0, 0.999); pos = offset; }

    // converts n input samples, appending the output to out
    void process (const float* in, int n, std::vector<float>& out)
    {
        if (rational) { processRational (in, n, out); return; }
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
                const double* a = table.data() + (size_t) (p * h);
                const double* b = a + h;
                const double* x = history.data() + (size_t) (filled % h);  // oldest .. newest
                double sum = 0;
                for (int k = 0; k < h; ++k) sum += x[h - 1 - k] * (a[k] + u * (b[k] - a[k]));
                out.push_back ((float) sum);
                pos += step;
            }
            pos -= 1.0;
        }
    }

private:
    void processRational (const float* in, int n, std::vector<float>& out)
    {
        const int L = ringLen;
        for (int i = 0; i < n; ++i)
        {
            const int w = (int) (filled % L);
            history[(size_t) w] = in[i];
            history[(size_t) (w + L)] = in[i];
            const double* x = history.data() + w + L;                 // the newest; x[-i] the i-th before it
            // acc: the next output's instant less this input's, on the taps' grid (q per higher-rate sample)
            for (; acc < rows; acc += advance)
            {
                const double* h = poly.data() + (size_t) acc * (size_t) L;
                double s = 0;
                for (int k = 0; k < L; ++k) s += h[k] * x[-k];
                out.push_back ((float) s);
            }
            acc -= rows;
            ++filled;
        }
    }

    std::vector<double> table, history, poly;
    int ringLen = 0, rows = 1, advance = 1, acc = 0;
    bool rational = false;
    double step = 1.0, pos = 0.0, offset = 0.0;
    int half = 16;
    long long filled = 0;
};
}
