#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MetalKnob.h"
#include "ui/Visualizer.h"
#include "ui/Widgets.h"

// The editor is laid out in the reference's design pixels (888 x 359.5 at 100 %) and scaled as a whole by the
// UI Scale menu (50 ... 200 %).
class AntiparticleEditor : public juce::AudioProcessorEditor,
                             private juce::Timer,
                             private juce::ChangeListener,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AntiparticleEditor (AntiparticleProcessor&);
    ~AntiparticleEditor() override;

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
        float framePeakDb = -200.0f;                        // peak follower (drives the input target)
        float rawPeakDb = -200.0f;                          // this frame's sample peak (is audio playing?)
        double peakAge = 0.0;
    };
    class Content : public juce::Component
    {
    public:
        explicit Content (AntiparticleEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        AntiparticleEditor& ed;
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
    void showDialog (std::unique_ptr<ap::Dialog>);
    void updateMeter (Meter&, ap::LevelTap&, float dt);

    void paintTopBar (juce::Graphics&);
    void paintPanel (juce::Graphics&);
    void paintPlate (juce::Graphics&);
    void paintIoMeter (juce::Graphics&, float x0, const Meter (&m)[2], bool target);
    void paintGainReduction (juce::Graphics&);
    void paintLimiter (juce::Graphics&);
    void paintAmountText (juce::Graphics&);

    // a knob's caption shows its value (as the reference: "%.1f dB") while the mouse is over the knob's cell or the
    // knob is being dragged
    struct HoverTracker : juce::MouseListener
    {
        explicit HoverTracker (AntiparticleEditor& e) : ed (e) {}
        void mouseMove (const juce::MouseEvent& e) override { ed.updateReadout (e, false); }
        void mouseEnter (const juce::MouseEvent& e) override { ed.updateReadout (e, false); }
        void mouseExit (const juce::MouseEvent& e) override { ed.updateReadout (e, false); }
        void mouseDrag (const juce::MouseEvent& e) override { ed.updateReadout (e, false); }
        void mouseUp (const juce::MouseEvent& e) override { ed.updateReadout (e, true); }
        AntiparticleEditor& ed;
    } hoverTracker { *this };
    void updateReadout (const juce::MouseEvent&, bool released);
    juce::String captionText (const ap::MetalKnob&, const char* name) const;
    const ap::MetalKnob* readoutKnob = nullptr;

    // I/O link (ours): while linked the output gain is always the input gain's opposite. A gesture on either knob
    // moves the other (as one host gesture each); changes from the host are mirrored too
    void beginLink (ap::MetalKnob&);
    void endLink (ap::MetalKnob&);
    void mirror (bool fromInput);
    ap::MetalKnob* linkSource = nullptr;
    bool mirroring = false;

    AntiparticleProcessor& proc;
    Content content { *this };
    float scale = 1.0f;

    ap::Logo logo;
    ap::DropBox presetBox, scaleBox;
    ap::IconButton prevButton { ap::IconButton::Icon::left }, nextButton { ap::IconButton::Icon::right },
        saveButton { ap::IconButton::Icon::save };
    ap::SegmentToggle sidechain;
    ap::GearButton gear;

    ap::PowerButton eqPower, amountPower, limiterPower;
    ap::MetalKnob eqHigh { ap::MetalKnob::Kind::small }, eqMid { ap::MetalKnob::Kind::small }, eqLow { ap::MetalKnob::Kind::small };
    ap::MetalKnob amount { ap::MetalKnob::Kind::large };
    ap::MetalKnob inputKnob { ap::MetalKnob::Kind::plate }, limiterKnob { ap::MetalKnob::Kind::plate },
        outputKnob { ap::MetalKnob::Kind::plate };
    ap::Visualizer visualizer;
    ap::LinkButton ioLinkButton;
    struct PaintOverlay : juce::Component
    {
        std::function<void (juce::Graphics&)> paintFn;
        void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
    } amountText;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    std::unique_ptr<ap::DropList> dropList;
    std::unique_ptr<juce::Component> dialog;

    Meter inMeter[2], outMeter[2];
    float bandGrDb[3] {}, limGrDb[2] {};                     // now (the bars)
    float bandGrHold[3] {}, limGrHold[2] {};                 // deepest since the last click (the readouts)
    bool bandTargetLit[3] {};                                // GR targets, with a bar of hysteresis
    bool eqOn = true, characterOn = true, limiterOn = true;
    float characterValue = 0.5f;
    bool frozen = false;
    juce::Image knobShadow, plateImage;   // plate knobs' shadow and the grained plate, per physical scale
    float plateScale = 0.0f;
    float knobShadowScale = 0.0f;
    int lastBlocks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AntiparticleEditor)
};
