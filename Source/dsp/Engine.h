#pragma once
#include <memory>
#include <vector>
#include "Character.h"
#include "Filters.h"
#include "Limiter.h"
#include "Oversampler.h"
#include "Tuning.h"

// The whole audio path (MEASUREMENTS.md 1):
//   input gain -> [oversampled: EQ -> fixed delay -> character] -> limiter (look-ahead) -> output gain
// Bypassing the limiter also drops its look-ahead (as the reference does: its output then comes 38 samples early;
// the reported latency does not change). The host bypass gives the input delayed by the reported latency.
namespace ap::dsp
{
struct Params
{
    float inputGainDb = 0, outputGainDb = 0;
    float eqLowDb = 0, eqMidDb = 0, eqHighDb = 0;
    bool eqBypass = false;
    float amount = 0.5f;            // 0 .. 1 = 0 .. 200 %
    bool characterBypass = false;
    float limiterGainDb = 5.0f;
    bool limiterBypass = false;
    bool bypass = false;
    bool sidechainExternal = false;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock, int oversampling, const Tuning& t);
    void reset();
    int latency() const { return reportedLatency; }
    int oversamplingFactor() const { return selectedFactor; }   // as selected (8x processes at 4x, see Engine.cpp)

    // stereo in place; sc = side-chain channels (nullptr when there is none)
    void process (float* l, float* r, const float* scL, const float* scR, int n, const Params& p);

    // meters: deepest reductions since the last call (dB, positive)
    void takeBandGr (float* lowMidHigh) { character.takeGr (lowMidHigh); }
    float takeLimiterGr() { return limiter.takeGr(); }

private:
    void updateEq (const double* glide);       // the EQ gains in the glide's domain (Tuning::eqGlideDomain)
    double toGlide (double db) const;
    // a linear ramp toward a target over a fixed time (samples, fractional), re-planned when the target changes
    struct Ramp
    {
        double value = 0, target = 0, step = 0, left = 0, length = 1;
        void setLength (double n) { length = std::max (1.0, n); }
        void jump (double v) { value = target = v; left = 0; }
        void set (double v) { if (v != target) { target = v; step = (target - value) / length; left = length; } }
        inline double next() { return advance (1); }
        double advance (int k)              // k samples at once
        {
            if (left <= 0) return value;
            if (k >= left) { value = target; left = 0; }
            else { value += step * k; left -= k; }
            return value;
        }
        bool moving() const { return left > 0; }
    };
    // a gain that moves once per chunk by chunk / tau toward its target, linearly between chunk ends
    struct ChunkGain
    {
        double value = 1, end = 1, step = 0;
        void jump (double g) { value = end = g; step = 0; }
        void chunk (double target, int n, double tau)
        {
            value = end;
            end += (target - end) * std::min (1.0, n / tau);
            step = (end - value) / n;
        }
        inline double next() { value += step; return value; }
    };

    const Tuning* tune = nullptr;
    double fs = 48000.0, osRate = 48000.0;
    int osFactor = 1, selectedFactor = 1, maxBlock = 512, reportedLatency = 0, ctlLen = 128;
    Oversampler os;
    std::vector<double> work[2], scWork[2];

    Biquad eq[3];
    // first stage (the parameters' own ramps, read at chunk ends), then the stage the DSP follows
    Ramp inParam, eqParam[3], amountParam, eqOnParam;
    Ramp eqDb[3], amount, limGain;
    ChunkGain inGain, outGain;
    double gainTau = 480;
    bool first = true, eqOn = true, lastCharBypass = false, lastLimBypass = false;
    FracDelay core, bypassDelay;
    Character character;
    Limiter limiter;
};
}
