#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>
#include "BandFilters.h"
#include "Filters.h"
#include "BandFiltersRates.h"
#include "NlBandFilters.h"
#if defined (_M_X64) || defined (_M_AMD64) || defined (__x86_64__)
 #include <immintrin.h>
 #if defined (_MSC_VER) && ! defined (__clang__)
  #include <intrin.h>
  #define AP_AVX2_FN
  #define AP_AVX512_FN
 #else
  #include <cpuid.h>
  #define AP_AVX2_FN __attribute__ ((target ("avx2")))
  #define AP_AVX512_FN __attribute__ ((target ("avx512f")))
 #endif
 #define AP_WIDE 1
#endif

// The character section's crossover: three FIR band filters (identified from the reference at 48 kHz, see
// nulltest/fit_bands.py) and the three signals the bands' nonlinear stages hear (the same bands with steeper edges,
// NlBandFilters.h), redesigned for other rates by frequency sampling with the same time delay. Zero-latency
// convolution: the first `block` taps directly, the rest by uniformly partitioned FFT convolution, one input
// spectrum shared by the six filters. Stereo. The spectra are kept as separate real and imaginary arrays (so the
// partitions' products vectorise), the FFTs use tables of their twiddle factors, and two filters share each inverse
// FFT (their outputs are real: one is the real part, the other the imaginary part).
namespace ap::dsp
{
#if AP_WIDE
// what the processor (and the system) supports: 0 neither, 2 AVX2, 3 AVX-512
inline int wideLevel()
{
    static const int level = []
    {
        auto cpuid = [] (int leaf, int sub, unsigned (&r)[4])
        {
#if defined (_MSC_VER) && ! defined (__clang__)
            int v[4]; __cpuidex (v, leaf, sub); for (int k = 0; k < 4; ++k) r[k] = (unsigned) v[k];
#else
            __cpuid_count (leaf, sub, r[0], r[1], r[2], r[3]);
#endif
        };
        unsigned r[4];
        cpuid (1, 0, r);
        const bool osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1;
        if (! osxsave || ! avx) return 0;
#if defined (_MSC_VER) && ! defined (__clang__)
        const unsigned long long xcr0 = _xgetbv (0);
#else
        unsigned lo, hi; __asm__ ("xgetbv" : "=a" (lo), "=d" (hi) : "c" (0)); const unsigned long long xcr0 = ((unsigned long long) hi << 32) | lo;
#endif
        if ((xcr0 & 6) != 6) return 0;
        cpuid (7, 0, r);
        const bool avx2 = (r[1] >> 5) & 1, avx512 = (r[1] >> 16) & 1;
        if (avx512 && (xcr0 & 0xe6) == 0xe6) return 3;
        return avx2 ? 2 : 0;
    }();
    return level;
}

// the direct taps of the six filters over a stretch, 8 or 4 samples at a time, the six sums side by side (each the
// same sum in the same order as one sample at a time: no fused multiply-adds); returns how many samples it did
AP_AVX512_FN inline int firHead512 (const double* const* h, int nh, const double* cur, int m, const double* const* tail, double* const* out)
{
    int j = 0;
    for (; j + 8 <= m; j += 8)
    {
        __m512d a0 = _mm512_loadu_pd (tail[0] + j), a1 = _mm512_loadu_pd (tail[1] + j), a2 = _mm512_loadu_pd (tail[2] + j);
        __m512d a3 = _mm512_loadu_pd (tail[3] + j), a4 = _mm512_loadu_pd (tail[4] + j), a5 = _mm512_loadu_pd (tail[5] + j);
        for (int i = 0; i < nh; ++i)
        {
            const __m512d x = _mm512_loadu_pd (cur + j - i);
            a0 = _mm512_add_pd (a0, _mm512_mul_pd (_mm512_set1_pd (h[0][i]), x));
            a1 = _mm512_add_pd (a1, _mm512_mul_pd (_mm512_set1_pd (h[1][i]), x));
            a2 = _mm512_add_pd (a2, _mm512_mul_pd (_mm512_set1_pd (h[2][i]), x));
            a3 = _mm512_add_pd (a3, _mm512_mul_pd (_mm512_set1_pd (h[3][i]), x));
            a4 = _mm512_add_pd (a4, _mm512_mul_pd (_mm512_set1_pd (h[4][i]), x));
            a5 = _mm512_add_pd (a5, _mm512_mul_pd (_mm512_set1_pd (h[5][i]), x));
        }
        _mm512_storeu_pd (out[0] + j, a0); _mm512_storeu_pd (out[1] + j, a1); _mm512_storeu_pd (out[2] + j, a2);
        _mm512_storeu_pd (out[3] + j, a3); _mm512_storeu_pd (out[4] + j, a4); _mm512_storeu_pd (out[5] + j, a5);
    }
    return j;
}

AP_AVX2_FN inline int firHeadAvx2 (const double* const* h, int nh, const double* cur, int m, const double* const* tail, double* const* out)
{
    int j = 0;
    for (; j + 4 <= m; j += 4)
    {
        __m256d a0 = _mm256_loadu_pd (tail[0] + j), a1 = _mm256_loadu_pd (tail[1] + j), a2 = _mm256_loadu_pd (tail[2] + j);
        __m256d a3 = _mm256_loadu_pd (tail[3] + j), a4 = _mm256_loadu_pd (tail[4] + j), a5 = _mm256_loadu_pd (tail[5] + j);
        for (int i = 0; i < nh; ++i)
        {
            const __m256d x = _mm256_loadu_pd (cur + j - i);
            a0 = _mm256_add_pd (a0, _mm256_mul_pd (_mm256_set1_pd (h[0][i]), x));
            a1 = _mm256_add_pd (a1, _mm256_mul_pd (_mm256_set1_pd (h[1][i]), x));
            a2 = _mm256_add_pd (a2, _mm256_mul_pd (_mm256_set1_pd (h[2][i]), x));
            a3 = _mm256_add_pd (a3, _mm256_mul_pd (_mm256_set1_pd (h[3][i]), x));
            a4 = _mm256_add_pd (a4, _mm256_mul_pd (_mm256_set1_pd (h[4][i]), x));
            a5 = _mm256_add_pd (a5, _mm256_mul_pd (_mm256_set1_pd (h[5][i]), x));
        }
        _mm256_storeu_pd (out[0] + j, a0); _mm256_storeu_pd (out[1] + j, a1); _mm256_storeu_pd (out[2] + j, a2);
        _mm256_storeu_pd (out[3] + j, a3); _mm256_storeu_pd (out[4] + j, a4); _mm256_storeu_pd (out[5] + j, a5);
    }
    _mm256_zeroupper();
    return j;
}
#endif

class FirBank
{
public:
    static constexpr int kFilters = 6;       // low, mid, high, then what their nonlinear stages hear
    // exactRates: at 44.1 / 88.2 / 96 / 192 kHz use the reference's own filters for that rate (BandFiltersRates.h;
    // only when the delay is the plain core delay, i.e. without oversampling). hostRate / hostDelay: with oversampling
    // the filters are the host rate's ones (its own set where there is one, else the 48 kHz one) redrawn at `rate`
    void prepare (double rate, double delaySamples, bool exactRates = false, double hostRate = 48000.0, double hostDelay = 151.27917)
    {
        design (rate, delaySamples, exactRates, hostRate, hostDelay);
        int n = 0;
        for (auto& h : ir) n = std::max (n, (int) h.size());
        block = n < 1000 ? 64 : 128;                 // direct head vs FFT tail: about the cheapest split for these lengths
        fftSize = 2 * block;
        parts = std::max (1, (n + block - 1) / block);
        // tail partitions (p >= 1) in the frequency domain
        makeTables();
        // (the wide kernel wants the same number of direct taps for all six)
        headTaps = std::min (block, (int) ir[0].size());
        headsEqual = true;
        for (int b = 1; b < kFilters; ++b) headsEqual = headsEqual && std::min (block, (int) ir[b].size()) == headTaps;
        for (int b = 0; b < kFilters; ++b)
        {
            const int nb = (int) ir[b].size();
            specRe[b].assign ((size_t) (parts * fftSize), 0.0);
            specIm[b].assign ((size_t) (parts * fftSize), 0.0);
            for (int p = 1; p < parts; ++p)
            {
                double* re = specRe[b].data() + (size_t) (p * fftSize); double* im = specIm[b].data() + (size_t) (p * fftSize);
                for (int i = 0; i < block && p * block + i < nb; ++i) re[i] = ir[b][(size_t) (p * block + i)];
                fftTables (re, im, false);
            }
        }
        accRe.assign ((size_t) (kFilters * fftSize), 0.0); accIm = accRe;
        winRe.assign ((size_t) fftSize, 0.0); winIm = winRe;
        for (auto& c : ch)
        {
            c.fdlRe.assign ((size_t) (parts * fftSize), 0.0);
            c.fdlIm.assign ((size_t) (parts * fftSize), 0.0);
            c.lin.assign ((size_t) (2 * block), 0.0);
            for (auto& t : c.tail) t.assign ((size_t) block, 0.0);
        }
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.fdlRe.begin(), c.fdlRe.end(), 0.0);
            std::fill (c.fdlIm.begin(), c.fdlIm.end(), 0.0);
            std::fill (c.lin.begin(), c.lin.end(), 0.0);
            for (auto& t : c.tail) std::fill (t.begin(), t.end(), 0.0);
            c.pos = 0; c.fdlHead = 0;
        }
    }

    // one sample of one channel in; the six filter outputs out (low, mid, high, then their nonlinear stages' inputs)
    inline void process (int channel, double x, double* out)
    {
        auto& c = ch[channel];
        // (lin: the last block, then this one so far: the last `block` samples always in a row, newest at x)
        c.lin[(size_t) (block + c.pos)] = x;
        const double* newest = c.lin.data() + block + c.pos;
        for (int b = 0; b < kFilters; ++b)
        {
            const double* h = ir[b].data();
            const int nh = std::min (block, (int) ir[b].size());
            double acc = c.tail[b][(size_t) c.pos];
            for (int i = 0; i < nh; ++i) acc += h[i] * newest[-i];
            out[b] = acc;
        }
        if (++c.pos == block)
        {
            c.pos = 0;
            blockDone (c);
        }
    }
    // n samples of one channel in; out[b][j] the six filters' outputs. The same sums in the same order as one sample
    // at a time (process), run across the samples so they do not wait on each other
    void processBlock (int channel, const double* x, int n, double* const* out)
    {
        auto& c = ch[channel];
        for (int done = 0; done < n;)
        {
            const int m = std::min (n - done, block - c.pos);
            double* cur = c.lin.data() + block + c.pos;
            std::copy (x + done, x + done + m, cur);
            int first = 0;          // (the samples the wide kernel did)
#if AP_WIDE
            if (headsEqual && wideLevel() >= 2)
            {
                const double* hs[kFilters]; const double* ts[kFilters]; double* os[kFilters];
                for (int b = 0; b < kFilters; ++b) { hs[b] = ir[b].data(); ts[b] = c.tail[b].data() + c.pos; os[b] = out[b] + done; }
                first = wideLevel() == 3 ? firHead512 (hs, headTaps, cur, m, ts, os) : firHeadAvx2 (hs, headTaps, cur, m, ts, os);
            }
#endif
            for (int b = 0; b < kFilters; ++b)
            {
                const double* h = ir[b].data();
                const int nh = std::min (block, (int) ir[b].size());
                double* o = out[b] + done;
                const double* t = c.tail[b].data() + c.pos;
                for (int j = first; j < m; ++j) o[j] = t[j];
                for (int i = 0; i < nh; ++i)
                {
                    const double hi = h[i];
                    const double* src = cur - i;
                    for (int j = first; j < m; ++j) o[j] += hi * src[j];
                }
            }
            c.pos += m; done += m;
            if (c.pos == block)
            {
                c.pos = 0;
                blockDone (c);
            }
        }
    }

    const std::vector<double>& response (int band) const { return ir[band]; }

