#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MetalKnob.h"
#include "ui/Visualizer.h"
#include "ui/Widgets.h"

// The editor is laid out in the reference's design pixels (888 x 359.5 at 100 %) and scaled as a whole by the
// UI Scale menu (50 ... 200 %).
class DevilsParticleEditor : public juce::AudioProcessorEditor,
                             private juce::Timer,
                             private juce::ChangeListener,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DevilsParticleEditor (DevilsParticleProcessor&);
    ~DevilsParticleEditor() override;

    void resized() override;
    void setUiScale (float);

    static constexpr float designW = 888.0f, designH = 359.5f;
    static constexpr float scales[] { 0.5f, 0.75f, 0.85f, 1.0f, 1.25f, 1.5f, 2.0f };

    // snapshot tool: freeze meters and visualiser to a given state
    void setTestLevels (float inDb, float outDb, bool audio);

private:
    struct Meter                                     // one channel of an I/O meter as drawn
    {
        float peakHoldDb = -200.0f, rmsHoldDb = -200.0f;   // the readouts (held until clicked)
        float barDb = -200.0f, peakDb = -200.0f;           // LED bar (RMS) and peak LED
        double peakAge = 0.0;
    };
    class Content : public juce::Component
    {
    public:
        explicit Content (DevilsParticleEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        DevilsParticleEditor& ed;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void parameterChanged (const juce::String&, float) override;
    void updateFromParams();
    void showPresetList();
    void showScaleList();
    void closePopups();
    void showSettings();
    void showSavePreset();
    void showAboutMenu();
    void showDialog (std::unique_ptr<dp::Dialog>);
    dp::MenuLook menuLook;
    void updateMeter (Meter&, dp::LevelTap&, float dt);

    void paintTopBar (juce::Graphics&);
    void paintPanel (juce::Graphics&);
    void paintPlate (juce::Graphics&);
    void paintIoMeter (juce::Graphics&, float x0, const Meter (&m)[2], bool target);
    void paintGainReduction (juce::Graphics&);
    void paintLimiter (juce::Graphics&);
    void paintAmountText (juce::Graphics&);

    DevilsParticleProcessor& proc;
    Content content { *this };
    float scale = 1.0f;

    dp::Logo logo;
    dp::DropBox presetBox, scaleBox;
    dp::IconButton prevButton { dp::IconButton::Icon::left }, nextButton { dp::IconButton::Icon::right },
        saveButton { dp::IconButton::Icon::save };
    dp::SegmentToggle sidechain;
    dp::GearButton gear;

    dp::PowerButton eqPower, amountPower, limiterPower;
    dp::MetalKnob eqHigh { dp::MetalKnob::Kind::small }, eqMid { dp::MetalKnob::Kind::small }, eqLow { dp::MetalKnob::Kind::small };
    dp::MetalKnob amount { dp::MetalKnob::Kind::large };
    dp::MetalKnob inputKnob { dp::MetalKnob::Kind::plate }, limiterKnob { dp::MetalKnob::Kind::plate },
        outputKnob { dp::MetalKnob::Kind::plate };
    dp::Visualizer visualizer;
    struct PaintOverlay : juce::Component
    {
        std::function<void (juce::Graphics&)> paintFn;
        void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
    } amountText;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    std::unique_ptr<dp::DropList> dropList;
    std::unique_ptr<juce::Component> dialog;

    Meter inMeter[2], outMeter[2];
    float bandGrDb[3] {}, limGrDb[2] {};
    bool eqOn = true, characterOn = true, limiterOn = true;
    float characterValue = 0.5f;
    bool frozen = false;
    juce::Image knobShadow, plateImage;   // plate knobs' shadow and the grained plate, per physical scale
    float plateScale = 0.0f;
    float knobShadowScale = 0.0f;
    int lastBlocks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevilsParticleEditor)
};
