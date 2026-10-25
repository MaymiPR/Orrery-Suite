#include "Controls.h"
#include "Params.h"

using namespace juce;
namespace theme = orrery::theme;

namespace sc
{
//==============================================================================
void SegmentStack::paint (Graphics& g)
{
    const float w = (float) getWidth();
    const int n = options.size();
    const float total = rowHeight * (float) n + gap * (float) (n - 1);
    ignoreUnused (total);
    g.setFont (theme::font (12.0f));
    for (int i = 0; i < n; ++i)
    {
        // every row is a box of its own (so two lines meet between rows), rounded only at the stack's outer corners;
        // the selected one is filled
        const Rectangle<float> row (0.0f, (rowHeight + gap) * (float) i, w, rowHeight);
        const bool first = i == 0, last = i == n - 1;
        Path p;
        p.addRoundedRectangle (row.getX(), row.getY(), row.getWidth(), row.getHeight(), 3.0f, 3.0f, first, first, last, last);
        const bool down = i == pressed && held;
        if (i != selected && (i == hover || down))
        {
            g.setColour (theme::mint.withAlpha (0.2f));
            g.fillPath (p);
        }
        g.setColour (theme::mint);
        if (i == selected) g.fillPath (p);
        else if (! down)                    // held down, a row shows no outline (measured)
        {
            Path o;
            const auto r = row.reduced (0.5f);
            o.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 2.5f, 2.5f, first, first, last, last);
            g.strokePath (o, PathStrokeType (1.0f));
        }
        g.setColour (i == selected ? Colour (0xff0b1512) : theme::mint);
        g.drawText (options[i], row.translated (0.0f, -0.5f), Justification::centred, false);
    }
}

// a row is chosen on release over the row that was pressed (measured: a press dragged off chooses nothing)
void SegmentStack::mouseDown (const MouseEvent& e) { pressed = rowAt (e.y); held = true; repaint(); }

// the pressed look lasts only while the mouse stays on the row it pressed
void SegmentStack::mouseDrag (const MouseEvent& e)
{
    const bool on = getLocalBounds().contains (e.getPosition()) && rowAt (e.y) == pressed;
    if (on != held) { held = on; repaint(); }
}

void SegmentStack::mouseUp (const MouseEvent& e)
{
    const int i = rowAt (e.y);
    const bool over = getLocalBounds().contains (e.getPosition());
    const int was = pressed;
    pressed = -1;
    held = false;
    repaint();
    if (over && i == was && i != selected) { selected = i; if (onChange) onChange (i); }
}

//==============================================================================
void TextToggle::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (23.5f);
    g.setFont (theme::font (12.0f));
    if (isMouseButtonDown() && isMouseOver())
    {
        // pressed, on or off: a dark mint box without outline, the text dimmed (measured)
        g.setColour (theme::mint.withAlpha (0.2f));
        g.fillRoundedRectangle (b.reduced (0.5f), 3.0f);
        g.setColour (theme::mint.withAlpha (0.85f));
        g.drawText (text, b.translated (0.0f, -0.5f), Justification::centred, false);
        return;
    }
    if (active)
    {
        g.setColour (theme::mint);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Colour (0xff0b1512));
    }
    else
    {
        if (isMouseOver())
        {
            g.setColour (theme::mint.withAlpha (0.2f));
            g.fillRoundedRectangle (b.reduced (0.5f), 3.0f);
        }
        g.setColour (theme::mint);
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
    }
    g.setFont (theme::font (12.0f));
    g.drawText (text, b.translated (0.0f, -0.5f), Justification::centred, false);
}

//==============================================================================
Rectangle<float> CompareToggle::circle() const
{
    return Rectangle<float> (22.0f, 22.0f).withCentre (getLocalBounds().toFloat().withHeight (24.5f).getCentre());
}

