#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Params.h"
#include "Presets.h"
#include "shared/dsp/LevelMeter.h"

// Scorpius: the recreation of Cradle's Orion (RESEARCH.md). Phase 2: the parameters, presets, the A / B compare and
// the meters are complete; the audio passes through the input and output gains only (the modules follow in Phase 4).
class ScorpiusProcessor : public juce::AudioProcessor
{
public:
    ScorpiusProcessor();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    sc::PresetManager presets { apvts };

    // Compare A -> B: two complete settings in one instance (as the reference). The active one lives in the parameters;
    // the other is kept here. Switching stores the active one and loads the other; copy() copies the active one over
    // the other (the arrow: A -> B while A is active, B -> A while B is). A setting never used yet starts as a copy of
    // the one in use when it is first selected (measured: B shows A's changes until B itself is changed).
    int compareSlot() const { return slot; }
    void selectCompareSlot (int);
    void copyCompareSlot();

    // host-only state, saved with the session: the editor's zoom and the particle animation (the reference keeps both
    // in its state's prefs)
    std::atomic<float> uiScale { 1.0f };
    std::atomic<bool> animation { true };
    std::atomic<bool> ioLink { false };      // I/O link (ours, as in The Antiparticle): the output gain mirrors the input gain

    // meters: input (after the input gain) and output, L R; the compression module's gain reduction (positive dB)
    orrery::LevelMeter inMeter, outMeter;
    std::atomic<float> compGr { 0.0f };
    std::atomic<int> blocksProcessed { 0 };

private:
    juce::ValueTree captureSlot() const;
    void applySlot (const juce::ValueTree&);

    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::atomic<float>* inGainRaw = nullptr;
    std::atomic<float>* outGainRaw = nullptr;
    juce::AudioBuffer<float> meterBuf;
    juce::SmoothedValue<float> inGain, outGain;
    juce::ValueTree slots[2];
    bool slotUsed[2] { true, false };
    int slot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorpiusProcessor)
};
