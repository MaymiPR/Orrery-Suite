#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace ap
{
AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto gain = [&] (const char* pid, const char* name, float range)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (-range, range), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals)));
    };
    auto toggle = [&] (const char* pid, const char* name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 1.0f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals)));
    };
    gain (id::inputGain, "Input Gain", 12.0f);
    gain (id::outputGain, "Output Gain", 12.0f);
    gain (id::eqLow, "EQ Low Gain", 6.0f);
    gain (id::eqMid, "EQ Mid Gain", 6.0f);
    gain (id::eqHigh, "EQ High Gain", 6.0f);
    toggle (id::eqBypass, "EQ Bypass");
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::character, 1 }, "Character", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals)));
    toggle (id::characterBypass, "Character Bypass");
    NormalisableRange<float> limRange (0.0f, 12.0f,
                                       [] (float, float, float v) { return limiterDbFromNorm (v); },
                                       [] (float, float, float db) { return limiterNormFromDb (db); });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::limiterGain, 1 }, "Limiter Input Gain", limRange, limiterDefaultDb,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals)));
    toggle (id::limiterBypass, "Limiter Bypass");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}
}

AntiparticleProcessor::AntiparticleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "TheAntiparticle", ap::createLayout())
{
    bypassParam = apvts.getParameter (ap::id::bypass);
    const char* ids[11] { ap::id::inputGain, ap::id::outputGain, ap::id::eqLow, ap::id::eqMid, ap::id::eqHigh, ap::id::eqBypass,
                          ap::id::character, ap::id::characterBypass, ap::id::limiterGain, ap::id::limiterBypass, ap::id::bypass };
    for (int i = 0; i < 11; ++i) raw[i] = apvts.getRawParameterValue (ids[i]);
    ap::Settings::get().addChangeListener (this);
}

AntiparticleProcessor::~AntiparticleProcessor() { ap::Settings::get().removeChangeListener (this); }

void AntiparticleProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // a new oversampling ratio: prepare again (and tell the host the new latency)
    if (preparedRate > 0.0 && ap::dsp::tuning().forceOversampling <= 0 && ap::Settings::get().oversampling() != engine.oversamplingFactor())
    {
        suspendProcessing (true);
        prepareToPlay (preparedRate, preparedBlock);
        suspendProcessing (false);
    }
}

bool AntiparticleProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // stereo only, like the reference (mono -> mono is refused); the side-chain bus is stereo or off
    if (l.getMainInputChannelSet() != AudioChannelSet::stereo() || l.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.inputBuses[1];
        if (! sc.isDisabled() && sc != AudioChannelSet::stereo()) return false;
    }
    return true;
}

void AntiparticleProcessor::prepareToPlay (double sampleRate, int block)
{
    preparedRate = sampleRate;
    preparedBlock = block;
    meterBuf.setSize (2, jmax (1, block));
    const auto& t = ap::dsp::tuning();
    engine.prepare (sampleRate, block, t.forceOversampling > 0 ? (int) t.forceOversampling : ap::Settings::get().oversampling(), t);
    rmsCoef = (float) std::exp (-1.0 / (0.3 * sampleRate));       // placeholder readout RMS window (MEASUREMENTS.md 6: open)
    // the meters as fitted to the reference's LEDs on a pink-noise ramp (MEASUREMENTS.md 6): the green bar is the plain
    // RMS over 150 ms, the mint run and the input target follow a fast peak (1 ms attack, 50 ms release)
    barCoef = (float) std::exp (-1.0 / (0.15 * sampleRate));
    fastAttack = 1.0f - (float) std::exp (-1.0 / (0.001 * sampleRate));
    fastRelease = (float) std::exp (-1.0 / (0.05 * sampleRate));
    for (auto& m : meterState) std::fill (std::begin (m), std::end (m), 0.0f);
    setLatencySamples (engine.latency());
}