void CompareToggle::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f).reduced (0.5f);
    const float half = b.getWidth() * 0.5f;
    const int inactive = 1 - slot;
    g.setColour (theme::boxFill);
    g.fillRoundedRectangle (b, 4.0f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (b, 4.0f);
        g.reduceClipRegion (clip);
        if (hover == inactive || pressedPart == inactive)
        {
            g.setColour (Colour (0xff252523));
            g.fillRect (inactive == 0 ? b.withWidth (half) : b.withTrimmedLeft (half));
        }
        // the active half is white, or dark while held down
        g.setColour (pressedPart == slot ? Colour (0xff252523) : theme::text);
        g.fillRect (slot == 0 ? b.withWidth (half) : b.withTrimmedLeft (half));
    }
    g.setColour (theme::text);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setFont (theme::font (12.0f));
    const Colour grey (0xff6e7375);
    // the active letter: dark on white, grey under the mouse, white while held down
    g.setColour (slot == 0 ? (pressedPart == 0 ? theme::text : hover == 0 ? grey : theme::boxFill) : theme::text);
    g.drawText ("A", Rectangle<float> (0.0f, 0.0f, 26.0f, 24.5f).translated (0.5f, -0.5f), Justification::centred, false);
    g.setColour (slot == 1 ? (pressedPart == 1 ? theme::text : hover == 1 ? grey : theme::boxFill) : theme::text);
    g.drawText ("B", Rectangle<float> (b.getRight() - 26.0f, 0.0f, 26.0f, 24.5f).translated (0.0f, -0.5f), Justification::centred, false);

    // the copy button: a black circle across the middle, ringed, with an arrow towards the other setting
    const auto c = circle();
    g.setColour (pressedPart == 2 ? Colour (0xff3a3a38) : Colour (0xff0b0b0b));
    g.fillEllipse (c);
    g.setColour (theme::text);
    g.drawEllipse (c.reduced (0.5f), 1.0f);
    // the arrow: grey under the mouse, white again while held down
    g.setColour (hover == 2 && pressedPart != 2 ? Colour (0xff9a9fa1) : theme::text);
    const float d = slot == 0 ? 1.0f : -1.0f;
    const auto m = c.getCentre();
    Path p;
    p.startNewSubPath (m.x - 6.0f * d, m.y);
    p.lineTo (m.x + 6.0f * d, m.y);
    p.startNewSubPath (m.x + 1.0f * d, m.y - 5.0f);
    p.lineTo (m.x + 6.0f * d, m.y);
    p.lineTo (m.x + 1.0f * d, m.y + 5.0f);
    g.strokePath (p, PathStrokeType (1.1f, PathStrokeType::mitered, PathStrokeType::rounded));
}

int CompareToggle::partAt (Point<float> p) const
{
    if (circle().contains (p)) return 2;
    return p.x < (float) getWidth() * 0.5f ? 0 : 1;
}

// acts on release over the part that was pressed (measured: a press dragged off does nothing)
void CompareToggle::mouseDown (const MouseEvent& e) { pressedPart = partAt (e.position); repaint(); }

void CompareToggle::mouseUp (const MouseEvent& e)
{
    const int part = getLocalBounds().contains (e.getPosition()) ? partAt (e.position) : -1;
    const int was = pressedPart;
    pressedPart = -1;
    repaint();
    if (part != was) return;
    if (part == 2) { if (onCopy) onCopy(); return; }
    if (part >= 0 && part != slot && onSelect) onSelect (part);
}

//==============================================================================
Fader::Fader()
{
    noFocus (*this);
    setSliderStyle (Slider::LinearVertical);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 1.0);
    setSliderSnapsToMousePosition (false);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (pixelsPerRange);
    setDoubleClickReturnValue (true, 0.0);
}

void Fader::paint (Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f;
    const float v = (float) valueToProportionOfLength (getValue());
    const float capY = bottomY + (topY - bottomY) * v;
    // track: a thin line over the cap's travel; mint from the cap down while the path is on
    g.setColour (Colour (0xff494949));
    g.fillRect (cx - 1.0f, topY, 2.0f, bottomY - topY);
    if (on && v > 0.0f)
    {
        g.setColour (theme::mint);
        g.fillRect (cx - 1.0f, capY, 2.0f, bottomY - capY);
    }
    // cap: brushed metal with a radial sheen (bright towards the upper right and lower left, as a spun knob face), a
    // bright rim and a dark groove across just above the middle (measured on the reference at 200 %)
    const Rectangle<float> cap (cx - capW * 0.5f, capY - capH * 0.5f, capW, capH);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (cap, 3.0f);
        g.reduceClipRegion (clip);
        const auto c = cap.getCentre();
        constexpr int wedges = 72;
        for (int i = 0; i < wedges; ++i)
        {
            const float a0 = MathConstants<float>::twoPi * (float) i / (float) wedges, a1 = a0 + MathConstants<float>::twoPi / (float) wedges + 0.01f;
            const float am = 0.5f * (a0 + a1);
            // angle from up, clockwise: brightest at 45 and 225 degrees
            const float v = 0.62f + 0.22f * std::cos (2.0f * (am - MathConstants<float>::pi * 0.25f)) + 0.04f * (std::cos (am) < 0.0f ? 1.0f : 0.0f);
            Path w;
            w.startNewSubPath (c);
            w.lineTo (c.x + 40.0f * std::sin (a0), c.y - 40.0f * std::cos (a0));
            w.lineTo (c.x + 40.0f * std::sin (a1), c.y - 40.0f * std::cos (a1));
            w.closeSubPath();
            g.setColour (Colour::greyLevel (jlimit (0.0f, 1.0f, v)));
            g.fillPath (w);
        }
    }
    g.setColour (Colour (0xffcfcfcf));
    g.drawRoundedRectangle (cap.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (Colour (0xff2e2e2e));
    g.fillRoundedRectangle (cap.getX() + 3.5f, cap.getCentreY() - 1.75f, cap.getWidth() - 7.0f, 2.0f, 1.0f);
}

