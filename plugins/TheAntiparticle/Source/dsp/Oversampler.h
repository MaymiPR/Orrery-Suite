#pragma once
#include <algorithm>
#include <vector>
#include "OsFilters.h"

// 2x / 4x / 8x oversampling with linear-phase half-band FIR stages sized like the reference's (its path gets 22.49 /
// 27.71 / 29.66 samples longer at 2x / 4x / 8x; ours 22 / 27 / 28.75, the rest is in the fixed delay). Stage 1 is
// designed to give the reference's top-octave roll-off (nulltest/fit_oversampling.py -> OsFilters.h). Stereo, double.
namespace ap::dsp
{
class HalfbandStage
{
public:
    void prepare (const double* taps, int n)
    {
        h.assign (taps, taps + n);
        len = n;
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
    void prepare (int factor, int maxBlock)
    {
        stages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
        const double* taps[3] { stages == 1 ? osStage1x2 : osStage1x4, osStage2, osStage3 };
        const int lens[3] { stages == 1 ? osStage1x2Taps : osStage1x4Taps, osStage2Taps, osStage3Taps };
        for (int s = 0; s < stages; ++s) st[s].prepare (taps[s], lens[s]);
        for (int s = 0; s <= stages; ++s)
            for (auto& b : buf[s]) b.assign ((size_t) (maxBlock << s), 0.0);
    }
    void reset() { for (auto& s : st) s.reset(); }
    int factor() const { return 1 << stages; }
    // latency in base-rate samples (up + down)
    double latency() const
    {
        double l = 0.0;
        for (int s = 0; s < stages; ++s) l += (double) st[s].latencyAtOwnRate() / (double) (2 << s);
        return l;
    }
    // base-rate block in -> pointers to the oversampled block (n * factor samples per channel)
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
    // the oversampled block (as returned by up, processed in place) -> base rate
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
    HalfbandStage st[3];
    std::vector<double> buf[4][2];
    int stages = 0;
};
}
