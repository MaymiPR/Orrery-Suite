#pragma once
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

// moves linearly to each new target over a fixed number of samples
struct Ramp
{
    double value = 0, target = 0, step = 0;
    int left = 0, length = 1;
    void setLength (int samples) { length = samples < 1 ? 1 : samples; }
    void jump (double v) { value = target = v; left = 0; }
    void set (double t)
    {
        if (t == target) return;
        target = t; left = length; step = (target - value) / length;
    }
    double next()
    {
        if (left > 0) { value += step; if (--left == 0) value = target; }
        return value;
    }
    bool moving() const { return left > 0; }
};
}
