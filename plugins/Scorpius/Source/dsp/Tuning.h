#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// Every constant of Scorpius's DSP, from the measurements (MEASUREMENTS.md) and the fits (nulltest/fit_*.py), in one
// struct so the offline renderer (tools/ScRender.cpp) can override any of them by name while fitting; the plug-in uses
// the defaults.
namespace sc::dsp
{
struct Tuning
{
    // harmonic EQ (Eq.h): per band (40, 100, 1k, 3k, 10k) an RBJ filter (kind 0 peaking, 1 high shelf, 2 low shelf), its
    // gain the knob's law times a scale, its boost through a high-pass (0: none), then the soft clip
    // S(u) = u / (1 + |u|^p)^(1/p) with ceiling c (nulltest/fit_eqfinal.py, fit_eqsat.py)
    double eqHz[5] { 43.799, 99.953, 982.856, 3010.255, 9661.045 };
    double eqQ[5] { 0.701, 0.6672, 0.6677, 0.6682, 0.7054 };
    double eqKind[5] { 2, 0, 0, 0, 1 };
    double eqGainScale[5] { 7.179 / 7.124, 7.132 / 7.124, 7.133 / 7.124, 7.135 / 7.124, 7.145 / 7.124 };
    double eqHpHz[5] { 20.196, 20.0, 20.0, 20.0, 20.0 }, eqHpQ[5] { 0.717, 0.7071, 0.7071, 0.7071, 0.7071 };
    double eqCeiling = 0.4703, eqKnee = 2.151;
    double eqCeilingScale[5] { 1, 1, 1, 1, 1 };   // per band
    // the knob's law: the filter's gain (dB, before the band's scale) at knob 0, 0.5, 1, 2, 3, 4, 5, 6 dB
    double eqLaw[8] { 0, 0.622, 1.2385, 2.4552, 3.6506, 4.826, 5.9828, 7.1224 };   // (nulltest/fit_eqlaw.py)
    double eqHpAfter = 1;                  // 1: the high-pass after the soft clip (fitted: nulls 10 ... 16 dB deeper)
    double eqRampMs = 5.0;                 // a band's gain moves linearly to a new value over this time
    double switchMs = 5.0;                 // a module's power crossfades over this time
    double amountRampMs = 10.0;            // harmonics, compression and lift amounts move linearly over this time

    // the expander (Expander.h): threshold (dBFS peak), gain slope below it, hold window, release time constant
    double expThresholdDb = -58.098, expSlope = 2.028, expHoldMs = 1133.8, expReleaseMs = 207.8;   // (nulltest/fit_exp.py)
    // harmonics (Harmonics.h), [0] 80 Hz, [1] 50 Hz: the bell's frequency, Q, gain, the high-pass after the clip, the
    // clip's ceiling and knee (nulltest/fit_harm.py)
    double harmHz[2] { 111.5166, 69.9423 }, harmQ[2] { 0.3174, 0.3127 }, harmGainDb[2] { 5.8189, 5.843 };
    double harmHpHz[2] { 20.6772, 20.4059 }, harmCeiling[2] { 1.6403, 1.4107 }, harmKnee[2] { 1.9678, 2.2203 };

    // compression (Compressor.h), [0] mode a, [1] mode b: threshold (dBFS of the detector's level), ratio, knee width
    // (dB), detector attack / release (ms), RMS share of the detector (0 = peak follower), gain smoothing (ms, 0 = none)
    double compThresholdDb[2] { -12.818, -12.827 }, compRatio[2] { 4.987, 4.969 }, compKneeDb[2] { 7.391, 7.341 };   // (nulltest/fit_comp.py)
    double compAttackMs[2] { 1.026, 1.043 }, compReleaseMs[2] { 20.54, 19.746 }, compRms[2] { 0.039, 0.047 };
    double compGainAttackMs[2] { 0.0, 0.0 }, compGainReleaseMs[2] { 0.0, 0.0 };

    // parallels (Parallels.h): each path's weight (its power times its fader's taper) moves linearly over this time
    double parRampMs = 2.0;
    // sub: gain x 6th-order Butterworth low-pass x high-pass (nulltest/fit_sub3.py)
    double subGain = 1.995, subLowPassHz = 140.207, subHighPassHz = 15.003;
    // loud: the gain (dB) at detector levels -72, -66, ..., +12 dBFS, the detector's attack, release and RMS share
    double loudCurve[15] { 8.994, 7.202, 6.544, 5.723, 5.030, 4.409, 3.839, 3.332, 2.879, 2.479, 1.845, 0.903, 0.473, -0.3, 0.197 };   // (nulltest/fit_loud.py)
    double loudAttackMs = 0.6695, loudReleaseMs = 12.896, loudRms = 0.0096;
    // grit: the envelopes' times (ms), boost per dB of fast over slow, its soft limit (dB), the fade-in threshold and range
    double gritFastAttackMs = 2.2896, gritFastReleaseMs = 0.3183, gritSlowAttackMs = 72.285, gritSlowReleaseMs = 183.3;   // (nulltest/fit_grit.py)
    double gritAmount = 0.28818, gritMaxDb = 6.0493, gritThresholdDb = -54.145, gritRangeDb = 39.807;
    // low: the input low-pass (Hz, order), the output low-pass and its DC high-pass, gain, soft limit, flip hysteresis
    double lowInHz = 152.155, lowInOrder = 2, lowOutHz = 55.129, lowOutOrder = 6, lowHpHz = 3.292, lowGain = -1.0338, lowCeiling = 0.5892,
           lowHysteresis = 1.0e-4;

