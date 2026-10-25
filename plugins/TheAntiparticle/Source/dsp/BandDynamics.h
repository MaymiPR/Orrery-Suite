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
    bool peak = false, feedback = false, dual = false, mixDb = false, truePeak = false;
    double env = 0, env2 = 0, grSm = 0, lastWet = 1, gr = 0;
    double hist[4] {};        // true-peak detector: the last four mid samples

    // peakDetector: 0 mean-square, 1 peak of the samples, 2 peak between the samples too (4x, cubic)
    void configure (const double* p, int peakDetector, bool feedbackDetector, double fs)
    {
        auto coef = [fs] (double ms) { return 1.0 - std::exp (-1.0 / (std::max (1.0e-3, ms) * 1.0e-3 * fs)); };
        mixDb = p[bpMixDb] > 0.5;
        mix = mixDb ? std::max (0.0, p[bpMix]) : std::clamp (p[bpMix], 0.0, 1.0);
        thresh = p[bpThresh];
        slope = 1.0 - 1.0 / std::max (1.0, p[bpRatio]);
        knee = std::max (0.0, p[bpKnee]);
        att = coef (p[bpAttack]);
        rel = coef (p[bpRelease]);
        side = p[bpSide]; side2 = side * side;
        peak = peakDetector != 0; truePeak = peakDetector == 2; feedback = feedbackDetector;
        gAtt = p[bpGainAttack] > 0.0 ? coef (p[bpGainAttack]) : 1.0;
        gRel = p[bpGainRelease] > 0.0 ? coef (p[bpGainRelease]) : 1.0;
        dual = p[bpAttack2] > 0.0;
        att2 = dual ? coef (p[bpAttack2]) : 1.0;
        rel2 = dual ? coef (p[bpRelease2]) : 1.0;
        slowOffset = p[bpSlowOffset];
    }
    void reset() { env = env2 = grSm = gr = 0; lastWet = 1; hist[0] = hist[1] = hist[2] = hist[3] = 0; }

    // the detector's mid and side samples in; the band gain n out (gr: the wet path's reduction, dB)
    inline double step (double m, double sd, bool externalSidechain)
    {
        if (feedback && ! externalSidechain) { m *= lastWet; sd *= lastWet; }
        double levelDb;
        if (peak)
        {
            double v = std::abs (m) + side * std::abs (sd);
            if (truePeak)
            {
                // Catmull-Rom between the two middle samples of the last four, at 0, 1/4, 1/2 and 3/4
                hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = hist[3]; hist[3] = m;
                const double a0 = hist[1], a1 = 0.5 * (hist[2] - hist[0]), a2 = hist[0] - 2.5 * hist[1] + 2 * hist[2] - 0.5 * hist[3],
                             a3 = 0.5 * (hist[3] - hist[0]) + 1.5 * (hist[1] - hist[2]);
                v = std::abs (a0);
                for (double t : { 0.25, 0.5, 0.75 }) v = std::max (v, std::abs (((a3 * t + a2) * t + a1) * t + a0));
            }
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
        if (mixDb) return std::pow (10.0, -mix * gr / 20.0);
        return (1.0 - mix) + mix * wet;
    }
};
}
