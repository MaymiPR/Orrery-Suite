// Unit tests for The Antiparticle: the DSP blocks (Source/dsp) and the processor. Run: AntiparticleTests (exit code 0 =
// all passed). The match with the reference itself is the null-test suite (nulltest/nulltest.py, NULLTESTS.md).
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "PluginProcessor.h"
#include "dsp/Engine.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

using namespace ap::dsp;

static double db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }

// gain of a linear system at f, from its response to a long sine (lock-in over the second half)
template <typename F>
static double gainAt (F&& process, double f, double fs, int n = 48000)
{
    std::vector<double> y ((size_t) n);
    for (int i = 0; i < n; ++i) y[(size_t) i] = process (std::sin (2 * kPi * f * i / fs));
    double re = 0, im = 0;
    for (int i = n / 2; i < n; ++i) { re += y[(size_t) i] * std::cos (2 * kPi * f * i / fs); im += y[(size_t) i] * std::sin (2 * kPi * f * i / fs); }
    return 2.0 * std::hypot (re, im) / (n / 2);
}

static void testBiquads()
{
    Biquad b;
    b.design (Biquad::Type::peak, 637, 0.5, 0.0, 48000);
    CHECK_NEAR (b.b0, 1.0, 1e-12); CHECK_NEAR (b.a1, b.b1, 1e-12);
    b.reset();
    b.design (Biquad::Type::lowShelf, 180, 0.7, 6.0, 48000);
    CHECK_NEAR (db (gainAt ([&] (double x) { return b.process (x, 0); }, 20, 48000, 96000)), 6.0, 0.05);
    b.reset();
    b.design (Biquad::Type::highShelf, 10000, 0.7, -6.0, 48000);
    CHECK_NEAR (db (gainAt ([&] (double x) { return b.process (x, 0); }, 20000, 48000)), -6.0, 0.1);
}

static void testFracDelay()
{
    FracDelay d;
    d.prepare (151.27917);
    // the measured all-pass: a = 0.5636
    CHECK_NEAR (d.a, 0.5636, 1e-3);
    double l = 1, r = 1, sum = 0, moment = 0;
    for (int i = 0; i < 2000; ++i)
    {
        if (i > 0) l = r = 0;
        d.process (l, r);
        sum += l; moment += l * i;
    }
    CHECK_NEAR (sum, 1.0, 1e-9);                 // unity at DC
    CHECK_NEAR (moment / sum, 151.27917, 1e-6);  // group delay at DC
}

static void testFirBank()
{
    FirBank fb;
    fb.prepare (48000.0, 151.27917);
    // at 48 kHz it is the identified FIR exactly: compare with direct convolution
    std::mt19937 rng (7);
    std::normal_distribution<double> nd;
    std::vector<double> x (3000);
    for (auto& v : x) v = nd (rng);
    double maxErr = 0;
    for (int i = 0; i < (int) x.size(); ++i)
    {
        double out[3], dummy[3];
        fb.process (0, x[(size_t) i], out);
        fb.process (1, 0.0, dummy);
        for (int b = 0; b < 3; ++b)
        {
            double ref = 0;
            for (int k = 0; k < bandTaps48k && k <= i; ++k) ref += bandIr48k[b][k] * x[(size_t) (i - k)];
            maxErr = std::max (maxErr, std::abs (ref - out[b]));
        }
    }
    CHECK (maxErr < 1e-9);
    // at 96 kHz (redesigned) the bands still sum to unity gain in the audio band
    FirBank f2;
    f2.prepare (96000.0, 302.53139);
    for (double f : { 50.0, 157.0, 700.0, 2676.0, 10000.0, 18000.0 })
    {
        f2.reset();
        const double g = gainAt ([&] (double v) { double o[3], q[3]; f2.process (0, v, o); f2.process (1, 0.0, q); return o[0] + o[1] + o[2]; }, f, 96000.0, 96000);
        CHECK_NEAR (db (g), 0.0, 0.25);
    }
}

static void testOversampler()
{
    // up then down with nothing in between: flat to 18 kHz, the reference's roll-off above (-0.53 dB at 20 kHz),
    // and the latency the engine assumes
    for (int factor : { 2, 4, 8 })
    {
        Oversampler os;
        os.prepare (factor, 16);
        CHECK_NEAR (os.latency(), factor == 2 ? 22.0 : factor == 4 ? 27.0 : 28.75, 1e-9);
        auto gainAt = [&] (double f)
        {
            os.reset();
            const int n = 48000;
            std::vector<double> y ((size_t) n);
            for (int i = 0; i < n; i += 16)
            {
                double l[16], r[16];
                for (int k = 0; k < 16; ++k) { l[k] = std::sin (2 * kPi * f * (i + k) / 48000.0); r[k] = 0; }
                double *ul, *ur;
                os.up (l, r, 16, ul, ur);
                os.down (l, r, 16);
                for (int k = 0; k < 16; ++k) y[(size_t) (i + k)] = l[k];
            }
            double re = 0, im = 0;
            for (int i = n / 2; i < n; ++i) { re += y[(size_t) i] * std::cos (2 * kPi * f * i / 48000.0); im += y[(size_t) i] * std::sin (2 * kPi * f * i / 48000.0); }
            return db (2.0 * std::hypot (re, im) / (n / 2));
        };
        CHECK_NEAR (gainAt (1000.0), 0.0, 0.02);
        CHECK_NEAR (gainAt (15000.0), 0.0, 0.05);
        CHECK (gainAt (20000.0) < -0.2);
    }
}

