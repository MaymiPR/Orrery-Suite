#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace sc
{
namespace
{
// the reference shows every value with three decimals ("0.800", "-12.000", "80.000")
String threeDecimals (float v, int) { return String (v, 3); }
}

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals);
    auto gain = [&] (const char* pid, const char* name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (-12.0f, 12.0f), 0.0f, attrs));
    };
    auto onOff = [&] (const char* pid, const char* name, bool on)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 1.0f), on ? 1.0f : 0.0f, attrs));
    };
    auto amount = [&] (const char* pid, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, attrs));
    };
    gain (id::inputGain, "Input Gain");
    onOff (id::driveOn, "Drive On/Off", true);
    onOff (id::driveHiPass, "Drive Hi-Pass", true);
    onOff (id::harmOn, "Harm On/Off", true);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::harmFreq, 1 }, "Harm Freq", NormalisableRange<float> (50.0f, 80.0f, 30.0f), 80.0f, attrs));
    amount (id::harmAmount, "Harm Amount", 0.8f);
    onOff (id::compOn, "Comp On/Off", true);
    amount (id::compAmount, "Comp Amount", 0.9f);
    onOff (id::liftOn, "Lift On/Off", true);
    amount (id::liftAmount, "Lift Amount", 0.8f);
    onOff (id::eqOn, "EQ On/Off", false);
    const char* eqNames[5] { "EQ 40Hz", "EQ 100Hz", "EQ 1kHz", "EQ 3kHz", "EQ 10kHz" };
    for (int b = 0; b < 5; ++b)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b], 1 }, eqNames[b], NormalisableRange<float> (0.0f, 6.0f), 0.0f, attrs));
    const char* parNames[4][2] { { "Par Sub On/Off", "Par Sub Level" }, { "Par Low On/Off", "Par Low Level" },
                                 { "Par Loud On/Off", "Par Loud Level" }, { "Par Grit On/Off", "Par Grit Level" } };
    for (int k = 0; k < 4; ++k)
    {
        onOff (parallels[k].on, parNames[k][0], false);
        amount (parallels[k].level, parNames[k][1], 0.0f);
    }
    gain (id::outputGain, "Output Gain");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::driveColor, 1 }, "Drive Color", StringArray { "a", "b", "c" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::compMode, 1 }, "Comp Mode", StringArray { "a", "b" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}
}

ScorpiusProcessor::ScorpiusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Scorpius", sc::createLayout())
{
    bypassParam = apvts.getParameter (sc::id::bypass);
    inGainRaw = apvts.getRawParameterValue (sc::id::inputGain);
    outGainRaw = apvts.getRawParameterValue (sc::id::outputGain);
    slots[0] = slots[1] = captureSlot();
}

bool ScorpiusProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // stereo only, like the reference (mono -> mono is refused; no side-chain)
    return l.getMainInputChannelSet() == AudioChannelSet::stereo() && l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void ScorpiusProcessor::prepareToPlay (double sampleRate, int block)
{
    meterBuf.setSize (2, jmax (1, block));
    inMeter.prepare (sampleRate);
    outMeter.prepare (sampleRate);
    for (auto* s : { &inGain, &outGain }) s->reset (sampleRate, 0.01);
    inGain.setCurrentAndTargetValue (Decibels::decibelsToGain (inGainRaw->load()));
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (outGainRaw->load()));
}

void ScorpiusProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    if (main.getNumChannels() < 2) return;
    const bool bypassed = bypassParam->getValue() >= 0.5f;

    inGain.setTargetValue (bypassed ? 1.0f : Decibels::decibelsToGain (inGainRaw->load()));
    outGain.setTargetValue (bypassed ? 1.0f : Decibels::decibelsToGain (outGainRaw->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        main.getWritePointer (0)[i] *= g;
        main.getWritePointer (1)[i] *= g;
    }
    // the input meters read the signal after the input gain
    inMeter.measure (main);
    // (the modules: Phase 4)
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        main.getWritePointer (0)[i] *= g;
        main.getWritePointer (1)[i] *= g;
    }
    outMeter.measure (main);
    compGr = 0.0f;
    ++blocksProcessed;
}

//==============================================================================
ValueTree ScorpiusProcessor::captureSlot() const
{
    ValueTree t ("SLOT");
    for (auto* pid : sc::presetIds())
        if (auto* p = apvts.getParameter (pid)) t.setProperty (pid, p->getValue(), nullptr);
    t.setProperty ("preset", presets.currentName(), nullptr);
    return t;
}

void ScorpiusProcessor::applySlot (const ValueTree& t)
{
    for (auto* pid : sc::presetIds())
        if (auto* p = apvts.getParameter (pid))
            if (t.hasProperty (pid))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost ((float) t.getProperty (pid));
                p->endChangeGesture();
            }
    presets.setCurrentName (t.getProperty ("preset", presets.currentName()).toString());
}

void ScorpiusProcessor::selectCompareSlot (int s)
{
    s = jlimit (0, 1, s);
    if (s == slot) return;
    slots[slot] = captureSlot();
    slot = s;
    applySlot (slots[slot]);
}

void ScorpiusProcessor::copyCompareSlot()
{
    slots[slot] = captureSlot();
    slots[1 - slot] = slots[slot].createCopy();
}

void ScorpiusProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("animation", animation.load() ? 1 : 0, nullptr);
    state.setProperty ("preset", presets.currentName(), nullptr);
    state.setProperty ("compare", slot, nullptr);
    // the inactive compare slot (the active one is the parameters themselves)
    state.removeChild (state.getChildWithName ("SLOT"), nullptr);
    state.appendChild (slots[1 - slot].createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ScorpiusProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            uiScale = (float) tree.getProperty ("uiScale", 1.0f);
            animation = (int) tree.getProperty ("animation", 1) != 0;
            presets.setCurrentName (tree.getProperty ("preset", sc::PresetManager::defaultName()).toString());
            slot = jlimit (0, 1, (int) tree.getProperty ("compare", 0));
            auto other = tree.getChildWithName ("SLOT");
            tree.removeChild (other, nullptr);
            apvts.replaceState (tree);
            slots[slot] = captureSlot();
            slots[1 - slot] = other.isValid() ? other.createCopy() : slots[slot].createCopy();
        }
}

AudioProcessorEditor* ScorpiusProcessor::createEditor() { return new ScorpiusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ScorpiusProcessor(); }
