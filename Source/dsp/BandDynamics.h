#pragma once
#include <algorithm>
#include <cmath>
#include "Tuning.h"

// One character band's gain computer: detector (mean-square or peak, attack / release, optional slow second
// detector), soft-knee static curve, optional smoothing of the reduction, parallel mix. Gives the gain n applied to
// the band before its shaper. Shared by Character.h and tools/charsim.cpp (the fitting library).
namespace ap::dsp
{
struct BandDynamics
{
    double mix = 0, thresh = 0, slope = 0, knee = 0, att = 1, rel = 1, side = 0, side2 = 0, gAtt = 1, gRel = 1,
           att2 = 1, rel2 = 1, slowOffset = 0;
    bool peak = false, feedback = false, dual = false;
    double env = 0, env2 = 0, grSm = 0, lastWet = 1, gr = 0;

    void configure (const double* p, bool peakDetector, bool feedbackDetector, double fs)
    {
        auto coef = [fs] (double ms) { return 1.0 - std::exp (-1.0 / (std::max (1.0e-3, ms) * 1.0e-3 * fs)); };
        mix = std::clamp (p[bpMix], 0.0, 1.0);
        thresh = p[bpThresh];
        slope = 1.0 - 1.0 / std::max (1.0, p[bpRatio]);
        knee = std::max (0.0, p[bpKnee]);
        att = coef (p[bpAttack]);
        rel = coef (p[bpRelease]);
        side = p[bpSide]; side2 = side * side;
        peak = peakDetector; feedback = feedbackDetector;
        gAtt = p[bpGainAttack] > 0.0 ? coef (p[bpGainAttack]) : 1.0;
        gRel = p[bpGainRelease] > 0.0 ? coef (p[bpGainRelease]) : 1.0;
        dual = p[bpAttack2] > 0.0;
        att2 = dual ? coef (p[bpAttack2]) : 1.0;
        rel2 = dual ? coef (p[bpRelease2]) : 1.0;
        slowOffset = p[bpSlowOffset];
    }
    void reset() { env = env2 = grSm = gr = 0; lastWet = 1; }

    // the detector's mid and side samples in; the band gain n out (gr: the wet path's reduction, dB)
    inline double step (double m, double sd, bool externalSidechain)
    {
        if (feedback && ! externalSidechain) { m *= lastWet; sd *= lastWet; }
        double levelDb;
        if (peak)
        {
            const double v = std::abs (m) + side * std::abs (sd);
            env += (v > env ? att : rel) * (v - env);
            levelDb = 20.0 * std::log10 (env + 1.0e-12);
            if (dual)
            {
                env2 += (v > env2 ? att2 : rel2) * (v - env2);
                levelDb = std::max (levelDb, 20.0 * std::log10 (env2 + 1.0e-12) + slowOffset);
            }
        }
        else
        {
            const double v = m * m + side2 * sd * sd;
            env += (v > env ? att : rel) * (v - env);
            levelDb = 10.0 * std::log10 (env + 1.0e-24) + 3.0103;   // a sine reads its peak
            if (dual)
            {
                env2 += (v > env2 ? att2 : rel2) * (v - env2);
                levelDb = std::max (levelDb, 10.0 * std::log10 (env2 + 1.0e-24) + 3.0103 + slowOffset);
            }
        }
        const double over = levelDb - thresh;
        double g = 0.0;
        if (2.0 * over >= knee) g = slope * over;
        else if (2.0 * over > -knee) { const double t = over + 0.5 * knee; g = slope * t * t / (2.0 * knee); }
        grSm += (g > grSm ? gAtt : gRel) * (g - grSm);
        gr = grSm;
        const double wet = std::pow (10.0, -gr / 20.0);
        lastWet = wet;
        return (1.0 - mix) + mix * wet;
    }
};
}