static AntiparticleProcessor* makeProcessor (double fs, int block)
{
    auto* p = new AntiparticleProcessor();
    p->setPlayConfigDetails (2, 2, fs, block);
    p->prepareToPlay (fs, block);
    return p;
}

static void setParam (AntiparticleProcessor& p, const char* name, float displayed)
{
    for (auto* q : p.getParameters())
        if (q->getName (64) == name) { q->setValueNotifyingHost (q->getValueForText (juce::String (displayed))); return; }
    CHECK (false);
}

static std::vector<std::vector<float>> render (AntiparticleProcessor& p, const std::vector<std::vector<float>>& in, int block)
{
    const int n = (int) in[0].size();
    std::vector<std::vector<float>> out (2, std::vector<float> ((size_t) n));
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += block)
    {
        const int m = std::min (block, n - pos);
        buf.setSize (2, m, false, false, true);
        for (int c = 0; c < 2; ++c) std::copy (in[(size_t) c].begin() + pos, in[(size_t) c].begin() + pos + m, buf.getWritePointer (c));
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) std::copy (buf.getReadPointer (c), buf.getReadPointer (c) + m, out[(size_t) c].begin() + pos);
    }
    return out;
}

static void testLatency()
{
    auto& t = tuning();
    t.forceOversampling = 1;
    for (auto [fs, expected] : { std::pair<double, int> { 44100.0, 179 }, { 48000.0, 189 }, { 96000.0, 357 } })
    {
        std::unique_ptr<AntiparticleProcessor> p (makeProcessor (fs, 512));
        CHECK (p->getLatencySamples() == expected);
    }
    for (auto [os, expected] : { std::pair<int, int> { 2, 212 }, { 4, 217 }, { 8, 219 } })
    {
        t.forceOversampling = os;
        std::unique_ptr<AntiparticleProcessor> p (makeProcessor (48000.0, 512));
        CHECK (p->getLatencySamples() == expected);
    }
    t.forceOversampling = 1;
}

static void testBypassIsDelayedInput()
{
    std::unique_ptr<AntiparticleProcessor> p (makeProcessor (48000.0, 256));
    setParam (*p, "Bypass", 1.0f);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    std::vector<std::vector<float>> x (2, std::vector<float> (4000));
    for (auto& c : x) for (auto& v : c) v = u (rng);
    const auto y = render (*p, x, 256);
    const int lat = p->getLatencySamples();
    double maxErr = 0;
    for (int i = lat; i < 4000; ++i)
        for (int c = 0; c < 2; ++c) maxErr = std::max (maxErr, (double) std::abs (y[(size_t) c][(size_t) i] - x[(size_t) c][(size_t) (i - lat)]));
    CHECK (maxErr < 1e-6);
}

static void testLimiterCeiling()
{
    std::unique_ptr<AntiparticleProcessor> p (makeProcessor (48000.0, 512));
    setParam (*p, "Limiter Input Gain", 12.0f);
    setParam (*p, "Input Gain", 6.0f);
    std::mt19937 rng (5);
    std::normal_distribution<float> nd (0.0f, 0.5f);
    std::vector<std::vector<float>> x (2, std::vector<float> (48000));
    for (auto& c : x) for (auto& v : c) v = nd (rng);
    const auto y = render (*p, x, 512);
    float peak = 0;
    for (auto& c : y) for (auto v : c) peak = std::max (peak, std::abs (v));
    CHECK (peak <= std::pow (10.0f, -0.1f / 20.0f) + 1e-5f);
    CHECK (std::isfinite (peak));
}

static void testCharacterSmallSignal()
{
    // amount 100 %, quiet tones: each band's identified small-signal gain (the limiter and EQ off)
    for (auto [f, band] : { std::pair<double, int> { 40.0, 0 }, { 700.0, 1 }, { 12000.0, 2 } })
    {
        std::unique_ptr<AntiparticleProcessor> p (makeProcessor (48000.0, 512));
        setParam (*p, "Limiter Bypass", 1.0f);
        setParam (*p, "EQ Bypass", 1.0f);
        std::vector<std::vector<float>> x (2, std::vector<float> (96000));
        for (int i = 0; i < 96000; ++i) x[0][(size_t) i] = x[1][(size_t) i] = (float) (0.001 * std::sin (2 * kPi * f * i / 48000.0));
        const auto y = render (*p, x, 512);
        double re = 0, im = 0;
        for (int i = 48000; i < 96000; ++i) { re += y[0][(size_t) i] * std::cos (2 * kPi * f * i / 48000.0); im += y[0][(size_t) i] * std::sin (2 * kPi * f * i / 48000.0); }
        CHECK_NEAR (db (2.0 * std::hypot (re, im) / 48000.0 / 0.001), bandGain48k[band][4], 0.15);
    }
}