    std::vector<std::pair<const char*, double*>> entries()
    {
        std::vector<std::pair<const char*, double*>> e { { "eqCeiling", &eqCeiling }, { "eqKnee", &eqKnee }, { "eqRampMs", &eqRampMs },
                                                         { "switchMs", &switchMs }, { "amountRampMs", &amountRampMs }, { "eqHpAfter", &eqHpAfter },
                                                         { "parRampMs", &parRampMs }, { "subGain", &subGain }, { "subLowPassHz", &subLowPassHz },
                                                         { "subHighPassHz", &subHighPassHz }, { "expThresholdDb", &expThresholdDb },
                                                         { "expSlope", &expSlope }, { "expHoldMs", &expHoldMs }, { "expReleaseMs", &expReleaseMs },
                                                         { "loudAttackMs", &loudAttackMs }, { "loudReleaseMs", &loudReleaseMs }, { "loudRms", &loudRms },
                                                         { "lowInHz", &lowInHz }, { "lowInOrder", &lowInOrder }, { "lowOutHz", &lowOutHz }, { "lowOutOrder", &lowOutOrder },
                                                         { "lowHpHz", &lowHpHz }, { "lowGain", &lowGain }, { "lowCeiling", &lowCeiling }, { "lowHysteresis", &lowHysteresis },
                                                         { "gritFastAttackMs", &gritFastAttackMs }, { "gritFastReleaseMs", &gritFastReleaseMs },
                                                         { "gritSlowAttackMs", &gritSlowAttackMs }, { "gritSlowReleaseMs", &gritSlowReleaseMs },
                                                         { "gritAmount", &gritAmount }, { "gritMaxDb", &gritMaxDb }, { "gritThresholdDb", &gritThresholdDb },
                                                         { "gritRangeDb", &gritRangeDb } };
        static const char* loudKeys[15] { "loud0", "loud1", "loud2", "loud3", "loud4", "loud5", "loud6", "loud7", "loud8", "loud9", "loud10",
                                          "loud11", "loud12", "loud13", "loud14" };
        for (int k = 0; k < 15; ++k) e.push_back ({ loudKeys[k], &loudCurve[k] });
        static const char* harmKeys[2][6] { { "harm80.hz", "harm80.q", "harm80.gain", "harm80.hp", "harm80.ceil", "harm80.knee" },
                                            { "harm50.hz", "harm50.q", "harm50.gain", "harm50.hp", "harm50.ceil", "harm50.knee" } };
        static const char* compKeys[2][8] { { "compA.thresh", "compA.ratio", "compA.knee", "compA.attack", "compA.release", "compA.rms", "compA.gAttack", "compA.gRelease" },
                                            { "compB.thresh", "compB.ratio", "compB.knee", "compB.attack", "compB.release", "compB.rms", "compB.gAttack", "compB.gRelease" } };
        for (int m = 0; m < 2; ++m)
        {
            double* v[8] { &compThresholdDb[m], &compRatio[m], &compKneeDb[m], &compAttackMs[m], &compReleaseMs[m], &compRms[m],
                           &compGainAttackMs[m], &compGainReleaseMs[m] };
            for (int k = 0; k < 8; ++k) e.push_back ({ compKeys[m][k], v[k] });
        }
        for (int m = 0; m < 2; ++m)
        {
            double* v[6] { &harmHz[m], &harmQ[m], &harmGainDb[m], &harmHpHz[m], &harmCeiling[m], &harmKnee[m] };
            for (int k = 0; k < 6; ++k) e.push_back ({ harmKeys[m][k], v[k] });
        }
        static const char* bandKeys[5][7] { { "eq0.hz", "eq0.q", "eq0.kind", "eq0.scale", "eq0.hp", "eq0.hpq", "eq0.ceil" },
                                            { "eq1.hz", "eq1.q", "eq1.kind", "eq1.scale", "eq1.hp", "eq1.hpq", "eq1.ceil" },
                                            { "eq2.hz", "eq2.q", "eq2.kind", "eq2.scale", "eq2.hp", "eq2.hpq", "eq2.ceil" },
                                            { "eq3.hz", "eq3.q", "eq3.kind", "eq3.scale", "eq3.hp", "eq3.hpq", "eq3.ceil" },
                                            { "eq4.hz", "eq4.q", "eq4.kind", "eq4.scale", "eq4.hp", "eq4.hpq", "eq4.ceil" } };
        static const char* lawKeys[8] { "eqLaw0", "eqLaw1", "eqLaw2", "eqLaw3", "eqLaw4", "eqLaw5", "eqLaw6", "eqLaw7" };
        for (int k = 0; k < 8; ++k) e.push_back ({ lawKeys[k], &eqLaw[k] });
        for (int b = 0; b < 5; ++b)
        {
            double* v[7] { &eqHz[b], &eqQ[b], &eqKind[b], &eqGainScale[b], &eqHpHz[b], &eqHpQ[b], &eqCeilingScale[b] };
            for (int k = 0; k < 7; ++k) e.push_back ({ bandKeys[b][k], v[k] });
        }
        return e;
    }
    bool set (const std::string& key, double v)
    {
        for (auto& e : entries())
            if (key == e.first) { *e.second = v; return true; }
        return false;
    }
    // a file of "key value" lines; returns the number of unknown keys
    int load (const char* path)
    {
        int bad = 0;
        if (auto* f = std::fopen (path, "r"))
        {
            char key[256]; double v;
            while (std::fscanf (f, "%255s %lf", key, &v) == 2)
                if (! set (key, v)) { std::printf ("unknown tuning key %s\n", key); ++bad; }
            std::fclose (f);
        }
        return bad;
    }
};

inline Tuning& tuning()
{
    static Tuning t;
    return t;
}
}
