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
    inputGain = apvts.getRawParameterValue (ap::id::inputGain);
    outputGain = apvts.getRawParameterValue (ap::id::outputGain);
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

void AntiparticleProcessor::prepareToPlay (double sampleRate, int)
{
    inGainSmoothed.reset (sampleRate, 0.02);
    outGainSmoothed.reset (sampleRate, 0.02);
    inGainSmoothed.setCurrentAndTargetValue (Decibels::decibelsToGain (inputGain->load()));
    outGainSmoothed.setCurrentAndTargetValue (Decibels::decibelsToGain (outputGain->load()));
    rmsCoef = (float) std::exp (-1.0 / (0.3 * sampleRate));       // placeholder RMS window; measured in Phase 3
    std::fill (std::begin (rmsState), std::end (rmsState), 0.0f);
    setLatencySamples (0);
}

void AntiparticleProcessor::measure (const AudioBuffer<float>& b, int, ap::LevelTap& l, ap::LevelTap& r, float* state)
{
    const int n = b.getNumSamples();
    ap::LevelTap* taps[2] { &l, &r };
    for (int c = 0; c < 2; ++c)
    {
        const float* x = b.getReadPointer (jmin (c, b.getNumChannels() - 1));
        float pk = 0.0f, ms = state[c];
        for (int i = 0; i < n; ++i)
        {
            pk = jmax (pk, std::abs (x[i]));
            ms = x[i] * x[i] + rmsCoef * (ms - x[i] * x[i]);
        }
        state[c] = ms;
        taps[c]->push (pk, std::sqrt (2.0f * ms));
    }
}

void AntiparticleProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();

    if (bypassParam->getValue() >= 0.5f)
    {
        measure (main, 0, inL, inR, rmsState);
        measure (main, 0, outL, outR, rmsState + 2);
        ++blocksProcessed;
        return;
    }

    // Phase 2: gain staging only. EQ, character and limiter are Phase 4 (MEASUREMENTS.md).
    inGainSmoothed.setTargetValue (Decibels::decibelsToGain (inputGain->load()));
    outGainSmoothed.setTargetValue (Decibels::decibelsToGain (outputGain->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSmoothed.getNextValue();
        for (int c = 0; c < main.getNumChannels(); ++c) main.getWritePointer (c)[i] *= g;
    }
    measure (main, 0, inL, inR, rmsState);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSmoothed.getNextValue();
        for (int c = 0; c < main.getNumChannels(); ++c) main.getWritePointer (c)[i] *= g;
    }
    measure (main, 0, outL, outR, rmsState + 2);
    ++blocksProcessed;
}

void AntiparticleProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("sidechain", sidechainExternal.load() ? 1 : 0, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
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
            presets.setCurrentName (tree.getProperty ("preset", "default").toString());
            apvts.replaceState (tree);
        }
}

AudioProcessorEditor* AntiparticleProcessor::createEditor() { return new AntiparticleEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AntiparticleProcessor(); }