static void testSidechain()
{
    // EXT: the side-chain drives the character's detectors. A loud main signal with a silent side-chain is not
    // compressed (its band gains stay at the small-signal values); a loud side-chain compresses a quiet main signal.
    auto runCase = [] (float mainDb, float scDb)
    {
        std::unique_ptr<AntiparticleProcessor> p (new AntiparticleProcessor());
        auto layout = p->getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        p->setBusesLayout (layout);
        p->sidechainExternal = true;
        p->setRateAndBufferSizeDetails (48000.0, 512);   // (setPlayConfigDetails would reset the bus layout)
        CHECK (p->getTotalNumInputChannels() == 4);
        p->prepareToPlay (48000.0, 512);
        setParam (*p, "Limiter Bypass", 1.0f);
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        double acc = 0; int cnt = 0;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (b * 512 + i) / 48000.0;
                const float m = (float) (std::pow (10.0, mainDb / 20.0) * std::sin (2 * kPi * 700 * t));
                const float sc = (float) (std::pow (10.0, scDb / 20.0) * std::sin (2 * kPi * 650 * t));
                buf.setSample (0, i, m); buf.setSample (1, i, m); buf.setSample (2, i, sc); buf.setSample (3, i, sc);
            }
            p->processBlock (buf, midi);
            if (b >= 150) for (int i = 0; i < 512; ++i) { acc += (double) buf.getSample (0, i) * buf.getSample (0, i); ++cnt; }
        }
        return db (std::sqrt (2.0 * acc / cnt)) - mainDb;   // the gain on the main tone (dB)
    };
    // (-20 dBFS: compressed by ~1.7 dB with INT; with EXT only the shaper acts, a few hundredths, as the reference
    // measured: 8.382 vs 8.441 dB, MEASUREMENTS.md 3.7)
    const double quietMain = runCase (-40.0, -100.0), loudMainSilentSc = runCase (-20.0, -100.0), loudSc = runCase (-40.0, -3.0);
    CHECK_NEAR (loudMainSilentSc, quietMain, 0.3);
    CHECK (loudSc < quietMain - 2.0);
}

static void testStateRoundTrip()
{
    std::unique_ptr<AntiparticleProcessor> a (makeProcessor (48000.0, 512));
    setParam (*a, "EQ Low Gain", 3.5f);
    setParam (*a, "Character", 0.75f);
    setParam (*a, "Limiter Input Gain", 8.0f);
    a->sidechainExternal = true;
    juce::MemoryBlock mb;
    a->getStateInformation (mb);
    std::unique_ptr<AntiparticleProcessor> b (makeProcessor (48000.0, 512));
    b->setStateInformation (mb.getData(), (int) mb.getSize());
    for (int i = 0; i < a->getParameters().size(); ++i)
        CHECK_NEAR (b->getParameters()[i]->getValue(), a->getParameters()[i]->getValue(), 1e-6);
    CHECK (b->sidechainExternal.load());
}

static void testSilenceAndExtremes()
{
    auto& t = tuning();
    for (int os : { 1, 2, 4, 8 })
    {
        t.forceOversampling = os;
        std::unique_ptr<AntiparticleProcessor> p (makeProcessor (48000.0, 333));
        setParam (*p, "Character", 1.0f);
        setParam (*p, "EQ High Gain", 6.0f);
        std::vector<std::vector<float>> x (2, std::vector<float> (24000, 0.0f));
        for (int i = 12000; i < 24000; ++i) x[0][(size_t) i] = x[1][(size_t) i] = (i / 40) % 2 ? 1.0f : -1.0f;   // full-scale square
        const auto y = render (*p, x, 333);
        bool finite = true;
        float quiet = 0;
        for (auto& c : y) for (int i = 0; i < 24000; ++i) { finite &= std::isfinite (c[(size_t) i]); if (i < 11000) quiet = std::max (quiet, std::abs (c[(size_t) i])); }
        CHECK (finite);
        CHECK (quiet < 1e-9f);
    }
    t.forceOversampling = 0;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testBiquads();
    testFracDelay();
    testFirBank();
    testOversampler();
    testLatency();
    testBypassIsDelayedInput();
    testLimiterCeiling();
    testCharacterSmallSignal();
    testSidechain();
    testStateRoundTrip();
    testSilenceAndExtremes();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
