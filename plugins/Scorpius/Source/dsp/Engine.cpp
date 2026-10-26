#include <functional>
#include "Engine.h"

namespace sc::dsp
{
void Engine::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    for (auto* b : { &bufL, &bufR, &wetL, &wetR, &chainL, &chainR }) b->assign ((size_t) std::max (1, maxBlock), 0.0);
    for (auto* p : { &eqPower }) p->mix.setLength ((int) std::lround (tuning().switchMs * 0.001 * fs));
    eq.prepare (fs);
    sub.prepare (fs);
    for (auto& w : parWeight) w.setLength ((int) std::lround (tuning().parRampMs * 0.001 * fs));
    reset();
}

void Engine::reset()
{
    eq.reset();
    sub.reset();
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

void Engine::process (float* l, float* r, int n, const Params& p)
{
    for (int i = 0; i < n; ++i) { bufL[(size_t) i] = l[i]; bufR[(size_t) i] = r[i]; }
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
    }
    first = false;
    for (int i = 0; i < n; ++i) { l[i] = (float) bufL[(size_t) i]; r[i] = (float) bufR[(size_t) i]; }
}
}
