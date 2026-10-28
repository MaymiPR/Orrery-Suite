#pragma once
#include <functional>
#include <vector>
#include "Eq.h"
#include "Parallels.h"
#include "Expander.h"
#include "Compressor.h"
#include "LearnedModules.h"
#include "Filters.h"

// Scorpius's processing between the input and output gains (MEASUREMENTS.md 1): drive -> harmonics -> compression ->
// lift -> harmonic EQ, then the four parallels (sub, low, loud, grit) mixed in from the end of the chain. Parameters
// follow Cradle's control rate (128-sample chunks, Filters.h Ramp); a module's power switches at a block's start and
// most modules start afresh when switched on (MEASUREMENTS.md 12).
namespace sc::dsp
{
struct Params
{
    bool driveOn = true, hiPass = true; int driveColor = 0;
    bool harmOn = true; int harmFreq = 80; float harmAmount = 0.8f;
    bool compOn = true; int compMode = 1; float compAmount = 0.9f;
    bool liftOn = true; float liftAmount = 0.8f;
    bool eqOn = false; float eq[5] {};
    bool parOn[4] {}; float parLevel[4] {};
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    // key: the side-chain (EXT) for the compressor's detector, or null
    void process (float* l, float* r, int n, const Params&, const float* keyL = nullptr, const float* keyR = nullptr);

private:
    // a module's power: 0 ... 1 (the processed share), crossfading linearly
    struct Power
    {
        Ramp mix;
        bool on = false, target = false; long wait = 0;    // the switch as the module sees it, and a pending change
        int bit = 0;                                       // its bit in Tuning::restartOnEnable
        bool active() const { return mix.value > 0.0 || mix.moving(); }
    };
    void runModule (Power&, bool on, int n, const std::function<void (double*, double*, int)>& fn,
                    const std::function<void()>& restart = {});

    double fs = 48000.0;
    std::vector<double> bufL, bufR, wetL, wetR;
    bool first = true;
    Power eqPower, harmPower, compPower, drivePower, liftPower;
    Drive drive;
    Lift lift;
    Ramp harmAmount, compAmount, liftAmount;   // the amounts: Cradle's chunked smoothing, ~8 ms (MEASUREMENTS.md 12)
    std::vector<double> keyBufL, keyBufR;
    Compressor comp;
public:
    double compReduction = 0;              // the last block's largest gain reduction (dB)
    double compReadout() const { return comp.readout(); }   // what the readout above the knob shows (dB, positive)
private:
    Harmonics harm;
    Expander expander;
    std::vector<double> expGain;
    HarmonicEq eq;
    std::vector<double> chainL, chainR;    // the chain's output, which the parallels listen to
    Ramp parWeight[4];
    SubPath sub;
    LoudPath loud;
    LowPath low;
    Grit grit;
};
}
