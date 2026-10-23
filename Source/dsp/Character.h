#pragma once
#include <algorithm>
#include "Filters.h"
#include "FirBank.h"
#include "Tuning.h"

// The character section (MEASUREMENTS.md 3): a 3-band FIR split (FirBank; it carries the section's 3.15 ms delay);
// the side-chain is split by an IIR Linkwitz-Riley crossover (its level is all that is used); per band a stereo-linked parallel
// compressor (dry + wet, the wet path compressed; the GR meters show the wet path's reduction) followed by a cubic
// shaper (odd harmonics, H3 phase-locked at 180 degrees) and the band's make-up gain. Runs at the processing rate.
namespace ap::dsp
{
class Character
{
public:
    // delaySamples: the section's latency at the processing rate (the FIR filters' delay)
    void prepare (double processingRate, double delaySamples, const Tuning& t)
    {
        fs = processingRate;
        tune = &t;
        firs.prepare (fs, delaySamples);
        scXo.design (t.fc1, t.fc2, fs);
        reset();
        lastAmount = -1.0;
    }
    void reset()
    {
        firs.reset(); scXo.reset();
        for (auto& e : env) e = 0.0;
        for (auto& e : env2) e = 0.0;
        for (auto& g : grSm) g = 0.0;
        for (auto& w : lastWet) w = 1.0;
        for (auto& g : grMax) g = 0.0;
    }

    // amount 0 .. 1 (= 0 .. 200 %); interpolates the tabled band settings
    void setAmount (double amount)
    {
        if (amount == lastAmount) return;
        lastAmount = amount;
        const double x = std::clamp (amount, 0.0, 1.0) * (kAmountNodes - 1);
        const int k = std::min (kAmountNodes - 2, (int) x);
        const double f = x - k;
        for (int b = 0; b < 3; ++b)
        {
            double p[kBandParams];
            for (int q = 0; q < kBandParams; ++q) p[q] = tune->band[b][k][q] + (tune->band[b][k + 1][q] - tune->band[b][k][q]) * f;
            auto& s = st[b];
            s.makeup = std::pow (10.0, p[bpMakeup] / 20.0);
            s.mix = std::clamp (p[bpMix], 0.0, 1.0);
            s.cubic = p[bpCubic];
            s.thresh = p[bpThresh];
            s.slope = 1.0 - 1.0 / std::max (1.0, p[bpRatio]);
            s.knee = std::max (0.0, p[bpKnee]);
            s.att = 1.0 - std::exp (-1.0 / (std::max (1.0e-3, p[bpAttack]) * 1.0e-3 * fs));
            s.rel = 1.0 - std::exp (-1.0 / (std::max (1.0e-3, p[bpRelease]) * 1.0e-3 * fs));
            s.side2 = p[bpSide] * p[bpSide];
            s.side = p[bpSide];
            s.peak = tune->detectorPeak[b] != 0;
            s.feedback = tune->feedback[b] != 0;
            s.gAtt = p[bpGainAttack] > 0.0 ? 1.0 - std::exp (-1.0 / (p[bpGainAttack] * 1.0e-3 * fs)) : 1.0;
            s.gRel = p[bpGainRelease] > 0.0 ? 1.0 - std::exp (-1.0 / (p[bpGainRelease] * 1.0e-3 * fs)) : 1.0;
            s.dual = p[bpAttack2] > 0.0;
            s.att2 = s.dual ? 1.0 - std::exp (-1.0 / (p[bpAttack2] * 1.0e-3 * fs)) : 1.0;
            s.rel2 = s.dual ? 1.0 - std::exp (-1.0 / (std::max (1.0e-3, p[bpRelease2]) * 1.0e-3 * fs)) : 1.0;
            s.slowOffset = p[bpSlowOffset];
        }
    }

    // one stereo sample in place; sc = the external side-chain sample (nullptr: the input drives the detectors)
    inline void process (double& l, double& r, const double* scL = nullptr, const double* scR = nullptr)
    {
        double bl[3], br[3], dl[3], dr[3];
        firs.process (0, l, bl);
        firs.process (1, r, br);
        if (scL != nullptr)
        {
            scXo.process (*scL, 0, dl[0], dl[1], dl[2]);
            scXo.process (*scR, 1, dr[0], dr[1], dr[2]);
        }
        else
            for (int b = 0; b < 3; ++b) { dl[b] = bl[b]; dr[b] = br[b]; }

        l = r = 0.0;
        for (int b = 0; b < 3; ++b)
        {
            const auto& s = st[b];
            double m = 0.5 * (dl[b] + dr[b]), sd = 0.5 * (dl[b] - dr[b]);
            if (s.feedback && scL == nullptr) { m *= lastWet[b]; sd *= lastWet[b]; }
            double levelDb;
            if (s.peak)
            {
                const double v = std::abs (m) + s.side * std::abs (sd);
                env[b] += (v > env[b] ? s.att : s.rel) * (v - env[b]);
                levelDb = 20.0 * std::log10 (env[b] + 1.0e-12);
                if (s.dual)
                {
                    env2[b] += (v > env2[b] ? s.att2 : s.rel2) * (v - env2[b]);
                    levelDb = std::max (levelDb, 20.0 * std::log10 (env2[b] + 1.0e-12) + s.slowOffset);
                }
            }
            else
            {
                const double v = m * m + s.side2 * sd * sd;
                env[b] += (v > env[b] ? s.att : s.rel) * (v - env[b]);
                levelDb = 10.0 * std::log10 (env[b] + 1.0e-24) + 3.0103;   // a sine reads its peak
                if (s.dual)
                {
                    env2[b] += (v > env2[b] ? s.att2 : s.rel2) * (v - env2[b]);
                    levelDb = std::max (levelDb, 10.0 * std::log10 (env2[b] + 1.0e-24) + 3.0103 + s.slowOffset);
                }
            }
            const double over = levelDb - s.thresh;
            double gr = 0.0;
            if (2.0 * over >= s.knee) gr = s.slope * over;
            else if (2.0 * over > -s.knee) { const double t = over + 0.5 * s.knee; gr = s.slope * t * t / (2.0 * s.knee); }
            grSm[b] += (gr > grSm[b] ? s.gAtt : s.gRel) * (gr - grSm[b]);
            gr = grSm[b];
            grMax[b] = std::max (grMax[b], gr);
            const double wet = std::pow (10.0, -gr / 20.0);
            lastWet[b] = wet;
            const double n = (1.0 - s.mix) + s.mix * wet;
            const double ul = bl[b] * n, ur = br[b] * n;
            l += s.makeup * (ul - s.cubic * ul * ul * ul);
            r += s.makeup * (ur - s.cubic * ur * ur * ur);
        }
    }

    // deepest wet-path reduction (dB, positive) per band since the last call: low, mid, high
    void takeGr (float* out)
    {
        for (int b = 0; b < 3; ++b) { out[b] = (float) grMax[b]; grMax[b] = 0.0; }
    }

private:
    struct Band { double makeup = 1, mix = 0, cubic = 0, thresh = 0, slope = 0, knee = 0, att = 1, rel = 1, side = 0, side2 = 0, gAtt = 1, gRel = 1,
                  att2 = 1, rel2 = 1, slowOffset = 0;
                  bool peak = false, feedback = false, dual = false; };
    double fs = 48000.0, lastAmount = -1.0;
    const Tuning* tune = nullptr;
    FirBank firs;
    Crossover3 scXo;
    Band st[3];
    double env[3] {}, env2[3] {}, grMax[3] {}, grSm[3] {}, lastWet[3] { 1, 1, 1 };
};
}
