#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "shared/ui/MetalKnob.h"
#include "shared/ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Stripe.h"

// The editor is laid out in the reference's design pixels (888 x 403.5 at 100 %, measured on Orion at 200 %) and
// scaled as a whole by the UI Scale menu (50 ... 200 %), or freely by dragging its bottom-right corner (ours).
class ScorpiusEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ScorpiusEditor (ScorpiusProcessor&);
    ~ScorpiusEditor() override;

    void resized() override;
    void setUiScale (float);

    static constexpr float designW = 888.0f, designH = 403.5f;
    static constexpr float scales[] { 0.5f, 0.75f, 0.85f, 1.0f, 1.25f, 1.5f, 2.0f };

    // snapshot tool: freeze meters and animations at a given state
    void setTestLevels (float inDb, float outDb, bool audio);

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (ScorpiusEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        ScorpiusEditor& ed;
    };

    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override;
    void updateFromParams();
    void showPresetList();
    void showScaleList();
    void closePopups();
    void showSettings();
    void showSavePreset();
    void showAboutMenu();
    void showDialog (std::unique_ptr<juce::Component>);
    void sampleReadouts();
    void toggleParam (const char* pid);

    void paintTopBar (juce::Graphics&);
    void paintPanel (juce::Graphics&);
    void paintIoMeter (juce::Graphics&, bool output);
    void paintStripe (juce::Graphics&);
    void paintPlate (juce::Graphics&);

    // the readouts update on the reference's own clock (shared/dsp/LevelMeter.h): every 46 ms the peaks, every fifth
    // time the RMS
    struct ReadoutClock : juce::Timer
    {
        explicit ReadoutClock (ScorpiusEditor& e) : ed (e) {}
        void timerCallback() override { ed.sampleReadouts(); }
        ScorpiusEditor& ed;
    } readoutClock { *this };
    int readoutTicks = 0, readoutBlocks = 0;

    // a knob's caption shows its value while the mouse is over it or it is dragged (as the reference: "80.0%")
    struct HoverTracker : juce::MouseListener
    {
        explicit HoverTracker (ScorpiusEditor& e) : ed (e) {}
        void mouseMove (const juce::MouseEvent& e) override { ed.updateHover (e, false); }
        void mouseEnter (const juce::MouseEvent& e) override { ed.updateHover (e, false); }
        void mouseExit (const juce::MouseEvent& e) override { ed.updateHover (e, false); }
        void mouseDrag (const juce::MouseEvent& e) override { ed.updateHover (e, false); }
        void mouseUp (const juce::MouseEvent& e) override { ed.updateHover (e, true); }
        ScorpiusEditor& ed;
    } hoverTracker { *this };
    void updateHover (const juce::MouseEvent&, bool released);
    juce::String caption (const juce::Slider&, const juce::String& name) const;
    juce::Rectangle<float> hoverCell (const juce::Slider&) const;
    const juce::Slider* hovered = nullptr;

    // I/O link (ours, as in The Antiparticle): while linked the output gain is always the input gain's opposite. A
    // gesture on either knob moves the other (as one host gesture each); changes from the host are mirrored too
    void beginLink (juce::Slider&);
    void endLink (juce::Slider&);
    void mirror (bool fromInput);
    juce::Slider* linkSource = nullptr;
    bool mirroring = false;

    ScorpiusProcessor& proc;
    Content content { *this };
    float scale = 1.0f;

    struct Grip : juce::ResizableCornerComponent
    {
        Grip (juce::Component* c, juce::ComponentBoundsConstrainer* k) : juce::ResizableCornerComponent (c, k) { setRepaintsOnMouseActivity (true); }
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
    };
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<Grip> grip;
    juce::uint32 lastResizeMs = 0;
    void endLiveResize();

    // top bar
    orrery::Logo logo;
    orrery::DropBox presetBox, scaleBox;
    orrery::IconButton prevButton { orrery::IconButton::Icon::left }, nextButton { orrery::IconButton::Icon::right },
        saveButton { orrery::IconButton::Icon::save };
    sc::CompareToggle compare;
    orrery::LinkButton ioLinkButton;
    orrery::GearButton gear;

    // drive
    orrery::PowerButton drivePower;
    sc::DriveSparks sparks;
    sc::TextToggle hiPass { "hi pass" };
    sc::SegmentStack color { { "a", "b", "c" } };
    // harmonics, compression, lift
    orrery::PowerButton harmPower, compPower, liftPower;
    sc::Blob harmBlob { 6.5f }, liftBlob { 6.5f };
    sc::GrReadout grReadout;
    sc::SegmentStack freq { { "50Hz", "80Hz" } }, mode { { "a", "b" } };
    orrery::MetalKnob harmKnob { orrery::MetalKnob::Kind::small }, compKnob { orrery::MetalKnob::Kind::small },
        liftKnob { orrery::MetalKnob::Kind::small };
    // harmonic eq
    orrery::PowerButton eqPower;
    sc::EqCurve eqCurve;
    orrery::MetalKnob eqKnobs[5] { orrery::MetalKnob { orrery::MetalKnob::Kind::small }, orrery::MetalKnob { orrery::MetalKnob::Kind::small },
                                   orrery::MetalKnob { orrery::MetalKnob::Kind::small }, orrery::MetalKnob { orrery::MetalKnob::Kind::small },
                                   orrery::MetalKnob { orrery::MetalKnob::Kind::small } };
    // parallels
    orrery::PowerButton parPower[4];
    sc::Fader faders[4];
    // plate
    orrery::MetalKnob inputKnob { orrery::MetalKnob::Kind::plate }, outputKnob { orrery::MetalKnob::Kind::plate };
    sc::Stripe stripe;
    sc::Toast toast;
    // module displays fade with their power buttons; changes animate once the editor is up (not while it opens)
    sc::Fades fades;
    bool animateChanges = false;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<orrery::DropList> dropList;
    std::unique_ptr<juce::Component> dialog;

    orrery::Readouts inMeter[2], outMeter[2];
    float compGrDb = 0.0f;
    bool frozen = false;
    float phase = 0.0f, inLevel = 0.0f;        // animation time and the input level the animations follow (0 ... 1)
    int lastBlocks = 0;
    juce::Image plateImage, knobShadow;
    float plateScale = 0.0f, knobShadowScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorpiusEditor)
};
