// Unit tests for Scorpius: parameters, presets, the A / B compare, state, the pass-through of Phase 2 and the
// side-chain switch. Run:
// ScorpiusTests (exit code 0 = all passed);  ScorpiusTests bench: the learned models' CPU, part by part.
#include <cmath>
#include <cstdio>
#include "PluginProcessor.h"
#include "dsp/Convolver.h"
#include "dsp/LearnedModules.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

using namespace juce;

static float real (ScorpiusProcessor& p, const char* pid)
{
    auto* r = dynamic_cast<RangedAudioParameter*> (p.apvts.getParameter (pid));
    return r->convertFrom0to1 (r->getValue());
}

static void setReal (ScorpiusProcessor& p, const char* pid, float v)
{
    auto* r = dynamic_cast<RangedAudioParameter*> (p.apvts.getParameter (pid));
    r->setValueNotifyingHost (r->convertTo0to1 (v));
}

static void testParameters()
{
    ScorpiusProcessor p;
    // Orion's 26 host parameters with its names, in its order, then ours
    const char* names[] { "Input Gain", "Drive On/Off", "Drive Hi-Pass", "Harm On/Off", "Harm Freq", "Harm Amount", "Comp On/Off",
                          "Comp Amount", "Lift On/Off", "Lift Amount", "EQ On/Off", "EQ 40Hz", "EQ 100Hz", "EQ 1kHz", "EQ 3kHz",
                          "EQ 10kHz", "Par Sub On/Off", "Par Sub Level", "Par Low On/Off", "Par Low Level", "Par Loud On/Off",
                          "Par Loud Level", "Par Grit On/Off", "Par Grit Level", "Output Gain", "Drive Color", "Comp Mode", "Bypass" };
    const auto& params = p.getParameters();
    CHECK (params.size() == (int) std::size (names));
    for (int i = 0; i < jmin (params.size(), (int) std::size (names)); ++i)
        CHECK (params[i]->getName (64) == names[i]);
    // display text and ranges as the reference
    CHECK (params[0]->getText (0.0f, 64) == "-12.000");
    CHECK (params[4]->getText (0.0f, 64) == "50.000");
    CHECK (params[4]->getText (1.0f, 64) == "80.000");
    CHECK (params[11]->getText (0.5f, 64) == "3.000");
    // defaults = the preset "start me up"
    CHECK_NEAR (real (p, sc::id::harmAmount), 0.8, 1e-6);
    CHECK_NEAR (real (p, sc::id::compAmount), 0.9, 1e-6);
    CHECK_NEAR (real (p, sc::id::harmFreq), 80.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::eqOn), 0.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::compMode), 1.0, 1e-6);
}

static void testPresets()
{
    ScorpiusProcessor p;
    const auto all = p.presets.list();
    int factory = 0;
    for (auto& e : all) factory += e.factory ? 1 : 0;
    CHECK (factory == 9);
    for (auto& e : all)
        if (e.name == "fruity") p.presets.load (e);
    CHECK (p.presets.currentName() == "fruity");
    CHECK_NEAR (real (p, sc::id::inputGain), 9.12, 1e-3);
    CHECK_NEAR (real (p, sc::id::gritLevel), 0.189, 1e-3);
    CHECK_NEAR (real (p, sc::id::gritOn), 1.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::compMode), 0.0, 1e-6);
    // loading another factory preset resets everything it does not set
    for (auto& e : all)
        if (e.name == "low end theory") p.presets.load (e);
    CHECK_NEAR (real (p, sc::id::inputGain), 0.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::gritOn), 0.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::eq40), 6.0, 1e-4);
    CHECK_NEAR (real (p, sc::id::driveHiPass), 0.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::harmFreq), 50.0, 1e-6);
}

static void testCompareAndState()
{
    ScorpiusProcessor p;
    setReal (p, sc::id::liftAmount, 0.3f);
    p.copyCompareSlot();                      // A -> B
    p.selectCompareSlot (1);
    CHECK_NEAR (real (p, sc::id::liftAmount), 0.3, 1e-4);
    setReal (p, sc::id::liftAmount, 0.6f);    // B differs
    p.selectCompareSlot (0);
    CHECK_NEAR (real (p, sc::id::liftAmount), 0.3, 1e-4);
    p.selectCompareSlot (1);
    CHECK_NEAR (real (p, sc::id::liftAmount), 0.6, 1e-4);

    // the state keeps both settings and which one is active
    MemoryBlock mb;
    p.getStateInformation (mb);
    ScorpiusProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (q.compareSlot() == 1);
    CHECK_NEAR (real (q, sc::id::liftAmount), 0.6, 1e-4);
    q.selectCompareSlot (0);
    CHECK_NEAR (real (q, sc::id::liftAmount), 0.3, 1e-4);
}

