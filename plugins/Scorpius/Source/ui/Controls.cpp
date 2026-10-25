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
        g.setColour (theme::mint);
        if (i == selected) g.fillPath (p);
        else
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

void SegmentStack::mouseDown (const MouseEvent& e)
{
    const int i = jlimit (0, options.size() - 1, (int) ((float) e.y / (rowHeight + gap)));
    if (i != selected) { selected = i; repaint(); if (onChange) onChange (i); }
}

//==============================================================================
void TextToggle::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (23.5f);
    if (active)
    {
        g.setColour (theme::mint.brighter (isMouseOver() ? 0.06f : 0.0f));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Colour (0xff0b1512));
    }
    else
    {
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
    g.setColour (theme::boxFill);
    g.fillRoundedRectangle (b, 4.0f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (b, 4.0f);
        g.reduceClipRegion (clip);
        g.setColour (theme::text);
        g.fillRect (slot == 0 ? b.withWidth (half) : b.withTrimmedLeft (half));
    }
    g.setColour (theme::text);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setFont (theme::font (12.0f));
    g.setColour (slot == 0 ? theme::boxFill : theme::text);
    g.drawText ("A", Rectangle<float> (0.0f, 0.0f, 26.0f, 24.5f).translated (0.5f, -0.5f), Justification::centred, false);
    g.setColour (slot == 1 ? theme::boxFill : theme::text);
    g.drawText ("B", Rectangle<float> (b.getRight() - 26.0f, 0.0f, 26.0f, 24.5f).translated (0.0f, -0.5f), Justification::centred, false);

    // the copy button: a circle across the middle with an arrow towards the other setting
    const auto c = circle();
    g.setColour (theme::boxFill);
    g.fillEllipse (c);
    g.setColour (theme::text.withAlpha (isMouseOver() && c.contains (getMouseXYRelative().toFloat()) ? 1.0f : 0.95f));
    g.drawEllipse (c.reduced (0.5f), 1.0f);
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

void CompareToggle::mouseDown (const MouseEvent& e)
{
    if (circle().contains (e.position)) { if (onCopy) onCopy(); return; }
    const int s = e.x < getWidth() / 2 ? 0 : 1;
    if (s != slot && onSelect) onSelect (s);
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
    setMouseDragSensitivity (roundToInt (bottomY - topY));
}

void Fader::paint (Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f;
    const float v = (float) valueToProportionOfLength (getValue());
    const float capY = bottomY + (topY - bottomY) * v;
    // track: a thin line over the cap's travel; mint from the cap down while the path is on
    g.setColour (Colour (0xff4a4a4a));
    g.fillRect (cx - 0.75f, topY, 1.5f, bottomY - topY);
    if (on && v > 0.0f)
    {
        g.setColour (theme::mint);
        g.fillRect (cx - 0.75f, capY, 1.5f, bottomY - capY);
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
    // five marks (measured, design px relative to the column; size = arm length): two large, three small
    struct Mark { float x, y, size; };
    static constexpr Mark marks[5] { { 42.5f, 39.5f, 7.5f }, { 42.5f, 64.75f, 7.25f }, { 57.5f, 48.75f, 4.25f },
                                     { 20.5f, 54.75f, 4.25f }, { 6.5f, 73.75f, 4.25f } };
    const auto col = on ? theme::mint : theme::dimLabel;
    for (int i = 0; i < 5; ++i)
    {
        const auto& m = marks[i];
        // twinkle: each mark breathes with the level on its own phase
        const float tw = on ? 1.0f + 0.25f * level * std::sin (phase * 6.0f + (float) i * 1.7f) : 1.0f;
        const float s = m.size * tw;
        g.setColour (col);
        Path p;
        p.startNewSubPath (m.x - s, m.y); p.lineTo (m.x + s, m.y);
        p.startNewSubPath (m.x, m.y - s); p.lineTo (m.x, m.y + s);
        if (hiPass)
        {
            const float d = s * 0.72f;
            p.startNewSubPath (m.x - d, m.y - d); p.lineTo (m.x + d, m.y + d);
            p.startNewSubPath (m.x - d, m.y + d); p.lineTo (m.x + d, m.y - d);
        }
        g.strokePath (p, PathStrokeType (m.size > 5.0f ? 1.25f : 1.0f));
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
