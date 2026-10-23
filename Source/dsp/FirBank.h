#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include "BandFilters.h"
#include "Filters.h"
#include "BandFiltersRates.h"
#include "NlBandFilters.h"

// The character section's crossover: three FIR band filters (identified from the reference at 48 kHz, see
// nulltest/fit_bands.py) and the three signals the bands' nonlinear stages hear (the same bands with steeper edges,
// NlBandFilters.h), redesigned for other rates by frequency sampling with the same time delay. Zero-latency
// convolution: the first `block` taps directly, the rest by uniformly partitioned FFT convolution, one input
// spectrum shared by the six filters. Stereo.
namespace ap::dsp
{
class FirBank
{
public:
    static constexpr int kFilters = 6;       // low, mid, high, then what their nonlinear stages hear
    // exactRates: at 44.1 / 88.2 / 96 / 192 kHz use the reference's own filters for that rate (BandFiltersRates.h;
    // only when the delay is the plain core delay, i.e. without oversampling)
    void prepare (double rate, double delaySamples, bool exactRates = false)
    {
        design (rate, delaySamples, exactRates);
        int n = 0;
        for (auto& h : ir) n = std::max (n, (int) h.size());
        block = n < 1000 ? 64 : 128;                 // direct head vs FFT tail: about the cheapest split for these lengths
        fftSize = 2 * block;
        parts = std::max (1, (n + block - 1) / block);
        // tail partitions (p >= 1) in the frequency domain
        for (int b = 0; b < kFilters; ++b)
        {
            const int nb = (int) ir[b].size();
            spec[b].assign ((size_t) parts * fftSize, {});
            for (int p = 1; p < parts; ++p)
            {
                std::vector<std::complex<double>> buf ((size_t) fftSize);
                for (int i = 0; i < block && p * block + i < nb; ++i) buf[(size_t) i] = ir[b][(size_t) (p * block + i)];
                fft (buf, false);
                std::copy (buf.begin(), buf.end(), spec[b].begin() + (size_t) p * fftSize);
            }
        }
        for (auto& c : ch)
        {
            c.hist.assign ((size_t) block, 0.0);
            c.fdl.assign ((size_t) parts * fftSize, {});
            c.prevBlock.assign ((size_t) block, 0.0);
            c.curBlock.assign ((size_t) block, 0.0);
            for (auto& t : c.tail) t.assign ((size_t) block, 0.0);
        }
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.hist.begin(), c.hist.end(), 0.0);
            std::fill (c.fdl.begin(), c.fdl.end(), std::complex<double> {});
            std::fill (c.prevBlock.begin(), c.prevBlock.end(), 0.0);
            std::fill (c.curBlock.begin(), c.curBlock.end(), 0.0);
            for (auto& t : c.tail) std::fill (t.begin(), t.end(), 0.0);
            c.pos = 0; c.histPos = 0; c.fdlHead = 0;
        }
    }

    // one sample of one channel in; the six filter outputs out (low, mid, high, then their nonlinear stages' inputs)
    inline void process (int channel, double x, double* out)
    {
        auto& c = ch[channel];
        c.hist[(size_t) c.histPos] = x;
        c.curBlock[(size_t) c.pos] = x;
        for (int b = 0; b < kFilters; ++b)
        {
            const double* h = ir[b].data();
            const int nh = std::min (block, (int) ir[b].size());
            double acc = c.tail[b][(size_t) c.pos];
            int k = c.histPos;
            for (int i = 0; i < nh; ++i)
            {
                acc += h[i] * c.hist[(size_t) k];
                k = k == 0 ? block - 1 : k - 1;
            }
            out[b] = acc;
        }
        c.histPos = c.histPos + 1 == block ? 0 : c.histPos + 1;
        if (++c.pos == block)
        {
            c.pos = 0;
            blockDone (c);
        }
    }

    const std::vector<double>& response (int band) const { return ir[band]; }