// a setting never used yet starts as a copy of the one in use (as the reference: B shows A's changes)
static void testCompareFirstUse()
{
    ScorpiusProcessor p;
    setReal (p, sc::id::driveColor, 2.0f);
    setReal (p, sc::id::liftAmount, 0.25f);
    p.selectCompareSlot (1);
    CHECK_NEAR (real (p, sc::id::driveColor), 2.0, 1e-6);
    CHECK_NEAR (real (p, sc::id::liftAmount), 0.25, 1e-4);
    setReal (p, sc::id::liftAmount, 0.5f);
    p.selectCompareSlot (0);
    CHECK_NEAR (real (p, sc::id::liftAmount), 0.25, 1e-4);
    // the state remembers that B was used; a fresh instance's B is unused (copied on its first use)
    MemoryBlock mb;
    p.getStateInformation (mb);
    ScorpiusProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    q.selectCompareSlot (1);
    CHECK_NEAR (real (q, sc::id::liftAmount), 0.5, 1e-4);
}

static void modulesOff (ScorpiusProcessor& p)
{
    for (auto* pid : { sc::id::driveOn, sc::id::harmOn, sc::id::compOn, sc::id::liftOn, sc::id::eqOn })
        setReal (p, pid, 0.0f);
}

// the I/O link: the output gain follows the input gain inverted, in the audio whatever the output parameter says
static void testIoLink()
{
    ScorpiusProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    p.ioLink = true;
    modulesOff (p);
    setReal (p, sc::id::inputGain, 9.0f);
    setReal (p, sc::id::outputGain, 3.0f);
    AudioBuffer<float> b (2, 512);
    MidiBuffer midi;
    float last = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, 0.25f);
        p.processBlock (b, midi);
        last = b.getSample (1, 511);
    }
    CHECK_NEAR (Decibels::gainToDecibels (last / 0.25f), 0.0, 0.01);
    MemoryBlock mb;
    p.getStateInformation (mb);
    ScorpiusProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (q.ioLink.load());
}

// with every module off only the gains act (the reference is bit-exact so, MEASUREMENTS.md 1)
static void testPassThrough()
{
    ScorpiusProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    modulesOff (p);
    setReal (p, sc::id::inputGain, 6.0f);
    setReal (p, sc::id::outputGain, -12.0f);
    AudioBuffer<float> b (2, 512);
    MidiBuffer midi;
    float last = 0.0f;
    for (int block = 0; block < 20; ++block)          // past the gain smoothing
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, 0.25f);
        p.processBlock (b, midi);
        last = b.getSample (0, 511);
    }
    CHECK_NEAR (Decibels::gainToDecibels (last / 0.25f), -6.0, 0.01);
    CHECK (p.blocksProcessed.load() == 20);
}

// Sidechain EXT (ours): the compressor listens to the side-chain bus. A loud main signal with a silent side-chain is
// not compressed; a loud side-chain compresses a quiet one; INT ignores the side-chain. The switch is saved with the
// session and is not part of a compare setting.
static void testSidechain()
{
    auto runCase = [] (bool external, float mainDb, float sideDb)
    {
        ScorpiusProcessor p;
        auto layout = p.getBusesLayout();
        CHECK (layout.inputBuses.size() == 2);
        layout.inputBuses.getReference (1) = AudioChannelSet::stereo();
        CHECK (p.setBusesLayout (layout));
        p.sidechainExternal = external;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        AudioBuffer<float> b (4, 512);
        MidiBuffer midi;
        for (int block = 0; block < 40; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (block * 512 + i) / 48000.0;
                const float m = Decibels::decibelsToGain (mainDb) * (float) std::sin (MathConstants<double>::twoPi * 1000.0 * t);
                const float k = Decibels::decibelsToGain (sideDb) * (float) std::sin (MathConstants<double>::twoPi * 900.0 * t);
                b.setSample (0, i, m); b.setSample (1, i, m); b.setSample (2, i, k); b.setSample (3, i, k);
            }
            p.processBlock (b, midi);
        }
        return p.compGr.load();
    };
    CHECK (runCase (false, -6.0f, -100.0f) > 1.0f);              // INT: the main signal is compressed
    CHECK_NEAR (runCase (true, -6.0f, -100.0f), 0.0, 1e-6);      // EXT, silent side-chain: nothing
    CHECK (runCase (true, -60.0f, -6.0f) > 1.0f);                // EXT, loud side-chain: compressed
    CHECK_NEAR (runCase (false, -60.0f, -6.0f), 0.0, 1e-6);      // INT ignores the side-chain

    ScorpiusProcessor a;
    a.sidechainExternal = true;
    a.selectCompareSlot (1);
    CHECK (a.sidechainExternal.load());                          // not a compare setting
    MemoryBlock mb;
    a.getStateInformation (mb);
    ScorpiusProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (q.sidechainExternal.load());
}

