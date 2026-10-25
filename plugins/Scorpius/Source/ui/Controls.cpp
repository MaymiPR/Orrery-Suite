#include "Controls.h"
#include "Params.h"
#include "FaderCap.h"

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
    // Shift: JUCE's velocity mode (cursor hidden, slow moves fine-tune), as the reference; Ctrl drags at the normal speed
    setVelocityModeParameters (1.0, 1, 0.0, true, ModifierKeys::shiftModifier);
    setMouseDragSensitivity (pixelsPerRange);
    setDoubleClickReturnValue (true, 0.0);
}

// Alt-click: back to 0 (measured), without a drag after it
void Fader::mouseDown (const MouseEvent& e)
{
    Slider::mouseDown (e);
    if (e.mods.isAltDown()) setValue (getDoubleClickReturnValue(), sendNotificationSync);
}

void Fader::mouseDrag (const MouseEvent& e)
{
    if (! e.mods.isAltDown()) Slider::mouseDrag (e);
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
    // cap: the reference's cap, drawn from the model fitted to it (FaderCap.h), rendered per pixel at the current scale
    const int px = jmax (1, roundToInt (theme::pixelScale (g, *this) * 8.0f));      // eighths of a pixel per design px
    if (capImage.isNull() || (px != capPx && ! theme::liveResizing (*this))) renderCap (px);
    const Rectangle<float> cap (cx - capW * 0.5f, capY - capH * 0.5f, capW, capH);
    g.drawImage (capImage, cap, RectanglePlacement::stretchToFit);
}

// grey of the cap at a point (2 px per design px, from its centre), or < 0 outside it
static float capGrey (float x, float y)
{
    namespace fc = fadercap;
    auto inside = [] (float px, float py, float hw, float hh, float r)       // distance inside a rounded rectangle
    {
        const float qx = std::abs (px) - (hw - r), qy = std::abs (py) - (hh - r);
        return r - std::hypot (jmax (qx, 0.0f), jmax (qy, 0.0f)) - jmin (jmax (qx, qy), 0.0f);
    };
    const float d = inside (x, y, fc::halfW, fc::halfH, fc::radius);
    if (d <= 0.0f) return -1.0f;
    const float gx = 0.5f * (fc::grooveL + fc::grooveR), gy = 0.5f * (fc::grooveT + fc::grooveB);
    const float dg = inside (x - gx, y - gy, 0.5f * (fc::grooveR - fc::grooveL), 0.5f * (fc::grooveB - fc::grooveT), fc::grooveRadius);
    if (dg > 0.0f) return dg < fc::ring ? fc::grooveEdge : fc::grooveCore;
    // a table over an angle (turns clockwise from up), read with linear interpolation round the circle
    auto lookup = [] (const float* table, int steps, float turns)
    {
        const float u = (turns - std::floor (turns)) * (float) steps;
        const int i0 = (int) u % steps;
        const float w = u - std::floor (u);
        return table[i0] * (1.0f - w) + table[(i0 + 1) % steps] * w;
    };
    const int ring = (int) (d / fc::ring);
    if (ring < fc::rings)
        return lookup (fc::rim[ring], fc::rimSteps, std::atan2 (x / fc::halfW, -y / fc::halfH) / MathConstants<float>::twoPi);
    // the face repeats every half turn
    return lookup (dg > -fc::ring ? fc::halo : fc::face, fc::steps, std::atan2 (x, -y) / MathConstants<float>::pi);
}

void Fader::renderCap (int px)
{
    capPx = px;
    const float s = (float) px / 8.0f;                 // physical px per design px
    const int w = jmax (1, roundToInt (capW * s)), h = jmax (1, roundToInt (capH * s));
    capImage = Image (Image::ARGB, w, h, true, SoftwareImageType());
    Image::BitmapData bd (capImage, Image::BitmapData::writeOnly);
    constexpr int ss = 4;                              // 4 x 4 samples per pixel
    const float toModelX = capW * 2.0f / (float) w, toModelY = capH * 2.0f / (float) h;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            float sum = 0.0f;
            int in = 0;
            for (int sj = 0; sj < ss; ++sj)
                for (int si = 0; si < ss; ++si)
                {
                    const float x = ((float) i + ((float) si + 0.5f) / ss) * toModelX - capW - 2.0f * capOffsetX;
                    const float y = ((float) j + ((float) sj + 0.5f) / ss) * toModelY - capH;
                    const float v = capGrey (x, y);
                    if (v >= 0.0f) { sum += v; ++in; }
                }
            if (in == 0) continue;
            const float alpha = (float) in / (float) (ss * ss);
            const auto grey = (uint8) jlimit (0, 255, roundToInt (sum / (float) in));
            bd.setPixelColour (i, j, Colour (grey, grey, grey).withAlpha (alpha));
        }
}

//==============================================================================
namespace
{
// the marks' centres per colour (measured at 200 %: design px from the column's corner at 96, 41), in the order of
// their identities: a mark moves to its own place when the colour changes (marks 2 and 3 are the large ones)
constexpr float markPos[3][5][2] {
    { { 6.5f, 73.75f }, { 20.5f, 54.75f }, { 42.5f, 39.75f }, { 42.5f, 64.75f }, { 57.5f, 48.75f } },
    { { 40.5f, 74.75f }, { 58.5f, 42.75f }, { 20.5f, 39.75f }, { 33.5f, 52.75f }, { 7.5f, 57.75f } },
    { { 35.5f, 38.75f }, { 21.5f, 52.75f }, { 10.5f, 61.75f }, { 53.5f, 51.75f }, { 35.5f, 75.75f } } };
constexpr float markSize[5] { 4.25f, 4.25f, 7.5f, 7.5f, 4.25f };       // arm length

double nowMs() { return Time::getMillisecondCounterHiRes(); }

float easeInOutCubic (float u)
{
    return u < 0.5f ? 4.0f * u * u * u : 1.0f - std::pow (-2.0f * u + 2.0f, 3.0f) * 0.5f;
}
}

