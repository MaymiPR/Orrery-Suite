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
// One I/O meter channel as the reference draws it (MEASUREMENTS.md 6): a green bar on the RMS, a mint run above it up
// to the fast peak, a held peak LED; the readouts beside it.
struct LevelTap
{
    std::atomic<float> peak { 0.0f };     // highest sample since the editor last took it (linear): is audio playing?
    std::atomic<float> blockPeak { 0.0f };// the latest block's sample peak (linear): the peak readout samples it
    std::atomic<float> meanSquare { 0.0f };// latest mean square for the RMS readout (plain; one-pole on x^2, 89 ms)
    std::atomic<float> bar { 0.0f };      // latest RMS for the green bar (linear, plain; one-pole on x^2, 150 ms)
    std::atomic<float> fast { 0.0f };     // highest fast peak (|x| follower, 1 ms / 50 ms) since the editor took it
    void push (float blockPk, float blockMeanSquare, float blockBar, float blockFast)
    {
        auto raise = [] (std::atomic<float>& a, float v) { float p = a.load(); while (v > p && ! a.compare_exchange_weak (p, v)) {} };
        raise (peak, blockPk);
        raise (fast, blockFast);
        blockPeak.store (blockPk);
        meanSquare.store (blockMeanSquare);
        bar.store (blockBar);
    }
    float takePeak() { return peak.exchange (0.0f); }
    float takeFast() { return fast.exchange (0.0f); }
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

    // Compare A -> B (ours, as Cradle's Orion has it): two complete settings in one instance. The active one lives in the
    // parameters; the other is kept here. Switching stores the active one and loads the other; copy() copies the active
    // one over the other. A setting never used yet starts as a copy of the one in use when it is first selected.
    int compareSlot() const { return slot; }
    void selectCompareSlot (int);
    void copyCompareSlot();

    // host-only state (not part of presets, as in the reference): side-chain source and the editor's zoom
    std::atomic<bool> sidechainExternal { false };
    std::atomic<float> uiScale { 1.0f };
    std::atomic<float> hostBpm { 0.0f };     // the project's tempo, as the host last gave it (0: it gives none)
    std::atomic<bool> ioLink { false };      // I/O link (ours): the output gain mirrors the input gain; saved with the session

    // meters: input / output L R, multiband gain reduction (high, mid, low) and limiter L R, positive dB
    ap::LevelTap inL, inR, outL, outR;
    std::atomic<float> bandGr[3] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> limGr[2] { 0.0f, 0.0f };
    std::atomic<int> blocksProcessed { 0 };

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the settings page's oversampling ratio
    juce::ValueTree captureSlot() const;
    void applySlot (const juce::ValueTree&);
    juce::ValueTree slots[2];
    bool slotUsed[2] { true, false };
    int slot = 0;

    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::atomic<float>* raw[11] {};       // every parameter's current value, in ap::id order
    ap::dsp::Engine engine;
    juce::AudioBuffer<float> meterBuf;    // the input meters' copy (after the input gain)
    double preparedRate = 0.0;
    int preparedBlock = 0;
    float meterState[4][3] {};      // per channel (in L, in R, out L, out R): readout mean square, bar mean square, fast peak
    float rmsCoef = 0.0f, barCoef = 0.0f, fastAttack = 0.0f, fastRelease = 0.0f;
    void measure (const juce::AudioBuffer<float>&, ap::LevelTap&, ap::LevelTap&, float* state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiparticleProcessor)
};
