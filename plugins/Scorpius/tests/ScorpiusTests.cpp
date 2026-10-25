// Unit tests for Scorpius: parameters, presets, the A / B compare, state and the pass-through of Phase 2. Run:
// ScorpiusTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include "PluginProcessor.h"

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

static void testPassThrough()
{
    ScorpiusProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    setReal (p, sc::id::inputGain, 6.0f);
    setReal (p, sc::id::outputGain, -12.0f);
    AudioBuffer<float> b (2, 512);
    MidiBuffer midi;
    float last = 0.0f;
    for (int block = 0; block < 20; ++block)          // past the 10 ms gain smoothing
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, 0.25f);
        p.processBlock (b, midi);
        last = b.getSample (0, 511);
    }
    CHECK_NEAR (Decibels::gainToDecibels (last / 0.25f), -6.0, 0.01);
    CHECK (p.blocksProcessed.load() == 20);
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    testParameters();
    testPresets();
    testCompareAndState();
    testPassThrough();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