private:
    struct Channel
    {
        std::vector<double> lin, tail[kFilters];       // lin: the last block, then the current one
        std::vector<double> fdlRe, fdlIm;           // spectra of the last `parts` input windows (ring)
        int pos = 0, fdlHead = 0;
    };

    // a full input block arrived: its window spectrum enters the delay line; the tail of the next block is computed
    void blockDone (Channel& c)
    {
        double* wr = winRe.data(); double* wi = winIm.data();
        std::copy (c.lin.begin(), c.lin.end(), wr);
        std::fill (wi, wi + fftSize, 0.0);
        fftTables (wr, wi, false);
        c.fdlHead = c.fdlHead == 0 ? parts - 1 : c.fdlHead - 1;
        std::copy (wr, wr + fftSize, c.fdlRe.begin() + (std::ptrdiff_t) (c.fdlHead * fftSize));
        std::copy (wi, wi + fftSize, c.fdlIm.begin() + (std::ptrdiff_t) (c.fdlHead * fftSize));
        std::copy (c.lin.begin() + block, c.lin.end(), c.lin.begin());
        // next block's tail: sum over p >= 1 of X[j + 1 - p] H[p], X[j] being the window just added (p = 1)
        for (int b = 0; b < kFilters; ++b)
        {
            double* ar = accRe.data() + (size_t) (b * fftSize); double* ai = accIm.data() + (size_t) (b * fftSize);
            std::fill (ar, ar + fftSize, 0.0); std::fill (ai, ai + fftSize, 0.0);
            for (int p = 1; p < parts; ++p)
            {
                const int slot = (c.fdlHead + p - 1) % parts;
                const double* xr = c.fdlRe.data() + (size_t) (slot * fftSize); const double* xi = c.fdlIm.data() + (size_t) (slot * fftSize);
                const double* hr = specRe[b].data() + (size_t) (p * fftSize); const double* hi = specIm[b].data() + (size_t) (p * fftSize);
                for (int i = 0; i < fftSize; ++i)
                {
                    ar[i] += xr[i] * hr[i] - xi[i] * hi[i];
                    ai[i] += xr[i] * hi[i] + xi[i] * hr[i];
                }
            }
        }
        // two filters per inverse FFT: A + i B, whose inverse is a + i b (a and b real)
        for (int b = 0; b < kFilters; b += 2)
        {
            const double* ar = accRe.data() + (size_t) (b * fftSize); const double* ai = accIm.data() + (size_t) (b * fftSize);
            const double* br = ar + fftSize; const double* bi = ai + fftSize;
            for (int i = 0; i < fftSize; ++i) { wr[i] = ar[i] - bi[i]; wi[i] = ai[i] + br[i]; }
            fftTables (wr, wi, true);
            for (int i = 0; i < block; ++i) { c.tail[b][(size_t) i] = wr[block + i]; c.tail[b + 1][(size_t) i] = wi[block + i]; }
        }
    }
    // the FFT's tables for fftSize: the bit-reversed order and the twiddle factors e^(-2 pi i k / N)
    void makeTables()
    {
        const int n = fftSize;
        bitrev.assign ((size_t) n, 0);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int k = 0; k < bits; ++k) if (i & (1 << k)) r |= 1 << (bits - 1 - k); bitrev[(size_t) i] = r; }
        twCos.assign ((size_t) (n / 2), 0.0); twSin = twCos;
        for (int k = 0; k < n / 2; ++k) { twCos[(size_t) k] = std::cos (2 * kPi * k / n); twSin[(size_t) k] = std::sin (2 * kPi * k / n); }
    }
    // radix-2 FFT of fftSize points in place on separate real and imaginary arrays; inverse includes the 1 / N
    void fftTables (double* re, double* im, bool inverse) const
    {
        const int n = fftSize;
        for (int i = 0; i < n; ++i)
        {
            const int j = bitrev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        const double sign = inverse ? 1.0 : -1.0;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const double c = twCos[(size_t) (k * step)], sn = sign * twSin[(size_t) (k * step)];
                    double* ur = re + i + k; double* ui = im + i + k; double* vr = ur + half; double* vi = ui + half;
                    const double tr = *vr * c - *vi * sn, ti = *vr * sn + *vi * c;
                    *vr = *ur - tr; *vi = *ui - ti;
                    *ur += tr; *ui += ti;
                }
        }
        if (inverse) { const double g = 1.0 / n; for (int i = 0; i < n; ++i) { re[i] *= g; im[i] *= g; } }
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

    // the filters at `rate`: the host rate's responses, delay moved to `delaySamples`. Above 0.479 of the host rate
    // all bands are zero: the reference's section keeps its low-pass with oversampling (48 kHz: ~22.5 kHz, -45 dB at
    // 23 kHz at 2x; at 96 kHz its own 96 kHz filters, nulltest/null_oversampling.py)
    void design (double rate, double delaySamples, bool exactRates, double hostRate, double hostDelay)
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
        double d48 = 151.27917, srcRate = 48000.0;
        const BandFilterSet* src = nullptr;
        for (const auto& set : bandFilterSets)
            if (std::abs (hostRate - set.rate) < 1.0) { src = &set; srcRate = set.rate; d48 = hostDelay; }
        auto source = [src] (int b) { return src != nullptr ? (b < 3 ? src->ir[b] : src->nl[b - 3]) : b < 3 ? bandIr48k[b] : nlBandIr48k[b - 3]; };
        auto taps = [src] (int b) { return src != nullptr ? (b < 3 ? src->taps : src->nlTaps) : b < 3 ? bandTaps48k : nlBandTaps48k; };
        if (src == nullptr && std::abs (rate - 48000.0) < 1.0 && std::abs (delaySamples - d48) < 1.0e-6)
        {
            for (int b = 0; b < kFilters; ++b) ir[b].assign (source (b), source (b) + taps (b));
            return;
        }
        const double shift = delaySamples / rate - d48 / srcRate;      // seconds
        for (int b = 0; b < kFilters; ++b)
        {
            const double* h48 = source (b);
            const int n48 = taps (b), n = (int) std::ceil (n48 * rate / srcRate);
            int m = 1; while (m < 4 * n) m <<= 1;
            std::vector<std::complex<double>> spec ((size_t) m);
            for (int k = 0; k <= m / 2; ++k)
            {
                const double f = (double) k * rate / m;
                std::complex<double> v;
                if (f <= 23000.0 * srcRate / 48000.0)
                {
                    // DTFT of the host rate's response (exact, it is finite)
                    const double w = -2 * kPi * f / srcRate;
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
    std::vector<double> specRe[kFilters], specIm[kFilters], accRe, accIm, winRe, winIm, twCos, twSin;
    std::vector<int> bitrev;
    int headTaps = 0;
    bool headsEqual = false;
    Channel ch[2];
    int block = 64, fftSize = 128, parts = 1;
};
}
