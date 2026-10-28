#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// 2x / 4x / 8x oversampling with linear-phase FIR stages designed when prepared, as the reference's are: stage k runs
// at 2^(k+1) times the host rate and is a Kaiser-windowed sinc cut at a quarter of that rate (so a half-band filter),
// its length and window set by the textbook Kaiser formulas from a transition band between a passband edge in Hz and
// its mirror image, and a stopband depth; unit gain at DC. The edge and the depth are fitted from the reference's
// measured paths (nulltest/fit_oversampling.py: 16.9 ... 17.0 kHz and 100 dB match it to 0.000 dB at 44.1 / 48 /
// 88.2 / 96 kHz and every ratio), so the filters, and the latency, change with the host rate like the reference's.
// 8x processes at 4x (the reference's 8x output equals its 4x output, ~2 samples later, to -82 dB below 16 kHz; folding
// its third stage into a filter at 4x nulled worse on music, -45 vs -62 dB, as it filters the character's harmonics
// twice). Stereo, double.
namespace ap::dsp
{
inline constexpr double kOsEdgeHz = 17000.0, kOsDepthDb = 100.0;

// stage k's taps for host rate fs (odd length, symmetric)
inline std::vector<double> designOsStage (int k, double fs)
{
    const double rate = fs * (double) (2 << k);
    const double width = (rate - 4.0 * kOsEdgeHz) / (2.0 * rate);           // (stop - pass) / rate, stop = rate/2 - pass
    int order = (int) std::ceil ((kOsDepthDb - 7.95) / (2.285 * width * 2.0 * 3.14159265358979323846));
    order += order % 2;
    const double beta = 0.1102 * (kOsDepthDb - 8.7);
    auto i0 = [] (double x)
    {
        double s = 1.0, term = 1.0;
        for (int m = 1; m < 64 && term > 1.0e-20 * s; ++m) { term *= (x / (2.0 * m)) * (x / (2.0 * m)); s += term; }
        return s;
    };
    std::vector<double> h ((size_t) order + 1);
    double sum = 0.0;
    for (int n = 0; n <= order; ++n)
    {
        const double t = n - order / 2, r = 2.0 * n / order - 1.0;
        const double sinc = t == 0 ? 0.5 : std::sin (0.5 * 3.14159265358979323846 * t) / (3.14159265358979323846 * t);
        h[(size_t) n] = sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
        sum += h[(size_t) n];
    }
    for (auto& v : h) v /= sum;
    return h;
}

class HalfbandStage
{
public:
    void prepare (const std::vector<double>& taps)
    {
        h = taps;
        len = (int) h.size();
        for (auto& c : ch) { c.up.assign ((size_t) len, 0.0); c.down.assign ((size_t) len, 0.0); c.upPos = c.downPos = 0; }
    }
    void reset() { for (auto& c : ch) { std::fill (c.up.begin(), c.up.end(), 0.0); std::fill (c.down.begin(), c.down.end(), 0.0); c.upPos = c.downPos = 0; } }
    int latencyAtOwnRate() const { return len - 1; }          // up + down, at this stage's (higher) rate

    // one input sample -> two output samples (zero-stuffing + filter, x2 gain)
    inline void up (int c, double x, double& y0, double& y1)
    {
        y0 = push (ch[c].up, ch[c].upPos, x) * 2.0;
        y1 = push (ch[c].up, ch[c].upPos, 0.0) * 2.0;
    }
    // two input samples -> one output sample (filter, keep the even one: the delay is then exactly (len - 1) / 2)
    inline double down (int c, double x0, double x1)
    {
        const double y = push (ch[c].down, ch[c].downPos, x0);
        auto& d = ch[c];
        d.down[(size_t) d.downPos] = x1;
        d.downPos = d.downPos + 1 == len ? 0 : d.downPos + 1;
        return y;
    }

private:
    struct Channel { std::vector<double> up, down; int upPos = 0, downPos = 0; };
    // writes x into the ring and returns the filter output for it (direct form; the taps are short)
    inline double push (std::vector<double>& ring, int& pos, double x)
    {
        ring[(size_t) pos] = x;
        double acc = 0.0;
        int k = pos;
        for (int i = 0; i < len; ++i)
        {
            const double v = ring[(size_t) k];
            if (v != 0.0) acc += h[(size_t) i] * v;
            k = k == 0 ? len - 1 : k - 1;
        }
        pos = pos + 1 == len ? 0 : pos + 1;
        return acc;
    }
    std::vector<double> h;
    int len = 1;
    Channel ch[2];
};

class Oversampler
{
public:
    // factor as selected (1 / 2 / 4 / 8); 8 processes at 4 (factor())
    void prepare (double sampleRate, int selected, int maxBlock)
    {
        stages = selected >= 4 ? 2 : selected >= 2 ? 1 : 0;
        for (int s = 0; s < stages; ++s) st[s].prepare (designOsStage (s, sampleRate));
        for (int s = 0; s <= stages; ++s)
            for (auto& b : buf[s]) b.assign ((size_t) (maxBlock << s), 0.0);
    }
    void reset() { for (auto& s : st) s.reset(); }
    int factor() const { return 1 << stages; }
    // latency in host-rate samples (up + down)
    double latency() const
    {
        double l = 0.0;
        for (int s = 0; s < stages; ++s) l += (double) st[s].latencyAtOwnRate() / (double) (2 << s);
        return l;
    }
    // host-rate block in -> pointers to the oversampled block (n * factor samples per channel)
    void up (const double* l, const double* r, int n, double*& ol, double*& orr)
    {
        std::copy (l, l + n, buf[0][0].begin());
        std::copy (r, r + n, buf[0][1].begin());
        int m = n;
        for (int s = 0; s < stages; ++s, m *= 2)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < m; ++i)
                    st[s].up (c, buf[s][c][(size_t) i], buf[s + 1][c][(size_t) (2 * i)], buf[s + 1][c][(size_t) (2 * i + 1)]);
        ol = buf[stages][0].data(); orr = buf[stages][1].data();
    }
    // the oversampled block (as returned by up, processed in place) -> host rate
    void down (double* l, double* r, int n)
    {
        int m = n << stages;
        for (int s = stages - 1; s >= 0; --s, m /= 2)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < m / 2; ++i)
                    buf[s][c][(size_t) i] = st[s].down (c, buf[s + 1][c][(size_t) (2 * i)], buf[s + 1][c][(size_t) (2 * i + 1)]);
        std::copy (buf[0][0].begin(), buf[0][0].begin() + n, l);
        std::copy (buf[0][1].begin(), buf[0][1].begin() + n, r);
    }

private:
    HalfbandStage st[2];
    std::vector<double> buf[3][2];
    int stages = 0;
};
}