//==============================================================================
void DriveSparks::paint (Graphics& g)
{
    // five marks per colour (measured at 200 %: centres in design px from the column's corner at 96, 41; size = arm
    // length): two large, three small
    struct Mark { float x, y, size; };
    static constexpr Mark marks[3][5] {
        { { 6.5f, 73.75f, 4.25f }, { 20.5f, 54.75f, 4.25f }, { 42.5f, 39.75f, 7.5f }, { 42.5f, 64.75f, 7.5f }, { 57.5f, 48.75f, 4.25f } },
        { { 7.5f, 57.75f, 4.25f }, { 20.5f, 39.75f, 7.5f }, { 33.5f, 52.75f, 7.5f }, { 40.5f, 74.75f, 4.25f }, { 58.5f, 42.75f, 4.25f } },
        { { 10.5f, 61.75f, 7.5f }, { 21.5f, 52.75f, 4.25f }, { 35.5f, 38.75f, 4.25f }, { 35.5f, 75.75f, 4.25f }, { 53.5f, 51.75f, 7.5f } } };
    for (auto& m : marks[jlimit (0, 2, colour)])
    {
        const float s = m.size;
        g.setColour (theme::mint);
        Path p;
        p.startNewSubPath (m.x - s, m.y); p.lineTo (m.x + s, m.y);
        p.startNewSubPath (m.x, m.y - s); p.lineTo (m.x, m.y + s);
        if (hiPass)
        {
            const float d = s * 0.72f;
            p.startNewSubPath (m.x - d, m.y - d); p.lineTo (m.x + d, m.y + d);
            p.startNewSubPath (m.x - d, m.y + d); p.lineTo (m.x + d, m.y - d);
        }
        g.strokePath (p, PathStrokeType (m.size > 5.0f ? 1.0f : 0.55f));        // (measured: 2 px and 1 px at 200 %)
    }
}

//==============================================================================
void Blob::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float grow = on ? jlimit (0.0f, 1.0f, level) * amount : 0.0f;
    const float r = restRadius + (maxRadius - restRadius) * grow;
    Path p;
    constexpr int n = 96;
    for (int i = 0; i <= n; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / (float) n;
        // a few slow lobes whose depth follows the level
        const float wob = grow * (0.08f * std::sin (3.0f * a + phase * 1.3f) + 0.06f * std::sin (5.0f * a - phase * 0.9f)
                                  + 0.04f * std::sin (7.0f * a + phase * 2.1f));
        const float rr = r * (1.0f + wob);
        const Point<float> q (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    p.closeSubPath();
    g.setColour (on ? theme::mint : theme::dimLabel);
    g.strokePath (p, PathStrokeType (1.0f));
}

//==============================================================================
void EqCurve::paint (Graphics& g)
{
    // a log frequency axis over the component's width (40 Hz at the left edge ... 24 kHz: measured, 1 kHz falls at
    // x 84.5) and the bands as bells on a baseline at the bottom; 6 dB is the component's height less a margin
    const float w = (float) getWidth(), h = (float) getHeight();
    const float base = h - 1.0f, top = 4.0f;
    Path fill;
    fill.startNewSubPath (0.0f, base);
    for (int i = 0; i <= 200; ++i)
    {
        const float x = w * (float) i / 200.0f;
        const float f = 40.0f * std::pow (600.0f, x / w);
        float db = 0.0f;
        for (int b = 0; b < 5; ++b)
        {
            // a bell 1.4 octaves wide at half height, as drawn on the reference (the real shapes: Phase 3)
            const float o = std::log2 (f / eqFreqs[b]);
            db += gains[b] * std::exp (-o * o / (2.0f * 0.6f * 0.6f));
        }
        fill.lineTo (x, base - (base - top) * jlimit (0.0f, 1.0f, db / 6.0f));
    }
    fill.lineTo (w, base);
    fill.closeSubPath();
    g.setColour (theme::mint.withAlpha (0.45f));
    g.fillPath (fill);
    g.setColour (theme::mint);
    g.strokePath (fill, PathStrokeType (1.0f));
}
}
