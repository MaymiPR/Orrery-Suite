#pragma once
#include "shared/ui/Widgets.h"
#include "shared/ui/MetalKnob.h"

// Scorpius's own controls, drawn like the reference (Orion, measured at 200 %: RESEARCH.md "GUI"; all sizes in design
// pixels, the editor scales them). The shared ones (power buttons, menus, knobs) are in shared/ui.
namespace sc
{
using orrery::noFocus;

// Options stacked vertically (drive colour a / b / c, harmonics 50Hz / 80Hz, compression mode a / b): the selected one
// filled mint with dark text, the others black with mint text, one mint outline around all with a line between.
class SegmentStack : public juce::Component
{
public:
    explicit SegmentStack (juce::StringArray o) : options (std::move (o)) { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int selected = 0;
    float rowHeight = 22.5f, gap = 0.5f;
    std::function<void (int)> onChange;
private:
    juce::StringArray options;
};

// A labelled toggle ("hi pass"): filled mint while on, outlined while off.
class TextToggle : public juce::Component
{
public:
    explicit TextToggle (juce::String t) : text (std::move (t)) { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    bool active = true;
    std::function<void()> onClick;
private:
    juce::String text;
};

// Compare A -> B of the top bar: the active letter's half is white; the circle in the middle copies the active setting
// to the other one, its arrow pointing that way.
class CompareToggle : public juce::Component
{
public:
    CompareToggle() { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int slot = 0;
    std::function<void (int)> onSelect;
    std::function<void()> onCopy;
private:
    juce::Rectangle<float> circle() const;
};

// Vertical fader of the parallels: a grey track, mint below the cap while the path is on, and a metal cap.
class Fader : public juce::Slider
{
public:
    Fader();
    void paint (juce::Graphics&) override;
    bool on = false;
    // the fader's bounds are 37 x 180 from design y 72; the cap's centre travels from level 1 to level 0 between
    // these (the fader's own coordinates; measured: y 85.25 and 237.25 on the panel)
    static constexpr float top = 72.0f, capW = 37.0f, capH = 22.5f, topY = 85.25f - top, bottomY = 237.25f - top;
};

// The drive's sparks: a cluster of stars (hi-pass on) or "+" marks (off), twinkling with the level.
class DriveSparks : public juce::Component
{
public:
    DriveSparks() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    bool hiPass = true, on = true;
    float level = 0.0f, phase = 0.0f;       // 0 ... 1 input level, animation time
};

// The circles above harmonics and lift: a ring that wobbles and grows with the level the module works on.
class Blob : public juce::Component
{
public:
    explicit Blob (float r) : restRadius (r) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    bool on = true;
    float level = 0.0f, phase = 0.0f, amount = 1.0f;
    float restRadius, maxRadius = 21.0f;
};

// The harmonic EQ's response, drawn above the knobs while the EQ is on: the boost of the five bands on a log axis.
class EqCurve : public juce::Component
{
public:
    EqCurve() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    float gains[5] {};
};
}
