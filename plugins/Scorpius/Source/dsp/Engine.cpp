#include <functional>
#include "Engine.h"

namespace sc::dsp
{
void Engine::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    for (auto* b : { &bufL, &bufR, &wetL, &wetR, &chainL, &chainR, &expGain, &keyBufL, &keyBufR }) b->assign ((size_t) std::max (1, maxBlock), 0.0);
    for (auto* p : { &eqPower, &harmPower, &compPower, &drivePower, &liftPower }) p->mix.setLength ((int) std::lround (tuning().switchMs * 0.001 * fs));
    eq.prepare (fs);
    sub.prepare (fs);
    loud.prepare (fs);
    low.prepare (fs);
    grit.prepare (maxBlock);
    harm.prepare (maxBlock);
    comp.prepare (fs, maxBlock);
    drive.prepare();
    lift.prepare (maxBlock);
    expander.prepare (fs);
    for (auto* a : { &harmAmount, &compAmount, &liftAmount }) a->setLength ((int) std::lround (tuning().amountRampMs * 0.001 * fs));
    for (auto& w : parWeight) w.setLength ((int) std::lround (tuning().parRampMs * 0.001 * fs));
    reset();
}

void Engine::reset()
{
    eq.reset();
    sub.reset();
    loud.reset();
    low.reset();
    grit.reset();
    harm.reset();
    comp.reset();
    drive.reset();
    lift.reset();
    expander.reset();
    first = true;
}

// runs a module on the buffer, crossfading between its input and its output while its power changes
void Engine::runModule (Power& power, bool on, int n, const std::function<void (double*, double*, int)>& fn)
{
    if (first) power.mix.jump (on ? 1.0 : 0.0); else power.mix.set (on ? 1.0 : 0.0);
    if (! power.active()) return;
    if (! power.mix.moving() && power.mix.value >= 1.0)
    {
        fn (bufL.data(), bufR.data(), n);
        return;
    }
    std::copy (bufL.begin(), bufL.begin() + n, wetL.begin());
    std::copy (bufR.begin(), bufR.begin() + n, wetR.begin());
    fn (wetL.data(), wetR.data(), n);
    for (int i = 0; i < n; ++i)
    {
        const double m = power.mix.next();
        bufL[(size_t) i] += m * (wetL[(size_t) i] - bufL[(size_t) i]);
        bufR[(size_t) i] += m * (wetR[(size_t) i] - bufR[(size_t) i]);
    }
}

void Engine::process (float* l, float* r, int n, const Params& p, const float* keyL, const float* keyR)
{
    const std::pair<Ramp*, double> amounts[] { { &harmAmount, p.harmAmount }, { &compAmount, p.compAmount }, { &liftAmount, p.liftAmount } };
    for (auto& a : amounts)
        if (first) a.first->jump (a.second); else a.first->set (a.second);
    if (keyL != nullptr)
        for (int i = 0; i < n; ++i) { keyBufL[(size_t) i] = keyL[i]; keyBufR[(size_t) i] = keyR[i]; }
    for (int i = 0; i < n; ++i) { bufL[(size_t) i] = l[i]; bufR[(size_t) i] = r[i]; }
    // the expander of the drive / harmonics / compression / lift group, at its input
    if (p.driveOn || p.harmOn || p.compOn || p.liftOn || harmPower.active() || compPower.active() || drivePower.active() || liftPower.active())
    {
        expander.gains (bufL.data(), bufR.data(), expGain.data(), n);
        for (int i = 0; i < n; ++i) { bufL[(size_t) i] *= expGain[(size_t) i]; bufR[(size_t) i] *= expGain[(size_t) i]; }
    }
    else expander.reset();
    const int colour = p.driveColor; const bool hiPass = p.hiPass;
    runModule (drivePower, p.driveOn, n, [this, colour, hiPass] (double* a, double* b, int m) { drive.process (a, b, m, colour, hiPass); });
    harm.setMode (p.harmFreq);
    runModule (harmPower, p.harmOn, n, [this] (double* a, double* b, int m) { harm.process (a, b, m, harmAmount); });
    const int compMode = p.compMode;
    const bool ext = keyL != nullptr;
    compReduction = 0;
    runModule (compPower, p.compOn, n, [this, compMode, ext] (double* a, double* b, int m)
               { compReduction = comp.process (a, b, m, compMode, compAmount, ext ? keyBufL.data() : nullptr, ext ? keyBufR.data() : nullptr); });
    runModule (liftPower, p.liftOn, n, [this] (double* a, double* b, int m) { lift.process (a, b, m, liftAmount); });
    eq.setGains (p.eq, first);
    runModule (eqPower, p.eqOn, n, [this] (double* a, double* b, int m) { eq.process (a, b, m); });

    // the parallels: each adds its weighted contribution, all listening to the chain's output
    for (int k = 0; k < 4; ++k)
    {
        const double w = p.parOn[k] ? faderTaper (p.parLevel[k]) : 0.0;
        if (first) parWeight[k].jump (w); else parWeight[k].set (w);
    }
    if (parWeight[0].value > 0 || parWeight[0].moving() || parWeight[1].value > 0 || parWeight[2].value > 0 || parWeight[3].value > 0
        || parWeight[1].moving() || parWeight[2].moving() || parWeight[3].moving())
    {
        std::copy (bufL.begin(), bufL.begin() + n, chainL.begin());
        std::copy (bufR.begin(), bufR.begin() + n, chainR.begin());
        if (parWeight[0].value > 0 || parWeight[0].moving()) sub.process (chainL.data(), chainR.data(), bufL.data(), bufR.data(), n, parWeight[0]);
        else sub.reset();
        if (parWeight[1].value > 0 || parWeight[1].moving()) low.process (chainL.data(), chainR.data(), bufL.data(), bufR.data(), n, parWeight[1]);
        else low.reset();
        if (parWeight[2].value > 0 || parWeight[2].moving()) loud.process (chainL.data(), chainR.data(), bufL.data(), bufR.data(), n, parWeight[2]);
        else loud.reset();
        if (parWeight[3].value > 0 || parWeight[3].moving()) grit.process (chainL.data(), chainR.data(), bufL.data(), bufR.data(), n, parWeight[3]);
        else grit.reset();
    }
    first = false;
    for (int i = 0; i < n; ++i) { l[i] = (float) bufL[(size_t) i]; r[i] = (float) bufR[(size_t) i]; }
}
}
