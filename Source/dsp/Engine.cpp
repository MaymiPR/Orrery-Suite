#include "Engine.h"

namespace ap::dsp
{
namespace
{
constexpr int kSub = 16;                                        // parameter ramps advance per 16 samples
// the core path's extra delay with oversampling (bypassed-path impulse peak at 48 kHz: 174 / 179 / 181 vs 151.3)
// (group delay at 1 kHz: 173.767 / 178.990 vs 151.279; 8x is exactly 4x two samples later)
double oversamplingExtra (int factor) { return factor >= 8 ? 27.711 + 2.0 : factor >= 4 ? 27.711 : factor >= 2 ? 22.488 : 0.0; }
}

void Engine::prepare (double sampleRate, int block, int oversampling, const Tuning& t)
{
    tune = &t;
    fs = sampleRate;
    maxBlock = std::max (block, kSub);
    selectedFactor = oversampling >= 8 ? 8 : oversampling >= 4 ? 4 : oversampling >= 2 ? 2 : 1;
    // the reference's 8x sounds exactly like its 4x (residual -82 dB below 16 kHz, nulltest/NULLTESTS.md), only
    // ~2 samples later: process at 4x and add the difference to the fixed delay
    osFactor = std::min (selectedFactor, 4);
    osRate = fs * osFactor;

    os.prepare (osFactor, kSub);
    const double osLatency = os.latency();
    for (auto& w : work) w.assign ((size_t) kSub, 0.0);
    for (auto& w : scWork) w.assign ((size_t) (kSub * osFactor), 0.0);

    // the fixed delay is a fixed time; with oversampling the reference's path is a little longer
    const double pathDelay = coreDelay (fs, t) + oversamplingExtra (selectedFactor) * fs / 48000.0;
    // the fixed delay sits inside the oversampled part, fraction included (the reference's too: our first-stage filters
    // are fitted with it in place, nulltest/fit_oversampling.py)
    const double osDelay = std::max (0.0, pathDelay - osLatency) * osFactor;     // at the processing rate
    core.prepare (osDelay);
    limiter.prepare (fs, t);
    reportedLatency = (int) std::lround (pathDelay + limiter.latency());
    bypassDelay.prepare ((double) reportedLatency);

    // with oversampling the reference's character path is a little later than its bypassed path (phase vs frequency of
    // the small-signal response: +0.012 / +0.040 samples at 2x / 4x)
    const double charOffset = osFactor >= 4 ? 0.0395 : osFactor >= 2 ? 0.0117 : 0.0;
    character.prepare (osRate, osDelay + charOffset * osFactor, t);
    for (auto& r : eqDb) r.setLength ((int) std::lround (t.rampMs * 1.0e-3 * fs));
    amount.setLength ((int) std::lround (t.rampMs * 1.0e-3 * fs));
    limGain.setLength ((int) std::lround (t.rampMs * 1.0e-3 * fs));
    gainCoef = 1.0 - std::exp (-1.0 / (t.gainSmoothMs * 1.0e-3 * fs));
    reset();
}

void Engine::reset()
{
    os.reset();
    for (auto& f : eq) f.reset();
    core.reset();
    bypassDelay.reset();
    character.reset();
    limiter.reset();
    first = true;
}

void Engine::updateEq (const double* db)
{
    const auto& t = *tune;
    eq[0].design (Biquad::Type::lowShelf, t.eqLowHz, t.eqLowQ, db[0], osRate);
    eq[1].design (Biquad::Type::peak, t.eqMidHz, t.eqMidQ, db[1], osRate);
    eq[2].design (Biquad::Type::highShelf, t.eqHighHz, t.eqHighQ, db[2], osRate);
}

void Engine::process (float* l, float* r, const float* scL, const float* scR, int n, const Params& p)
{
    const double inTarget = std::pow (10.0, p.inputGainDb / 20.0), outTarget = std::pow (10.0, p.outputGainDb / 20.0);
    const double eqTarget[3] { p.eqLowDb, p.eqMidDb, p.eqHighDb };
    const double limTarget = std::pow (10.0, p.limiterGainDb / 20.0);
    if (first)
    {
        inGain = inTarget; outGain = outTarget;
        for (int b = 0; b < 3; ++b) eqDb[b].jump (eqTarget[b]);
        amount.jump (p.amount); limGain.jump (limTarget);
        double db[3] { eqTarget[0], eqTarget[1], eqTarget[2] };
        updateEq (db);
        character.setAmount (p.amount);
        first = false;
    }
    for (int b = 0; b < 3; ++b) eqDb[b].set (eqTarget[b]);
    amount.set (p.amount);
    limGain.set (limTarget);
    const bool useSc = p.sidechainExternal && scL != nullptr && scR != nullptr;

    for (int start = 0; start < n; start += kSub)
    {
        const int m = std::min (kSub, n - start);
        // parameters for this slice
        if (eqDb[0].moving() || eqDb[1].moving() || eqDb[2].moving())
        {
            double db[3];
            for (int b = 0; b < 3; ++b) { for (int i = 0; i < m; ++i) eqDb[b].next(); db[b] = eqDb[b].value; }
            updateEq (db);
        }
        if (amount.moving()) { for (int i = 0; i < m; ++i) amount.next(); character.setAmount (amount.value); }

        double* w[2] { work[0].data(), work[1].data() };
        for (int i = 0; i < m; ++i)
        {
            inGain += (inTarget - inGain) * gainCoef;
            w[0][i] = l[start + i] * inGain;
            w[1][i] = r[start + i] * inGain;
        }
        double* sc[2] { scWork[0].data(), scWork[1].data() };
        if (useSc)
            for (int i = 0; i < m; ++i)          // the detectors only need the side-chain's level: held, not filtered
                for (int k = 0; k < osFactor; ++k) { sc[0][i * osFactor + k] = scL[start + i]; sc[1][i * osFactor + k] = scR[start + i]; }

        // oversampled part
        double* u[2] { w[0], w[1] };
        const int mu = m * osFactor;
        if (osFactor > 1) os.up (w[0], w[1], m, u[0], u[1]);
        for (int i = 0; i < mu; ++i)
        {
            double a = u[0][i], b = u[1][i];
            if (! p.eqBypass)
                for (auto& f : eq) { a = f.process (a, 0); b = f.process (b, 1); }
            // both paths run all the time (the switch between them is instant, as in the reference)
            double da = a, db = b;
            core.process (da, db);
            if (useSc) character.process (a, b, &sc[0][i], &sc[1][i]);
            else character.process (a, b);
            if (p.characterBypass) { a = da; b = db; }
            u[0][i] = a; u[1][i] = b;
        }
        if (osFactor > 1) os.down (w[0], w[1], m);

        // limiter, output gain, bypass
        for (int i = 0; i < m; ++i)
        {
            double a = w[0][i], b = w[1][i];
            const double g = limGain.next();
            if (! p.limiterBypass) { a *= g; b *= g; limiter.process (a, b); }
            outGain += (outTarget - outGain) * gainCoef;
            a *= outGain; b *= outGain;
            double dl = l[start + i], dr = r[start + i];
            bypassDelay.process (dl, dr);
            if (p.bypass) { a = dl; b = dr; }
            l[start + i] = (float) a; r[start + i] = (float) b;
        }
    }
}
}
