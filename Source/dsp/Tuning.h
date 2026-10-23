#pragma once
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "BandFilters.h"
#include "CharacterTable.h"

// Every constant of the DSP, from the measurements (MEASUREMENTS.md) and the fits (nulltest/fit_*.py). Kept in one
// struct so the offline renderer (tools/ApRender.cpp) can override any of them by name while fitting; the plug-in
// uses the defaults.
namespace ap::dsp
{
// The character section's per-band settings are tabled at amount 0, 12.5, ..., 200 % and interpolated linearly.
inline constexpr int kAmountNodes = 17;
enum BandParam
{
    bpMakeup,     // dB, gain after the shaper (= the small-signal gain)
    bpMix,        // 0 .. 1, share of the compressed (wet) path in the parallel mix
    bpCubic,      // the shaper sin (k u) / k as its cubic coefficient c = k^2 / 6 (0 = linear)
    bpThresh,     // dBFS (sine peak), compressor threshold
    bpRatio,      // compressor ratio
    bpKnee,       // dB, soft-knee width
    bpAttack,     // ms, detector attack
    bpRelease,    // ms, detector release
    bpSide,       // detector weight of the side signal (0 = mid only, 1 = L/R power sum)
    bpGainAttack, // ms, smoothing of the gain reduction (dB) while it grows (0 = none)
    bpGainRelease,// ms, ... while it shrinks
    bpAttack2,    // ms, second (slow) detector's attack (0 = no second detector)
    bpRelease2,   // ms, ... its release
    bpSlowOffset, // dB added to the slow detector's level before the larger of the two is taken
    bpWidth,      // dB, gain of the band output's side signal relative to its mid (after the shaper)
    kBandParams
};
inline constexpr const char* bandParamNames[kBandParams] { "makeup", "mix", "cubic", "thresh", "ratio", "knee", "attack", "release", "side",
                                                          "gainAttack", "gainRelease", "attack2", "release2", "slowOffset", "width" };
inline constexpr const char* bandNames[3] { "low", "mid", "high" };

struct Tuning
{
    // the always-present path: a fixed delay, fractional (first-order Thiran all-pass), D = ms * fs / 1000 + samples
    double coreDelayMs = 3.150528, coreDelaySamples = 0.054;

    // EQ: RBJ cookbook biquads at the processing (oversampled) rate
    double eqLowHz = 180.0, eqLowQ = 0.7, eqMidHz = 637.0, eqMidQ = 0.5, eqHighHz = 10000.0, eqHighQ = 0.7;

    // character: an IIR Linkwitz-Riley crossover for an external side-chain (the audio is split by FIR filters,
    // BandFilters.h), per band a parallel compressor, a sine shaper and a side gain
    double fc1 = 157.1, fc2 = 2675.8;
    double band[3][kAmountNodes][kBandParams] {};
    int detectorPeak[3] { 0, 0, 1 };      // 0: mean-square detector, 1: peak detector
    int feedback[3] { 0, 0, 0 };          // 1: the detector listens to the compressed (wet) signal

    // limiter (Limiter.h): window ahead and release (samples at 48 kHz), slow envelope, output trim
    double limWindow = 21, limReleaseSteps = 22;
    double limWindowAt = 0, limReleaseAt = 0;   // > 0: window / release in samples at the current rate (fitting)
    double limSlowAttackMs = 24.47, limSlowReleaseMs = 377.8;
    double limTrimDb = -0.1;              // after the 0 dBFS ceiling: the output's ceiling and small-signal gain

    // parameter smoothing
    // Parameters move at a control rate (Engine.cpp, MEASUREMENTS.md 5): each host block is cut into chunks of up to
    // ctlChunk samples. The input gain, EQ gains, amount and the EQ switch first ramp linearly (paramRampMs) and are
    // read at each chunk's end. Then the input and output gains move toward their target by chunk / gainTau per chunk
    // (linear between chunk ends); the EQ gains and the amount glide linearly over glideMs to each new value; the
    // limiter's input gain ramps linearly over limRampMs.
    double ctlChunk = 128, paramRampMs = 4.95, gainTauMs = 9.975, glideMs = 5.0, limRampMs = 10.0;
    double eqForm = 3;                    // the EQ filters: state-variable (Biquad::form; their gain moves are matched best)
    double eqPerSample = 1;               // 1: moving EQ gains redesign the filters every sample (else every 16)
    double charZeroRest = 0;              // 1: at amount 0 the band detectors rest (start from zero when it rises)
    double eqGlideDomain = 1;             // the EQ gains glide in dB (0), in amplitude 10^(dB/20) (1) or in A = 10^(dB/40) (2)

    double forceOversampling = 0;         // > 0: use this ratio instead of the settings page's (offline renders)

    Tuning() { setDefaults(); }

