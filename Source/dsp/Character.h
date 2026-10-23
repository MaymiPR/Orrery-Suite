#pragma once
#include <algorithm>
#include "BandDynamics.h"
#include "Filters.h"
#include "FirBank.h"
#include "Tuning.h"

// The character section (MEASUREMENTS.md 3): a 3-band FIR split (FirBank; it carries the section's 3.15 ms delay);
// each band's nonlinear stage works on the band with steeper edges (s), the rest of the band passes linearly
// (y = x - s + F(s): the FIR bands' shallow stopbands are not compressed or shaped, MEASUREMENTS.md 3.11);
// the side-chain is split by an IIR Linkwitz-Riley crossover (its level is all that is used); per band a stereo-linked parallel
// compressor (dry + wet, the wet path compressed; the GR meters show the wet path's reduction; the detector hears the
// mid signal only) followed by a sine shaper (odd harmonics, H3 phase-locked at 180 degrees), the band's make-up gain
// and a side gain (the band widened, increasingly with the amount). Runs at the processing rate.
namespace ap::dsp
{
class Character
{
public:
    // delaySamples: the section's latency at the processing rate (the FIR filters' delay)
    // exactRates: the FIR filters may be the reference's own ones for this rate (FirBank::prepare)
    void prepare (double processingRate, double delaySamples, const Tuning& t, bool exactRates = false)
    {
        fs = processingRate;
        tune = &t;
        firs.prepare (fs, delaySamples, exactRates);
        scXo.design (t.fc1, t.fc2, fs);
        reset();
        lastAmount = -1.0;
    }
    void reset()
    {
        firs.reset(); scXo.reset();
        for (auto& d : dyn) d.reset();
        for (auto& g : grMax) g = 0.0;
    }

    // amount 0 .. 1 (= 0 .. 200 %); interpolates the tabled band settings
    void setAmount (double amount)
    {
        if (amount == lastAmount) return;
        lastAmount = amount;
        atZero = amount <= 0.0 && tune->charZeroRest > 0.5;
        const double x = std::clamp (amount, 0.0, 1.0) * (kAmountNodes - 1);
        const int k = std::min (kAmountNodes - 2, (int) x);
        const double f = x - k;
        for (int b = 0; b < 3; ++b)
        {
            double p[kBandParams];
            for (int q = 0; q < kBandParams; ++q) p[q] = tune->band[b][k][q] + (tune->band[b][k + 1][q] - tune->band[b][k][q]) * f;
            auto& s = st[b];
            s.makeup = std::pow (10.0, p[bpMakeup] / 20.0);
            // the shaper is sin (k u) / k (identified per cycle: u - c u^3 + 0.3 c^2 u^5 ..., nulltest/ident.py);
            // the tables keep c = k^2 / 6, its cubic coefficient
            s.drive = std::sqrt (std::max (0.0, 6.0 * p[bpCubic]));
            s.width = std::pow (10.0, p[bpWidth] / 20.0);
            dyn[b].configure (p, tune->detectorPeak[b] != 0, tune->feedback[b] != 0, fs);
        }
    }

    // one stereo sample in place; sc = the external side-chain sample (nullptr: the input drives the detectors)
    inline void process (double& l, double& r, const double* scL = nullptr, const double* scR = nullptr)
    {
        if (atZero) for (auto& d : dyn) d.reset();
        double bl[FirBank::kFilters], br[FirBank::kFilters], dl[3], dr[3];
        firs.process (0, l, bl);
        firs.process (1, r, br);
        const double* sl = bl + 3;      // what the nonlinear stages hear
        const double* sr = br + 3;
        if (scL != nullptr)
        {
            scXo.process (*scL, 0, dl[0], dl[1], dl[2]);
            scXo.process (*scR, 1, dr[0], dr[1], dr[2]);
        }
        else
            for (int b = 0; b < 3; ++b) { dl[b] = sl[b]; dr[b] = sr[b]; }

        l = r = 0.0;
        for (int b = 0; b < 3; ++b)
        {
            const auto& s = st[b];
            const double n = dyn[b].step (0.5 * (dl[b] + dr[b]), 0.5 * (dl[b] - dr[b]), scL != nullptr);
            grMax[b] = std::max (grMax[b], dyn[b].gr);
            double yl = sl[b] * n, yr = sr[b] * n;
            if (s.drive > 1.0e-6) { yl = std::sin (s.drive * yl) / s.drive; yr = std::sin (s.drive * yr) / s.drive; }
            yl += bl[b] - sl[b];
            yr += br[b] - sr[b];
            const double m = 0.5 * s.makeup * (yl + yr), sd = 0.5 * s.makeup * s.width * (yl - yr);
            l += m + sd;
            r += m - sd;
        }
    }

    // deepest wet-path reduction (dB, positive) per band since the last call: low, mid, high
    void takeGr (float* out)
    {
        for (int b = 0; b < 3; ++b) { out[b] = (float) grMax[b]; grMax[b] = 0.0; }
    }

private:
    struct Band { double makeup = 1, drive = 0, width = 1; };
    double fs = 48000.0, lastAmount = -1.0;
    bool atZero = false;
    const Tuning* tune = nullptr;
    FirBank firs;
    Crossover3 scXo;
    Band st[3];
    BandDynamics dyn[3];
    double grMax[3] {};
};
}
