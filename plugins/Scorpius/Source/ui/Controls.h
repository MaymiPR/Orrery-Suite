#pragma once
#include "shared/ui/Widgets.h"
#include "shared/ui/MetalKnob.h"

// Scorpius's own controls, drawn like the reference (Orion, measured at 200 %: RESEARCH.md "GUI"; all sizes in design
// pixels, the editor scales them). The shared ones (power buttons, menus, knobs) are in shared/ui.
namespace sc
{
using orrery::noFocus;

// Options stacked vertically (drive colour a / b / c, harmonics 50Hz / 80Hz, compression mode a / b): the selected one
// filled mint with dark text, the others black with mint text, each row outlined on its own.
class SegmentStack : public juce::Component
{
public:
    explicit SegmentStack (juce::StringArray o) : options (std::move (o)) { noFocus (*this); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    // under the mouse an unselected row fills with mint at 20 % (measured; the selected one does not change)
    void mouseMove (const juce::MouseEvent& e) override { setHover (rowAt (e.y)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    int selected = 0;
    float rowHeight = 22.5f, gap = 0.5f;
    std::function<void (int)> onChange;
private:
    int rowAt (int y) const { return juce::jlimit (0, options.size() - 1, (int) ((float) y / (rowHeight + gap))); }
    void setHover (int r) { if (r != hover) { hover = r; repaint(); } }
    juce::StringArray options;
    int hover = -1, pressed = -1;
    bool held = false;
};

// A labelled toggle ("hi pass"): filled mint while on, outlined while off (and then filled with mint at 20 % under
// the mouse).
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


// Vertical fader of the parallels: a grey track, mint below the cap while the path is on, and a metal cap.
// Mouse (measured on the reference): a drag anywhere on the fader moves the level by 1/176 per design pixel, relative to
// where it started (a click without a drag does not jump); Shift = velocity mode (fine); a double-click or an Alt-click
// resets it to 0.
class Fader : public juce::Slider
{
public:
    Fader();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool on = false;
    static constexpr int pixelsPerRange = 176;     // (JUCE's own relative drag, so the host gets the gestures)
    // the fader's bounds are 37 x 180 from design y 72; the cap's centre travels from level 1 to level 0 between
    // these (the fader's own coordinates; measured at 200 %: y 85.5 and 237.5 on the panel; the cap 37 x 22.9 in a 37 x 23 box)
    static constexpr float top = 72.0f, capW = 37.0f, capH = 23.0f, topY = 85.5f - top, bottomY = 237.5f - top;
    static constexpr float capOffsetX = -0.12f;     // the reference draws its cap this far left of the track
private:
    juce::Image capImage;            // the cap at the current physical pixel size
    int capPx = 0;
    void renderCap (int eighths);
};

// The drive's marks: five of them, in an arrangement of their own for each colour (a, b, c), drawn as eight-armed stars
// while the hi-pass is on and as "+" marks while it is off. They do not move with the level (measured: idle and
// playing frames are the same). Filmed on the reference (RefCapture bursts at ~70 frames/s):
//   - a new colour moves each mark in a straight line to its place in the new arrangement (every mark keeps its own
//     identity and size through a, b and c), easing in and out (cubic) over 215 ms;
//   - the hi-pass cross-fades the two shapes over 112 ms: the old shape fades out under the new one fading in.
class DriveSparks : public juce::Component, private juce::Timer
{
public:
    DriveSparks() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    void setColour (int c, bool animate);
    void setHiPass (bool on, bool animate);
    static constexpr double moveMs = 215.0, crossMs = 112.0;
private:
    void timerCallback() override;
    juce::Point<float> markAt (int mark, double now) const;
    int colour = 0;
    bool hiPass = true;
    juce::Point<float> from[5];           // where each mark set out from
    double moveStart = -1.0e9, crossStart = -1.0e9;
};

// Fades components in and out as the reference fades a module's display when its power button is pressed (the
// circles, the GR readout, the EQ curve, the drive's marks): linearly, 85 ms for a full fade (measured).
class Fades : private juce::Timer
{
public:
    static constexpr double fullMs = 85.0;
    void show (juce::Component&, bool visible, bool animate);
private:
    struct Fade { juce::Component::SafePointer<juce::Component> c; float from, to; double start; };
    std::vector<Fade> fades;
    void timerCallback() override;
};

// The compression's gain reduction readout above its knob ("- 3.6", "0.0").
class GrReadout : public juce::Component
{
public:
    GrReadout() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    juce::String text { "0.0" };
};


// The circles above harmonics and lift (measured on the reference with tones, steps and music, RESEARCH.md UI rounds 4
// and 5): at rest a circle of radius 6.43; once the level it follows (Visuals.h: the lows for harmonics, the mids for
// lift, a new reading every 85 ms) passes -52 dBFS, a ring of radius k0 + k1 (a^power - 0.083), a the level (9.6 at
// -50 dBFS, 10.7 at -40, 18 at -10 with tones), growing and shrinking at its own pace (Law: fitted to films of music,
// each circle by itself). Its outline wobbles by about 1 px (rms) in lobes 2 ... 12, most in 2 ... 4, and the shape
// morphs in place, only as the level changes: a steady tone leaves it still, music keeps it moving.
class Blob : public juce::Component
{
public:
    struct Law { float k0, k1, power, upSeconds, downSeconds, fallPxPerSecond; };
    static constexpr Law lows { 8.286f, 15.797f, 0.312f, 0.05f, 0.298f, 23.9f }, mids { 3.755f, 19.072f, 0.168f, 0.223f, 0.405f, 53.2f };
    Blob (juce::uint32 seed, Law);
    void paint (juce::Graphics&) override;
    // a new peak (linear) for the time since the last call
    void update (float peak, float seconds);
    // the snapshot tool: the shape for a level, still
    void freeze (float peak);
    static constexpr float restRadius = 6.43f, maxRadius = 22.0f;
private:
    static constexpr int lobes = 11;              // 2 ... 12
    Law law;
    float target (float peak) const;
    float radius = restRadius, time = 0.0f, lastDb = -200.0f;
    float amp[lobes] {}, phaseC[lobes] {}, phaseS[lobes] {}, rateC[lobes] {}, rateS[lobes] {};
};

// The harmonic EQ's bands, drawn above the knobs while the EQ is on: one shape per band, as the reference draws them.
class EqCurve : public juce::Component
{
public:
    EqCurve() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    float gains[5] {};
};
}
