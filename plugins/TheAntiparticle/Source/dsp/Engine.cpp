#include "Engine.h"

namespace ap::dsp
{
namespace
{
constexpr int kSub = 16;                                        // parameter ramps advance per 16 samples
static_assert (kSub * 8 <= Character::maxStretch, "a stretch (kSub samples, oversampled) must fit the character section's buffers");
// with oversampling the reference's path is its filters' delay plus a fraction of a sample, measured per rate at 2x and
// 4x (nulltest/os_rates_fit.py: the whole path's delay fitted with the designed filters in place); its 8x is its 4x
// exactly 2 samples later (music -64.8 dB that way; its bypassed path alone fits 1.95)
double oversamplingResidual (double fs, int factor)
{
    if (factor < 2) return 0.0;
    const double r[4] { 44100, 48000, 88200, 96000 };
    const double d[4][2] { { -0.0113, 0.4660 }, { 0.4865, 0.7097 }, { -0.0453, -0.1358 }, { 0.4464, 0.3391 } };
    double v = 0.0;
    for (int i = 0; i < 4; ++i)
        if (std::abs (fs - r[i]) < 1.0) v = d[i][factor >= 4 ? 1 : 0];
    return v + (factor >= 8 ? 2.0 : 0.0);
}
}

void Engine::prepare (double sampleRate, int block, int oversampling, const Tuning& t)
{
    tune = &t;
    fs = sampleRate;
    maxBlock = std::max (block, kSub);
    selectedFactor = oversampling >= 8 ? 8 : oversampling >= 4 ? 4 : oversampling >= 2 ? 2 : 1;
    // the reference's 8x sounds exactly like its 4x (residual -82 dB below 16 kHz, nulltest/NULLTESTS.md), only ~2
    // samples later: process at 4x with the longer delay (Oversampler.h)
    os.prepare (fs, selectedFactor, kSub);
    osFactor = os.factor();
    osRate = fs * osFactor;
    const double osLatency = os.latency();
    for (auto& w : work) w.assign ((size_t) kSub, 0.0);
    for (auto& w : scWork) w.assign ((size_t) (kSub * osFactor), 0.0);
    amounts.assign ((size_t) (kSub * osFactor), 0.0);

    // the fixed delay is a fixed time; with oversampling the reference's path is a little longer
    const double pathDelay = coreDelay (fs, t) + (selectedFactor > 1 ? osLatency + oversamplingResidual (fs, selectedFactor) + t.osDelayAdd : 0.0);
    // the fixed delay sits inside the oversampled part, fraction included (the reference's too: our first-stage filters
    // are fitted with it in place, nulltest/fit_oversampling.py)
    const double osDelay = std::max (0.0, pathDelay - osLatency) * osFactor;     // at the processing rate
    core.prepare (osDelay);
    limiter.prepare (fs, t);
    reportedLatency = (int) std::lround (pathDelay + limiter.latency());
    bypassDelay.prepare ((double) reportedLatency);

    // with oversampling the reference's character path is a little later than its bypassed path (48 kHz: from the
    // phase of the small-signal response, then refined on drums and pink, nulltest/os_probe.py: the nulls move by
    // 10 dB per 0.002 samples, drums and pink agreeing)
    const double offs[3][3] { { 0.0097, 0.0330, 0.0330 }, { 0.0137, 0.0395, 0.0395 }, { 0.0517, 0.1595, 0.0395 } };   // 44.1 / 48 / 96 kHz; 2x / 4x / 8x
    const int row = std::abs (fs - 44100.0) < 1.0 ? 0 : std::abs (fs - 96000.0) < 1.0 ? 2 : 1;
    const double charOffset = selectedFactor >= 2 ? offs[row][selectedFactor >= 8 ? 2 : selectedFactor >= 4 ? 1 : 0] : 0.0;
    // above 48 kHz the crossover is the host rate's own redrawn at the processing rate; at 44.1 kHz the 48 kHz one
    // redrawn nulls far better (drums -59 vs -31 dB). 88.2 kHz is not measured: treated like 96 kHz
    const bool aboveBase = fs > 48001.0;
    const bool hostFilters = t.osHostFilters >= 0 ? t.osHostFilters > 0.5 : aboveBase;
    character.prepare (osRate, osDelay + charOffset * osFactor, t, osFactor == 1, hostFilters ? fs : 48000.0,
                       hostFilters ? coreDelay (fs, t) : coreDelay (48000.0, t));
    // the control-rate smoothing (Tuning.h, MEASUREMENTS.md 5)
    const double paramRamp = t.paramRampMs * 1.0e-3 * fs, glide = t.glideMs * 1.0e-3 * fs;
    inParam.setLength (paramRamp); amountParam.setLength (paramRamp); eqOnParam.setLength (paramRamp);
    for (auto& r : eqParam) r.setLength (paramRamp);
    for (auto& r : eqDb) r.setLength (glide);
    amount.setLength (glide);
    limGain.setLength (t.limRampMs * 1.0e-3 * fs);
    gainTau = t.gainTauMs * 1.0e-3 * fs;
    for (auto& f : eq) f.form = (int) t.eqForm;
    ctlLen = std::max (1, (int) std::lround (t.ctlChunk));
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

double Engine::toGlide (double db) const
{
    const int d = (int) tune->eqGlideDomain;
    return d == 1 ? std::pow (10.0, db / 20.0) : d == 2 ? std::pow (10.0, db / 40.0) : db;
}

void Engine::updateEq (const double* glide)
{
    const auto& t = *tune;
    const int d = (int) t.eqGlideDomain;
    double db[3];
    for (int b = 0; b < 3; ++b) db[b] = d == 1 ? 20.0 * std::log10 (glide[b]) : d == 2 ? 40.0 * std::log10 (glide[b]) : glide[b];
    eq[0].design (Biquad::Type::lowShelf, t.eqLowHz, t.eqLowQ, db[0], osRate);
    eq[1].design (Biquad::Type::peak, t.eqMidHz, t.eqMidQ, db[1], osRate);
    eq[2].design (Biquad::Type::highShelf, t.eqHighHz, t.eqHighQ, db[2], osRate);
}

void Engine::process (float* l, float* r, const float* scL, const float* scR, int n, const Params& p)
{
    const double eqTarget[3] { p.eqLowDb, p.eqMidDb, p.eqHighDb };
    const double limTarget = std::pow (10.0, p.limiterGainDb / 20.0);
    if (first)
    {
        inParam.jump (p.inputGainDb); inGain.jump (std::pow (10.0, p.inputGainDb / 20.0));
        outGain.jump (std::pow (10.0, p.outputGainDb / 20.0));
        for (int b = 0; b < 3; ++b) { eqParam[b].jump (eqTarget[b]); eqDb[b].jump (toGlide (eqTarget[b])); }
        amountParam.jump (p.amount); amount.jump (p.amount); limGain.jump (limTarget);
        eqOnParam.jump (p.eqBypass ? 0.0 : 1.0); eqOn = ! p.eqBypass;
        const double g0[3] { toGlide (eqTarget[0]), toGlide (eqTarget[1]), toGlide (eqTarget[2]) };
        updateEq (g0);
        character.setAmount (p.amount);
        lastCharBypass = p.characterBypass; lastLimBypass = p.limiterBypass;
        charSwitch.jump (p.characterBypass); eqSwitch.jump (p.eqBypass);
        first = false;
    }
    // with host blocks shorter than a control chunk the switches of the oversampled section (EQ, character) take
    // effect once ctlLen samples have gathered: 128 - n rounded up to whole blocks later (measured at 32 / 64 / 96)
    const long switchDelay = n < ctlLen ? (long) ((ctlLen - n + n - 1) / n) * n : 0;
    charSwitch.set (p.characterBypass, switchDelay);
    eqSwitch.set (p.eqBypass, switchDelay);
    // the character section and the limiter do not run while bypassed and start afresh when switched back on (the
    // reference's do: its toggles null at -59 dB this way, -31 dB with the stages kept running)
    if (lastLimBypass && ! p.limiterBypass) limiter.reset();
    lastLimBypass = p.limiterBypass;
    const bool useSc = p.sidechainExternal && scL != nullptr && scR != nullptr;

    // control rate: chunks of up to ctlLen samples from the block's start
    for (int c0 = 0; c0 < n; c0 += ctlLen)
    {
        const int len = std::min (ctlLen, n - c0);
        if (lastCharBypass && ! charSwitch.value) character.reset();
        lastCharBypass = charSwitch.value;
        const bool charBypass = charSwitch.value;
        inParam.set (p.inputGainDb);
        inGain.chunk (std::pow (10.0, inParam.advance (len) / 20.0), len, gainTau);
        outGain.chunk (std::pow (10.0, p.outputGainDb / 20.0), len, gainTau);
        for (int b = 0; b < 3; ++b) { eqParam[b].set (eqTarget[b]); eqDb[b].set (toGlide (eqParam[b].advance (len))); }
        amountParam.set (p.amount); amount.set (amountParam.advance (len));
        // the EQ switch follows its ramp as read at the previous chunk's end
        eqOn = eqOnParam.value >= 0.5;
        eqOnParam.set (eqSwitch.value ? 0.0 : 1.0); eqOnParam.advance (len);
        limGain.set (limTarget);

        for (int start = c0; start < c0 + len; start += kSub)
        {
            const int m = std::min (kSub, c0 + len - start);
            const bool eqMoving = eqDb[0].moving() || eqDb[1].moving() || eqDb[2].moving();
            const bool eqEverySample = eqMoving && tune->eqPerSample > 0.5;
            if (eqMoving && ! eqEverySample)
            {
                double db[3];
                for (int b = 0; b < 3; ++b) db[b] = eqDb[b].advance (m);
                updateEq (db);
            }
            const bool amountMoving = amount.moving();     // then the character settings follow it sample by sample

            double* w[2] { work[0].data(), work[1].data() };
            for (int i = 0; i < m; ++i)
            {
                const double g = inGain.next();
                w[0][i] = l[start + i] * g;
                w[1][i] = r[start + i] * g;
            }
            double* sc[2] { scWork[0].data(), scWork[1].data() };
            if (useSc)
                for (int i = 0; i < m; ++i)          // the detectors only need the side-chain's level: held, not filtered
                    for (int k = 0; k < osFactor; ++k) { sc[0][i * osFactor + k] = scL[start + i]; sc[1][i * osFactor + k] = scR[start + i]; }

            // oversampled part
            double* u[2] { w[0], w[1] };
            const int mu = m * osFactor;
            if (osFactor > 1) os.up (w[0], w[1], m, u[0], u[1]);
            // in two passes: the EQ and the fixed-delay path sample by sample, then (the character section on) its
            // crossover across the stretch and its bands sample by sample. The same steps in the same order per sample
            for (int i = 0; i < mu; ++i)
            {
                if (amountMoving && i % osFactor == 0) amounts[(size_t) i] = amount.next();
                if (eqEverySample && i % osFactor == 0)
                {
                    double db[3];
                    for (int q = 0; q < 3; ++q) db[q] = eqDb[q].next();
                    updateEq (db);
                }
                double a = u[0][i], b = u[1][i];
                // the EQ filters keep running while it is switched off
                double ea = a, eb = b;
                for (auto& f : eq) { ea = f.process (ea, 0); eb = f.process (eb, 1); }
                if (eqOn) { a = ea; b = eb; }
                double da = a, db = b;         // the fixed-delay path runs all the time (the switch to it is instant)
                core.process (da, db);
                if (charBypass) { a = da; b = db; }
                u[0][i] = a; u[1][i] = b;
            }
            if (! charBypass)
            {
                character.crossover (u[0], u[1], mu);
                for (int i = 0; i < mu; ++i)
                {
                    if (amountMoving && i % osFactor == 0) character.setAmount (amounts[(size_t) i]);
                    double a = u[0][i], b = u[1][i];
                    if (useSc) character.processAt (i, a, b, &sc[0][i], &sc[1][i]);
                    else character.processAt (i, a, b);
                    u[0][i] = a; u[1][i] = b;
                }
            }
            if (osFactor > 1) os.down (w[0], w[1], m);

            // limiter, output gain, bypass
            for (int i = 0; i < m; ++i)
            {
                double a = w[0][i], b = w[1][i];
                const double g = limGain.next();
                if (! p.limiterBypass) { a *= g; b *= g; limiter.process (a, b); }
                const double og = outGain.next();
                a *= og; b *= og;
                double dl = l[start + i], dr = r[start + i];
                bypassDelay.process (dl, dr);
                if (p.bypass) { a = dl; b = dr; }
                l[start + i] = (float) a; r[start + i] = (float) b;
            }
        }
        charSwitch.advance (len);
        eqSwitch.advance (len);
    }
}
}
