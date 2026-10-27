#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Params.h"
#include "Presets.h"
#include "shared/dsp/LevelMeter.h"
#include "dsp/Visuals.h"
#include "dsp/Light.h"
#include "dsp/Engine.h"
#include "dsp/Resampler.h"
#include "dsp/ConverterFits.h"

// Scorpius: the recreation of Cradle's Orion (RESEARCH.md, MEASUREMENTS.md). The audio runs input gain -> the modules
// (dsp/Engine.h) -> output gain.
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
    std::atomic<int> particleStyle { 1 };    // the stripe: 0 the reference's particles, 1 ours (Scorpius)
    std::atomic<bool> ioLink { false };      // I/O link (ours, as in The Antiparticle): the output gain mirrors the input gain
    // Sidechain INT / EXT (ours, as The Antiparticle's top bar; the reference has no side-chain): EXT takes the
    // compressor's detector from the side-chain bus instead of the signal it compresses
    std::atomic<bool> sidechainExternal { false };

    // meters: input (after the input gain) and output, L R; the compression module's gain reduction (positive dB)
    orrery::LevelMeter inMeter, outMeter;
    std::atomic<float> compGr { 0.0f };
    sc::Visuals visuals;                     // what the circles follow (dsp/Visuals.h)
    sc::Light light;                         // what the stripe's light follows (dsp/Light.h)
    std::atomic<int> blocksProcessed { 0 };

private:
    juce::ValueTree captureSlot() const;
    void applySlot (const juce::ValueTree&);

    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::atomic<float>* inGainRaw = nullptr;
    std::atomic<float>* outGainRaw = nullptr;
    std::atomic<float>* compAmountRaw = nullptr;
    sc::dsp::Engine engine;
    sc::dsp::Params engineParams() const;
    void runEngine (float* l, float* r, int n, const float* keyL, const float* keyR);
    bool resampling = false;
    static std::pair<double, int> referenceDelay (double sampleRate);
    sc::dsp::Resampler toInner[2], keyToInner[2], toOuter[2];
    const sc::dsp::fits::Converters* ownConverters = nullptr;   // above 48 kHz: the reference's own (ConverterFits.h)
    std::vector<float> inner[2], keyInner[2], outer[2];
    std::atomic<float>* raw (const char* pid) const { return apvts.getRawParameterValue (pid); }
    juce::AudioBuffer<float> meterBuf;
    float inGainNow = 1.0f, outGainNow = 1.0f, gainCoef = 0.0f;
    std::vector<float> silence;            // the side-chain when EXT is chosen but the host leaves the bus off
    juce::ValueTree slots[2];
    bool slotUsed[2] { true, false };
    int slot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorpiusProcessor)
};