// The stripe's light level (dsp/Light.h, measured on the reference): a jump is taken at once, a smooth ramp is not;
// music-like pulses fading out are followed; at a stop the level takes the loudest of the last 0.15 s and falls at
// 50 dB/s
static void testLight()
{
    sc::Light light;
    light.prepare (48000.0);
    AudioBuffer<float> b (2, 480);
    auto play = [&] (double seconds, std::function<float (double)> amplitude)
    {
        static double t = 0.0;
        for (int k = 0; k < (int) (seconds * 100.0); ++k)
        {
            for (int i = 0; i < 480; ++i, t += 1.0 / 48000.0)
            {
                const float v = amplitude (t) * (float) std::sin (MathConstants<double>::twoPi * 1000.0 * t);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            light.process (b);
        }
    };
    play (0.5, [] (double) { return Decibels::decibelsToGain (-20.0f); });
    CHECK_NEAR (light.levelDb(), -20.0, 0.1);
    CHECK (light.beatCount() == 1);
    // a slow ramp down to -60 dBFS (12.5 dB/s): the level stays
    double start = -1.0;
    play (3.2, [&] (double t) { if (start < 0) start = t; return Decibels::decibelsToGain ((float) (-20.0 - 12.5 * (t - start))); });
    CHECK_NEAR (light.levelDb(), -20.0, 0.1);
    // a jump up is taken
    play (0.3, [] (double) { return Decibels::decibelsToGain (-10.0f); });
    CHECK_NEAR (light.levelDb(), -10.0, 0.1);
    CHECK (light.beatCount() == 2);
    // pulses (4 a second) fading from -10 to -50 dBFS: followed
    start = -1.0;
    play (4.0, [&] (double t)
    {
        if (start < 0) start = t;
        const double u = t - start;
        return Decibels::decibelsToGain ((float) (-10.0 - 10.0 * u)) * (float) std::exp (-std::fmod (u, 0.25) / 0.08);
    });
    CHECK (light.levelDb() < -40.0f);
    // a stop: from the loudest of the last 0.15 s, falling at 50 dB/s
    play (0.5, [] (double) { return Decibels::decibelsToGain (-20.0f); });
    play (0.2, [] (double) { return 0.0f; });
    CHECK_NEAR (light.levelDb(), -20.0 - 50.0 * 0.2, 1.0);
}

// the long-filter convolver (no latency) against plain convolution
static void testConvolver()
{
    juce::Random rng (3);
    std::vector<float> taps (3000), x (6000);
    for (size_t k = 0; k < taps.size(); ++k) taps[k] = (rng.nextFloat() - 0.5f) * std::exp (-(float) k / 600.0f);
    for (auto& v : x) v = rng.nextFloat() - 0.5f;
    sc::dsp::Convolver c;
    c.prepare (taps);
    double err = 0, ref = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const float y = c.process (x[n]);
        double d = 0;
        for (size_t k = 0; k < taps.size() && k <= n; ++k) d += (double) taps[k] * x[n - k];
        err += (y - d) * (y - d); ref += d * d;
    }
    CHECK (10 * std::log10 (err / ref) < -100.0);
}

