#pragma once
#include <algorithm>
#include <cmath>

// Small building blocks of the DSP: a biquad (transposed direct form II, double precision) and a linear ramp.
namespace sc::dsp
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (const double c[5]) { b0 = c[0]; b1 = c[1]; b2 = c[2]; a1 = c[3]; a2 = c[4]; }
    void reset() { z1 = z2 = 0; }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// moves linearly to each new target over a fixed number of samples. With setChunked, as Cradle's framework smooths its
// parameters (measured on both references, The Antiparticle's MEASUREMENTS.md 5 and Scorpius's 12): a new target first
// moves along a ramp of its own that is read once per control chunk (chunks counted from the block's start), and the
// value glides linearly to each reading over `length` samples
struct Ramp
{
    double value = 0, target = 0, step = 0;
    int left = 0, length = 1;
    void setLength (int samples) { length = samples < 1 ? 1 : samples; }
    void setChunked (int chunkSamples, int paramSamples) { chunk = chunkSamples; paramLen = paramSamples < 1 ? 1 : paramSamples; }
    void jump (double v) { value = target = goal = pValue = v; left = pLeft = 0; }
    void set (double t)
    {
        if (chunk > 0)
        {
            if (t != goal) { goal = t; pLeft = paramLen; pStep = (goal - pValue) / paramLen; }
            return;
        }
        glideTo (t);
    }
    void beginBlock (int n) { pos = 0; blockLen = n; }
    double next()
    {
        if (chunk > 0 && pos % chunk == 0)
        {
            const int len = blockLen > pos ? std::min (chunk, blockLen - pos) : chunk;
            if (pLeft > 0) { if (len >= pLeft) { pValue = goal; pLeft = 0; } else { pValue += pStep * len; pLeft -= len; } }
            glideTo (pValue);
        }
        ++pos;
        if (left > 0) { value += step; if (--left == 0) value = target; }
        return value;
    }
    bool moving() const { return left > 0 || pLeft > 0; }

private:
    void glideTo (double t)
    {
        if (t == target) return;
        target = t; left = length; step = (target - value) / length;
    }
    int chunk = 0, paramLen = 1, pos = 0, blockLen = 0, pLeft = 0;
    double goal = 0, pValue = 0, pStep = 0;
};
}
