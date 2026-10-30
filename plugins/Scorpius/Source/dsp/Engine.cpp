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
    const int chunk = (int) std::lround (tuning().ctlChunk);
    for (auto* a : { &harmAmount, &compAmount, &liftAmount })
    {
        a->setLength ((int) std::lround ((chunk > 0 ? tuning().glideMs : tuning().amountRampMs) * 0.001 * fs));
        a->setChunked (chunk, (int) std::lround (tuning().paramRampMs * 0.001 * fs));
    }
    const bool parChunked = chunk > 0 && tuning().chunkPar > 0.5;
    for (auto& w : parWeight)
    {
        w.setLength ((int) std::lround ((parChunked ? tuning().glideMs : tuning().parRampMs) * 0.001 * fs));
        w.setChunked (parChunked ? chunk : 0, (int) std::lround (tuning().paramRampMs * 0.001 * fs));
    }
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

// a module's power for this block: switching at the block's start, as the reference does (Cradle's framework: with
// host blocks under a control chunk the switch waits until a whole chunk has gathered, 128 - n rounded up to whole
// blocks; MEASUREMENTS.md 12), or crossfading (switchMs > 0); switched on, it restarts from silence
void Engine::switchModule (Power& power, bool on, int n, const std::function<void()>& restart)
{
    const auto& t = tuning();
    power.bit = &power == &drivePower ? 1 : &power == &harmPower ? 2 : &power == &compPower ? 4 : &power == &liftPower ? 8 : 16;
    const int chunk = (int) std::lround (t.ctlChunk);
    if (first) { power.on = power.target = on; power.wait = 0; }
    else if (on != power.target)
    {
        power.target = on;
        power.wait = chunk > 0 && n < chunk ? (long) ((chunk - n + n - 1) / n) * n : 0;
    }
    else if (power.wait > 0) power.wait -= n;
    if (power.wait <= 0 && power.on != power.target)
    {
        power.on = power.target;
        if (power.on && ((int) t.restartOnEnable & power.bit) != 0 && restart) restart();
    }
    if (first || t.switchMs <= 0) power.mix.jump (power.on ? 1.0 : 0.0); else power.mix.set (power.on ? 1.0 : 0.0);
}

// runs a module on the buffer, crossfading between its input and its output while its power changes
void Engine::runModule (Power& power, int n, const std::function<void (double*, double*, int)>& fn)
{
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
    {
        if (first) a.first->jump (a.second); else a.first->set (a.second);
        a.first->beginBlock (n);
    }
    if (keyL != nullptr)
        for (int i = 0; i < n; ++i) { keyBufL[(size_t) i] = keyL[i]; keyBufR[(size_t) i] = keyR[i]; }
    for (int i = 0; i < n; ++i) { bufL[(size_t) i] = l[i]; bufR[(size_t) i] = r[i]; }
    switchModule (drivePower, p.driveOn, n, [this] { drive.reset(); });
    harm.setMode (p.harmFreq);
    switchModule (harmPower, p.harmOn, n, [this] { harm.reset(); });
    switchModule (compPower, p.compOn, n, [this] { comp.reset(); });
    switchModule (liftPower, p.liftOn, n, [this] { lift.reset(); });
    switchModule (eqPower, p.eqOn, n, [this] { eq.reset(); });
    // the expander of the drive / harmonics / compression / lift group, at its input, while one of them runs (not
    // while one waits to be switched on: the reference's expander starts closed with the module)
    if (harmPower.active() || compPower.active() || drivePower.active() || liftPower.active())
    {
        expander.gains (bufL.data(), bufR.data(), expGain.data(), n);
        for (int i = 0; i < n; ++i) { bufL[(size_t) i] *= expGain[(size_t) i]; bufR[(size_t) i] *= expGain[(size_t) i]; }
    }
    else expander.reset();
    const int colour = p.driveColor; const bool hiPass = p.hiPass;
    runModule (drivePower, n, [this, colour, hiPass] (double* a, double* b, int m) { drive.process (a, b, m, colour, hiPass); });
    runModule (harmPower, n, [this] (double* a, double* b, int m) { harm.process (a, b, m, harmAmount); });
    const int compMode = p.compMode;
    const bool ext = keyL != nullptr;
    compReduction = 0;
    runModule (compPower, n, [this, compMode, ext] (double* a, double* b, int m)
               { compReduction = comp.process (a, b, m, compMode, compAmount, ext ? keyBufL.data() : nullptr, ext ? keyBufR.data() : nullptr); });
    runModule (liftPower, n, [this] (double* a, double* b, int m) { lift.process (a, b, m, liftAmount); });
    eq.setGains (p.eq, first);
    eq.beginBlock (n);
    runModule (eqPower, n, [this] (double* a, double* b, int m) { eq.process (a, b, m); });

    // the parallels: each adds its weighted contribution, all listening to the chain's output
    for (int k = 0; k < 4; ++k)
    {
        const double w = p.parOn[k] ? faderTaper (p.parLevel[k]) : 0.0;
        if (first) parWeight[k].jump (w); else parWeight[k].set (w);
        parWeight[k].beginBlock (n);
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