void AntiparticleProcessor::measure (const AudioBuffer<float>& b, ap::LevelTap& l, ap::LevelTap& r, float* state)
{
    const int n = b.getNumSamples();
    ap::LevelTap* taps[2] { &l, &r };
    for (int c = 0; c < 2; ++c)
    {
        const float* x = b.getReadPointer (jmin (c, b.getNumChannels() - 1));
        float* st = state + 3 * c;
        float pk = 0.0f, ms = st[0], bar = st[1], fast = st[2], fastMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]), sq = x[i] * x[i];
            pk = jmax (pk, a);
            ms = sq + rmsCoef * (ms - sq);
            bar = sq + barCoef * (bar - sq);
            fast = a > fast ? fast + (a - fast) * fastAttack : fast * fastRelease;
            fastMax = jmax (fastMax, fast);
        }
        st[0] = ms; st[1] = bar; st[2] = fast;
        taps[c]->push (pk, std::sqrt (2.0f * ms), std::sqrt (bar), fastMax);
    }
}

void AntiparticleProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    if (main.getNumChannels() < 2) return;

    ap::dsp::Params p;
    p.inputGainDb = raw[0]->load(); p.outputGainDb = raw[1]->load();
    if (ioLink.load()) p.outputGainDb = -p.inputGainDb;      // the I/O link (ours): output = -input, always
    p.eqLowDb = raw[2]->load(); p.eqMidDb = raw[3]->load(); p.eqHighDb = raw[4]->load();
    p.eqBypass = raw[5]->load() >= 0.5f;
    p.amount = raw[6]->load();
    p.characterBypass = raw[7]->load() >= 0.5f;
    p.limiterGainDb = raw[8]->load();
    p.limiterBypass = raw[9]->load() >= 0.5f;
    p.bypass = bypassParam->getValue() >= 0.5f;
    p.sidechainExternal = sidechainExternal.load();

    const float* sc[2] { nullptr, nullptr };
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        if (side.getNumChannels() >= 2) { sc[0] = side.getReadPointer (0); sc[1] = side.getReadPointer (1); }
    }

    // input meters read the signal after the input gain
    {
        const float g = Decibels::decibelsToGain (p.inputGainDb);
        meterBuf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c) FloatVectorOperations::copyWithMultiply (meterBuf.getWritePointer (c), main.getReadPointer (c), p.bypass ? 1.0f : g, n);
        measure (meterBuf, inL, inR, &meterState[0][0]);
    }
    engine.process (main.getWritePointer (0), main.getWritePointer (1), sc[0], sc[1], n, p);
    measure (main, outL, outR, &meterState[2][0]);

    float gr[3];
    engine.takeBandGr (gr);
    // meters: the deepest values since the editor last took them (bands in the editor's order: high, mid, low)
    auto raise = [] (std::atomic<float>& a, float v) { float o = a.load(); while (v > o && ! a.compare_exchange_weak (o, v)) {} };
    for (int b = 0; b < 3; ++b) raise (bandGr[2 - b], p.characterBypass || p.bypass ? 0.0f : gr[b]);
    const float lg = engine.takeLimiterGr();
    for (auto& l : limGr) raise (l, p.limiterBypass || p.bypass ? 0.0f : lg);
    ++blocksProcessed;
}

void AntiparticleProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("sidechain", sidechainExternal.load() ? 1 : 0, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("ioLink", ioLink.load() ? 1 : 0, nullptr);
    state.setProperty ("preset", presets.currentName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AntiparticleProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            sidechainExternal = (int) tree.getProperty ("sidechain", 0) != 0;
            uiScale = (float) tree.getProperty ("uiScale", 1.0f);
            ioLink = (int) tree.getProperty ("ioLink", 0) != 0;
            presets.setCurrentName (tree.getProperty ("preset", "default").toString());
            apvts.replaceState (tree);
        }
}

AudioProcessorEditor* AntiparticleProcessor::createEditor() { return new AntiparticleEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AntiparticleProcessor(); }
