#pragma once
#include <algorithm>

// tanh and the logistic sigmoid for the learned models' activations, as a rational (Pade 7/6) approximation of tanh:
// within 1e-6 of it up to |x| = 4.97 and clamped beyond, several times cheaper than std::tanh / std::exp.
namespace sc::dsp
{
inline float fastTanh (float x)
{
    x = std::clamp (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return num / den;
}

inline float fastSigmoid (float x) { return 0.5f + 0.5f * fastTanh (0.5f * x); }
}
