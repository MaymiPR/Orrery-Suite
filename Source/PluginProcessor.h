#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Params.h"
#include "Presets.h"
#include "Settings.h"
#include "dsp/Engine.h"

namespace ap
{
// Levels the editor reads (audio thread writes, message thread reads and resets).
struct LevelTap
{
    std::atomic<float> peak { 0.0f };     // highest sample since the editor last took it (linear)
    std::atomic<float> rms { 0.0f };      // latest RMS (linear, AES: a sine reads its peak)
    std::atomic<float> follower { 0.0f }; // latest block's peak follower (0.2 ms attack, 0.3 s release): input target
    void push (float blockPeak, float blockRms, float blockFollower)
    {
        follower.store (blockFollower);
        float p = peak.load();
        while (blockPeak > p && ! peak.compare_exchange_weak (p, blockPeak)) {}
        rms.store (blockRms);
    }
    float takePeak() { return peak.exchange (0.0f); }
};
}

class AntiparticleProcessor : public juce::AudioProcessor,
                              private juce::ChangeListener
{
public:
    AntiparticleProcessor();
    ~AntiparticleProcessor() override;

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
    ap::PresetManager presets { apvts };

    // host-only state (not part of presets, as in the reference): side-chain source and the editor's zoom
    std::atomic<bool> sidechainExternal { false };
    std::atomic<float> uiScale { 1.0f };

    // meters: input / output L R, multiband gain reduction (high, mid, low) and limiter L R, positive dB
    ap::LevelTap inL, inR, outL, outR;
    std::atomic<float> bandGr[3] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> limGr[2] { 0.0f, 0.0f };
    std::atomic<int> blocksProcessed { 0 };

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the settings page's oversampling ratio

    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::atomic<float>* raw[11] {};       // every parameter's current value, in ap::id order
    ap::dsp::Engine engine;
    juce::AudioBuffer<float> meterBuf;    // the input meters' copy (after the input gain)
    double preparedRate = 0.0;
    int preparedBlock = 0;
    float rmsState[4] {};           // mean-square followers (in L, in R, out L, out R)
    float followState[4] {};        // peak followers (same order)
    float rmsCoef = 0.0f, followAttack = 0.0f, followRelease = 0.0f;
    void measure (const juce::AudioBuffer<float>&, ap::LevelTap&, ap::LevelTap&, float* msState, float* followerState);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiparticleProcessor)
};
