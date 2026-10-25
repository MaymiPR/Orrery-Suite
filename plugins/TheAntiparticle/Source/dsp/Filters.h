#pragma once
#include <cmath>
#include <vector>

// Filters of the DSP: RBJ-cookbook biquads (the reference's EQ fits them exactly), the Linkwitz-Riley crossover of the
// character section and the core path's fractional delay.
namespace ap::dsp
{
inline constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1[2] {}, s2[2] {}, s3[2] {}, s4[2] {};   // state per channel
    int form = 0;                // 0 transposed direct form II, 1 direct form I, 2 direct form II, 3 trapezoidal
                                 // state-variable filter (all the same response; they differ while the coefficients move)
    double sa1 = 0, sa2 = 0, sa3 = 0, m0 = 1, m1 = 0, m2 = 0;    // form 3

    void reset() { s1[0] = s1[1] = s2[0] = s2[1] = s3[0] = s3[1] = s4[0] = s4[1] = 0.0; }
    inline double process (double x, int ch)
    {
        if (form == 3)
        {
            const double v3 = x - s2[ch];
            const double v1 = sa1 * s1[ch] + sa2 * v3, v2 = s2[ch] + sa2 * s1[ch] + sa3 * v3;
            s1[ch] = 2 * v1 - s1[ch]; s2[ch] = 2 * v2 - s2[ch];
            return m0 * x + m1 * v1 + m2 * v2;
        }
        if (form == 1)
        {
            const double y = b0 * x + b1 * s1[ch] + b2 * s2[ch] - a1 * s3[ch] - a2 * s4[ch];
            s2[ch] = s1[ch]; s1[ch] = x; s4[ch] = s3[ch]; s3[ch] = y;
            return y;
        }
        if (form == 2)
        {
            const double w = x - a1 * s1[ch] - a2 * s2[ch];
            const double y = b0 * w + b1 * s1[ch] + b2 * s2[ch];
            s2[ch] = s1[ch]; s1[ch] = w;
            return y;
        }
        const double y = b0 * x + s1[ch];
        s1[ch] = b1 * x - a1 * y + s2[ch];
        s2[ch] = b2 * x - a2 * y;
        return y;
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    bool isIdentity() const { return b0 == 1 && b1 == 0 && b2 == 0 && a1 == 0 && a2 == 0; }

    enum class Type { lowShelf, peak, highShelf, lowPass, highPass, allPass };
    void design (Type t, double f0, double q, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        if (form == 3 && (t == Type::peak || t == Type::lowShelf || t == Type::highShelf))
        {
            double g = std::tan (kPi * f0 / fs), k = 1.0 / q;
            if (t == Type::peak) { k = 1.0 / (q * A); m0 = 1; m1 = k * (A * A - 1); m2 = 0; }
            else if (t == Type::lowShelf) { g /= std::sqrt (A); m0 = 1; m1 = k * (A - 1); m2 = A * A - 1; }
            else { g *= std::sqrt (A); m0 = A * A; m1 = k * (1 - A) * A; m2 = 1 - A * A; }
            sa1 = 1 / (1 + g * (g + k)); sa2 = g * sa1; sa3 = g * sa2;
            return;
        }
        switch (t)
        {
            case Type::peak: set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); break;
            case Type::lowShelf:
            {
                const double sq = 2 * std::sqrt (A) * al;
                set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                     (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
                break;
            }
            case Type::highShelf:
            {
                const double sq = 2 * std::sqrt (A) * al;
                set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                     (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
                break;
            }
            case Type::lowPass: set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); break;
            case Type::highPass: set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); break;
            case Type::allPass: set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al); break;
        }
    }
};

// 3-band Linkwitz-Riley 4th-order split: low = LP(fc1) * AP(fc2), mid = HP(fc1) * LP(fc2), high = HP(fc1) * HP(fc2).
// The bands sum to an all-pass. Stereo.
struct Crossover3
{
    Biquad lp1[2], hp1[2], lp2[2], hp2[2], ap2;
    void design (double fc1, double fc2, double fs)
    {
        const double q = 1.0 / std::sqrt (2.0);
        for (int i = 0; i < 2; ++i)
        {
            lp1[i].design (Biquad::Type::lowPass, fc1, q, 0, fs); hp1[i].design (Biquad::Type::highPass, fc1, q, 0, fs);
            lp2[i].design (Biquad::Type::lowPass, fc2, q, 0, fs); hp2[i].design (Biquad::Type::highPass, fc2, q, 0, fs);
        }
        ap2.design (Biquad::Type::allPass, fc2, q, 0, fs);   // = LP4 + HP4 at fc2
    }
    void reset()
    {
        for (auto* f : { &lp1[0], &lp1[1], &hp1[0], &hp1[1], &lp2[0], &lp2[1], &hp2[0], &hp2[1], &ap2 }) f->reset();
    }
    inline void process (double x, int ch, double& low, double& mid, double& high)
    {
        low = ap2.process (lp1[1].process (lp1[0].process (x, ch), ch), ch);
        const double h = hp1[1].process (hp1[0].process (x, ch), ch);
        mid = lp2[1].process (lp2[0].process (h, ch), ch);
        high = hp2[1].process (hp2[0].process (h, ch), ch);
    }
};

// integer delay + first-order Thiran all-pass for the fraction (how the reference's fixed-time delay behaves: its
// impulse response is exactly this, a = (1 - d) / (1 + d)). Stereo.
struct FracDelay
{
    std::vector<double> buf[2];
    int len = 1, pos = 0, whole = 0;
    double a = 0, z[2] {};
    void prepare (double samples)
    {
        whole = (int) std::floor (samples);
        double d = samples - whole;
        if (d < 0.05 && whole > 0) { d += 1.0; --whole; }    // keep the all-pass coefficient away from the unstable edge
        a = (1 - d) / (1 + d);
        len = whole + 2;
        for (auto& b : buf) b.assign ((size_t) len, 0.0);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0);
        z[0] = z[1] = 0; pos = 0;
    }
    inline void process (double& l, double& r)
    {
        int rd = pos - whole; if (rd < 0) rd += len;
        double* x[2] { &l, &r };
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& b = buf[ch];
            b[(size_t) pos] = *x[ch];
            const double in = b[(size_t) rd];
            const double y = a * in + z[ch];       // y = a x + x[-1] - a y[-1]
            z[ch] = in - a * y;
            *x[ch] = y;
        }
        pos = pos + 1 == len ? 0 : pos + 1;
    }
};
}