private:
    struct Channel
    {
        std::vector<double> hist, prevBlock, curBlock, tail[kFilters];
        std::vector<std::complex<double>> fdl;      // spectra of the last `parts` input windows (ring)
        int pos = 0, histPos = 0, fdlHead = 0;
    };

    // a full input block arrived: its window spectrum enters the delay line; the tail of the next block is computed
    void blockDone (Channel& c)
    {
        std::vector<std::complex<double>>& w = scratch;
        w.assign ((size_t) fftSize, {});
        for (int i = 0; i < block; ++i) { w[(size_t) i] = c.prevBlock[(size_t) i]; w[(size_t) (block + i)] = c.curBlock[(size_t) i]; }
        fft (w, false);
        c.fdlHead = c.fdlHead == 0 ? parts - 1 : c.fdlHead - 1;
        std::copy (w.begin(), w.end(), c.fdl.begin() + (size_t) c.fdlHead * fftSize);
        std::swap (c.prevBlock, c.curBlock);
        // next block's tail: sum over p >= 1 of X[j + 1 - p] H[p], X[j] being the window just added (p = 1)
        for (int b = 0; b < kFilters; ++b)
        {
            acc.assign ((size_t) fftSize, {});
            for (int p = 1; p < parts; ++p)
            {
                const int slot = (c.fdlHead + p - 1) % parts;
                const auto* X = c.fdl.data() + (size_t) slot * fftSize;
                const auto* Hp = spec[b].data() + (size_t) p * fftSize;
                for (int i = 0; i < fftSize; ++i) acc[(size_t) i] += X[i] * Hp[i];
            }
            fft (acc, true);
            for (int i = 0; i < block; ++i) c.tail[b][(size_t) i] = acc[(size_t) (block + i)].real();
        }
    }

    // iterative radix-2 FFT (size a power of two); inverse includes the 1 / N
    static void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2 * kPi / (double) len * (inverse ? 1 : -1);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const auto u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v; a[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
        if (inverse) for (auto& x : a) x /= (double) n;
    }

    // the filters at `rate`: the 48 kHz responses, delay moved to `delaySamples`. Above 23 kHz (higher rates) all
    // bands are zero: the reference's section keeps its ~22.5 kHz low-pass with oversampling (-45 dB at 23 kHz at 2x)
    void design (double rate, double delaySamples, bool exactRates)
    {
        if (exactRates)
            for (const auto& set : bandFilterSets)
                if (std::abs (rate - set.rate) < 1.0)
                {
                    for (int b = 0; b < 3; ++b)
                    {
                        ir[b].assign (set.ir[b], set.ir[b] + set.taps);
                        ir[b + 3].assign (set.nl[b], set.nl[b] + set.nlTaps);
                    }
                    return;
                }
        const double d48 = 151.27917;
        auto source = [] (int b) { return b < 3 ? bandIr48k[b] : nlBandIr48k[b - 3]; };
        auto taps = [] (int b) { return b < 3 ? bandTaps48k : nlBandTaps48k; };
        if (std::abs (rate - 48000.0) < 1.0 && std::abs (delaySamples - d48) < 1.0e-6)
        {
            for (int b = 0; b < kFilters; ++b) ir[b].assign (source (b), source (b) + taps (b));
            return;
        }
        const double shift = delaySamples / rate - d48 / 48000.0;      // seconds
        for (int b = 0; b < kFilters; ++b)
        {
            const double* h48 = source (b);
            const int n48 = taps (b), n = (int) std::ceil (n48 * rate / 48000.0);
            int m = 1; while (m < 4 * n) m <<= 1;
            std::vector<std::complex<double>> spec ((size_t) m);
            for (int k = 0; k <= m / 2; ++k)
            {
                const double f = (double) k * rate / m;
                std::complex<double> v;
                if (f <= 23000.0)
                {
                    // DTFT of the 48 kHz response (exact, it is finite)
                    const double w = -2 * kPi * f / 48000.0;
                    const std::complex<double> step (std::cos (w), std::sin (w));
                    std::complex<double> e (1);
                    for (int i = 0; i < n48; ++i) { v += h48[i] * e; e *= step; }
                }
                else
                    v = {};
                v *= std::polar (1.0, -2 * kPi * f * shift);
                spec[(size_t) k] = v;
                if (k > 0 && k < m / 2) spec[(size_t) (m - k)] = std::conj (v);
            }
            fft (spec, true);
            ir[b].resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double fade = i > n - n / 8 ? 0.5 * (1 + std::cos (kPi * (i - (n - n / 8)) / (n / 8))) : 1.0;
                ir[b][(size_t) i] = spec[(size_t) i].real() * fade;
            }
        }
    }

    std::vector<double> ir[kFilters];
    std::vector<std::complex<double>> spec[kFilters], scratch, acc;
    Channel ch[2];
    int block = 64, fftSize = 128, parts = 1;
};
}