    void setDefaults()
    {
        // fitted per band and amount node to the reference (nulltest/make_table.py -> CharacterTable.h)
        for (int b = 0; b < 3; ++b)
        {
            detectorPeak[b] = characterDetectorPeak[b];
            for (int k = 0; k < kAmountNodes; ++k)
                for (int q = 0; q < kBandParams; ++q) band[b][k][q] = characterTable[b][k][q];
        }
    }

    // "mid.mix.8 = 0.7" (band.param.node, node 0..16 = 0..200 %), "mid.mix = 0.7" (every node), or a scalar name
    bool set (const std::string& key, double v)
    {
        struct S { const char* n; double* p; };
        const S scalars[] { { "coreDelayMs", &coreDelayMs }, { "coreDelaySamples", &coreDelaySamples },
                            { "eqLowHz", &eqLowHz }, { "eqLowQ", &eqLowQ }, { "eqMidHz", &eqMidHz }, { "eqMidQ", &eqMidQ },
                            { "eqHighHz", &eqHighHz }, { "eqHighQ", &eqHighQ }, { "fc1", &fc1 }, { "fc2", &fc2 },
                            { "limWindow", &limWindow }, { "limWindowAt", &limWindowAt }, { "limReleaseAt", &limReleaseAt }, { "limReleaseSteps", &limReleaseSteps }, { "limSlowAttackMs", &limSlowAttackMs },
                            { "limSlowReleaseMs", &limSlowReleaseMs }, { "limTrimDb", &limTrimDb }, { "ctlChunk", &ctlChunk }, { "paramRampMs", &paramRampMs }, { "gainTauMs", &gainTauMs },
                            { "glideMs", &glideMs }, { "limRampMs", &limRampMs }, { "eqForm", &eqForm }, { "eqPerSample", &eqPerSample }, { "eqGlideDomain", &eqGlideDomain }, { "charZeroRest", &charZeroRest },
                            { "oversampling", &forceOversampling } };
        for (auto& s : scalars)
            if (key == s.n) { *s.p = v; return true; }
        for (int b = 0; b < 3; ++b)
        {
            const std::string bn = bandNames[b];
            if (key == bn + ".peak") { detectorPeak[b] = (int) v; return true; }
            if (key == bn + ".feedback") { feedback[b] = (int) v; return true; }
            for (int q = 0; q < kBandParams; ++q)
            {
                const std::string base = bn + "." + bandParamNames[q];
                if (key == base) { for (auto& node : band[b]) node[q] = v; return true; }
                if (key.rfind (base + ".", 0) == 0)
                {
                    const int k = std::atoi (key.c_str() + base.size() + 1);
                    if (k < 0 || k >= kAmountNodes) return false;
                    band[b][k][q] = v;
                    return true;
                }
            }
        }
        return false;
    }

    // "key = value" lines; '#' starts a comment. Returns the number of unknown keys (printed).
    int load (const char* path)
    {
        FILE* f = std::fopen (path, "r");
        if (f == nullptr) { std::printf ("cannot read tuning %s\n", path); return 1; }
        char line[512];
        int bad = 0;
        while (std::fgets (line, sizeof (line), f))
        {
            if (char* h = std::strchr (line, '#')) *h = 0;
            char key[256]; double v;
            if (std::sscanf (line, " %255[^= ] = %lf", key, &v) == 2 && ! set (key, v)) { std::printf ("unknown tuning key %s\n", key); ++bad; }
        }
        std::fclose (f);
        return bad;
    }
};

// the plug-in's tuning (the defaults; ApRender changes it before it creates the processor)
inline Tuning& tuning()
{
    static Tuning t;
    return t;
}

// the core path's delay in samples: measured at 44.1 / 48 / 88.2 / 96 / 192 kHz (integer part + Thiran fraction of
// the fitted all-pass), Tuning::coreDelayMs / coreDelaySamples at other rates
inline double coreDelay (double fs, const Tuning& t)
{
    const double r[5] { 44100, 48000, 88200, 96000, 192000 }, d[5] { 138.98868, 151.27917, 277.95471, 302.53139, 604.95553 };
    for (int i = 0; i < 5; ++i)
        if (std::abs (fs - r[i]) < 1.0) return d[i];
    return t.coreDelayMs * 1.0e-3 * fs + t.coreDelaySamples;
}

// the limiter's look-ahead in samples, measured at 44.1 / 48 / 88.2 / 96 / 192 kHz (linear between)
inline int limiterLookahead (double fs)
{
    const double r[5] { 44100, 48000, 88200, 96000, 192000 }, l[5] { 40, 38, 50, 54, 96 };
    if (fs <= r[0]) return (int) std::lround (l[0] * fs / r[0]);
    for (int i = 0; i < 4; ++i)
        if (fs <= r[i + 1]) return (int) std::lround (l[i] + (l[i + 1] - l[i]) * (fs - r[i]) / (r[i + 1] - r[i]));
    return (int) std::lround (l[4] * fs / r[4]);
}
}