// the DSP: every preset's processing stays finite and bounded on loud noise; the latency at 44.1 kHz is the reference's
// (31 samples); EXT takes the compressor's detector from the side-chain
static void testEngine()
{
    for (double fs : { 48000.0, 44100.0 })
    {
        ScorpiusProcessor p;
        p.setRateAndBufferSizeDetails (fs, 512);
        p.prepareToPlay (fs, 512);
        if (fs == 44100.0) CHECK (p.getLatencySamples() == 31);
        for (auto* pid : { sc::id::eqOn, sc::id::subOn, sc::id::lowOn, sc::id::loudOn, sc::id::gritOn }) setReal (p, pid, 1.0f);
        for (auto* pid : { sc::id::subLevel, sc::id::lowLevel, sc::id::loudLevel, sc::id::gritLevel }) setReal (p, pid, 1.0f);
        setReal (p, sc::id::eq100, 6.0f);
        AudioBuffer<float> b (2, 512);
        MidiBuffer midi;
        juce::Random rng (9);
        bool finite = true; float most = 0;
        for (int block = 0; block < 200; ++block)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, 2.0f * (rng.nextFloat() - 0.5f));
            p.processBlock (b, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { finite = finite && std::isfinite (b.getSample (c, i)); most = std::max (most, std::abs (b.getSample (c, i))); }
        }
        CHECK (finite);
        CHECK (most < 16.0f);
    }
    // above 48 kHz the reference's identified converters run: its latency, and every module off passes a tone at its
    // level (blocks of odd sizes)
    for (double fs : { 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        ScorpiusProcessor p;
        p.setRateAndBufferSizeDetails (fs, 512);
        p.prepareToPlay (fs, 512);
        CHECK (p.getLatencySamples() == (fs == 88200.0 ? 46 : fs == 96000.0 ? 48 : fs == 176400.0 ? 75 : 80));
        for (auto* pid : { sc::id::driveOn, sc::id::harmOn, sc::id::compOn, sc::id::liftOn, sc::id::eqOn }) setReal (p, pid, 0.0f);
        MidiBuffer midi;
        double in = 0, out = 0; long long t = 0;
        for (int block = 0; block < 300; ++block)
        {
            const int n = 37 + (block * 97) % 400;
            AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i, ++t)
                for (int c = 0; c < 2; ++c) b.setSample (c, i, 0.25f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) t / fs));
            p.processBlock (b, midi);
            if (block > 50) for (int i = 0; i < n; ++i) { out += (double) b.getSample (0, i) * b.getSample (0, i); in += 0.25 * 0.25 / 2; }
        }
        CHECK_NEAR (10 * std::log10 (out / in), 0.0, 0.1);
    }
    // EXT: a loud side-chain makes the compressor reduce a quiet main signal
    auto gainWith = [] (bool ext)
    {
        ScorpiusProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (1) = AudioChannelSet::stereo();
        p.setBusesLayout (layout);
        p.sidechainExternal = ext;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        for (auto* pid : { sc::id::driveOn, sc::id::harmOn, sc::id::liftOn }) setReal (p, pid, 0.0f);
        setReal (p, sc::id::compAmount, 1.0f);
        AudioBuffer<float> b (4, 512);
        MidiBuffer midi;
        double acc = 0;
        for (int block = 0; block < 60; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (block * 512 + i) / 48000.0;
                const float m = 0.03f * (float) std::sin (MathConstants<double>::twoPi * 1000.0 * t);
                const float k = 0.9f * (float) std::sin (MathConstants<double>::twoPi * 900.0 * t);
                b.setSample (0, i, m); b.setSample (1, i, m); b.setSample (2, i, k); b.setSample (3, i, k);
            }
            p.processBlock (b, midi);
            if (block >= 40) for (int i = 0; i < 512; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
        }
        return 10 * std::log10 (acc / (20 * 512) / (0.03 * 0.03 / 2));
    };
    CHECK (gainWith (true) < gainWith (false) - 6.0);
}

// seconds of CPU per 60 s of stereo at 48 kHz, in blocks of 512
static void bench()
{
    using namespace sc::dsp;
    const int n = 60 * 48000, B = 512;
    std::vector<double> x ((size_t) n);
    Random rng (1);
    for (auto& v : x) v = 0.3 * (rng.nextDouble() * 2 - 1);
    auto time = [&] (const char* name, auto&& run)
    {
        std::vector<double> buf = x;
        const auto t0 = Time::getHighResolutionTicks();
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; i += B) run (buf.data() + i, std::min (B, n - i));
        std::printf ("%-24s %6.2f s\n", name, Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - t0));
    };
    {
        Convolver conv; conv.prepare (models::driveAhp.post);
        time ("convolver (8192 taps)", [&] (double* p, int m) { for (int i = 0; i < m; ++i) p[i] = conv.process ((float) p[i]); });
    }
    {
        LearnedWaveNet wn; wn.prepare (models::driveAhp);
        time ("drive WaveNet (whole)", [&] (double* p, int m) { wn.process (p, p, m); });
    }
    {
        LearnedWaveNet wn; wn.prepare (models::grit);
        time ("grit WaveNet (whole)", [&] (double* p, int m) { wn.process (p, p, m); });
    }
    {
        LearnedWaveNet wn; wn.prepare (models::lift);
        time ("lift WaveNet (whole)", [&] (double* p, int m) { wn.process (p, p, m); });
    }
    {
        ParallelFilter dry; dry.set (fits::liftDry);
        time ("lift dry filter", [&] (double* p, int m) { for (int i = 0; i < m; ++i) p[i] = dry.process (p[i]); });
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc > 1 && String (argv[1]) == "bench") { bench(); return 0; }
    testParameters();
    testPresets();
    testCompareAndState();
    testCompareFirstUse();
    testIoLink();
    testPassThrough();
    testSidechain();
    testLight();
    testConvolver();
    testEngine();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