Point<float> DriveSparks::markAt (int i, double now) const
{
    const Point<float> to (markPos[colour][i][0], markPos[colour][i][1]);
    const float u = (float) jlimit (0.0, 1.0, (now - moveStart) / moveMs);
    return from[i] + (to - from[i]) * easeInOutCubic (u);
}

void DriveSparks::setColour (int c, bool animate)
{
    c = jlimit (0, 2, c);
    if (c == colour) return;
    const double now = nowMs();
    for (int i = 0; i < 5; ++i)            // (a move under way sets out again from where its marks are)
        from[i] = animate ? markAt (i, now) : Point<float> (markPos[c][i][0], markPos[c][i][1]);
    colour = c;
    moveStart = animate ? now : -1.0e9;
    if (animate) startTimerHz (60);
    repaint();
}

void DriveSparks::setHiPass (bool on, bool animate)
{
    if (on == hiPass) return;
    hiPass = on;
    crossStart = animate ? nowMs() : -1.0e9;
    if (animate) startTimerHz (60);
    repaint();
}

void DriveSparks::timerCallback()
{
    const double now = nowMs();
    if (now - moveStart > moveMs && now - crossStart > crossMs) stopTimer();
    repaint();
}

void DriveSparks::paint (Graphics& g)
{
    const double now = nowMs();
    // the cross-fade: the shape on its way out first, the new one over it
    const float q = (float) jlimit (0.0, 1.0, (now - crossStart) / crossMs);
    for (int layer = 0; layer < 2; ++layer)
    {
        const bool star = layer == 0 ? ! hiPass : hiPass;
        const float alpha = layer == 0 ? 1.0f - q : q;
        if (alpha <= 0.0f) continue;
        g.setColour (theme::mint.withMultipliedAlpha (alpha));
        for (int i = 0; i < 5; ++i)
        {
            const auto m = markAt (i, now);
            const float s = markSize[i];
            Path p;
            p.startNewSubPath (m.x - s, m.y); p.lineTo (m.x + s, m.y);
            p.startNewSubPath (m.x, m.y - s); p.lineTo (m.x, m.y + s);
            if (star)
            {
                const float d = s * 0.72f;
                p.startNewSubPath (m.x - d, m.y - d); p.lineTo (m.x + d, m.y + d);
                p.startNewSubPath (m.x - d, m.y + d); p.lineTo (m.x + d, m.y - d);
            }
            g.strokePath (p, PathStrokeType (s > 5.0f ? 1.0f : 0.55f));        // (measured: 2 px and 1 px at 200 %)
        }
    }
}

//==============================================================================
void Fades::show (Component& c, bool visible, bool animate)
{
    const float target = visible ? 1.0f : 0.0f;
    auto it = std::find_if (fades.begin(), fades.end(), [&c] (const Fade& f) { return f.c == &c; });
    if (it != fades.end() ? it->to == target : c.isVisible() == visible) return;        // already there or on its way
    const float now = c.isVisible() ? c.getAlpha() : 0.0f;                               // (a fade under way turns back)
    if (it != fades.end()) fades.erase (it);
    if (! animate)
    {
        c.setAlpha (1.0f);
        c.setVisible (visible);
        return;
    }
    c.setAlpha (now);
    c.setVisible (true);
    fades.push_back ({ &c, now, target, nowMs() });
    startTimerHz (60);
}

void Fades::timerCallback()
{
    const double t = nowMs();
    for (auto it = fades.begin(); it != fades.end();)
    {
        if (it->c == nullptr) { it = fades.erase (it); continue; }
        const double span = fullMs * std::abs (it->to - it->from);
        const float u = span <= 0.0 ? 1.0f : (float) jlimit (0.0, 1.0, (t - it->start) / span);
        it->c->setAlpha (it->from + (it->to - it->from) * u);
        if (u >= 1.0f)
        {
            it->c->setVisible (it->to > 0.0f);
            it->c->setAlpha (1.0f);
            it = fades.erase (it);
        }
        else
            ++it;
    }
    if (fades.empty()) stopTimer();
}

//==============================================================================
void GrReadout::paint (Graphics& g)
{
    g.setColour (theme::mint);
    g.setFont (theme::font (19.5f));
    theme::textAt (g, text, (float) getWidth() * 0.5f, 22.0f, Justification::horizontallyCentred);
}

//==============================================================================
void Toast::show (const String& t)
{
    text = t;
    shownAt = nowMs();
    setAlpha (1.0f);
    setVisible (true);
    toFront (false);
    repaint();
    startTimerHz (60);
}

void Toast::timerCallback()
{
    const double t = nowMs() - shownAt - holdMs;
    if (t >= fadeMs) { stopTimer(); setVisible (false); setAlpha (1.0f); return; }
    setAlpha (t <= 0.0 ? 1.0f : (float) (1.0 - t / fadeMs));
}

void Toast::paint (Graphics& g)
{
    // (measured at 200 %: the box 200 x 60.5 with a 1 px edge #252523, filled #050607; the text as the labels, its
    // baseline 34.25 below the box's top)
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff252523));
    g.fillRect (b);
    g.setColour (Colour (0xff050607));
    g.fillRect (b.reduced (1.0f));
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    theme::textAt (g, text, b.getCentreX() - 1.0f, 34.25f, Justification::horizontallyCentred);
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
