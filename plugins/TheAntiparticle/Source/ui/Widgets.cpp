#include "Widgets.h"
#include "BinaryData.h"

using namespace juce;

namespace ap
{
Font theme::font (float emPx)
{
    static const auto face = Typeface::createSystemTypefaceFor (BinaryData::InstrumentSansRegular_ttf,
                                                                BinaryData::InstrumentSansRegular_ttfSize);
    return Font (FontOptions (face).withPointHeight (emPx));
}

void theme::textAt (Graphics& g, const String& s, float x, float baseline, Justification j)
{
    GlyphArrangement ga;
    ga.addLineOfText (g.getCurrentFont(), s, 0.0f, 0.0f);
    const float w = ga.getBoundingBox (0, -1, false).getRight();
    const float dx = j.testFlags (Justification::right) ? x - w : j.testFlags (Justification::horizontallyCentred) ? x - w * 0.5f : x;
    ga.draw (g, AffineTransform::translation (dx, baseline));
}

static void powerIcon (Graphics& g, Point<float> c, float r, Colour col, float stroke)
{
    Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, 0.70f, MathConstants<float>::twoPi - 0.70f, true);
    p.startNewSubPath (c.x, c.y - r - 1.0f);
    p.lineTo (c.x, c.y - r * 0.15f);
    g.setColour (col);
    g.strokePath (p, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
}

void PowerButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f);
    if (active)
    {
        g.setColour (theme::mint.brighter (isMouseOver() ? 0.06f : 0.0f));
        g.fillRoundedRectangle (b, 2.5f);
        powerIcon (g, b.getCentre().translated (0.0f, 0.5f), 6.2f, Colour (0xff0b1512), 1.0f);
    }
    else
    {
        g.setColour (theme::mint);
        g.drawRoundedRectangle (b.reduced (0.5f), 2.5f, 1.0f);
        powerIcon (g, b.getCentre().translated (0.0f, 0.5f), 6.2f, theme::mint, 1.0f);
    }
}

void LinkButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f).reduced (0.5f);
    g.setColour (active ? theme::text : theme::boxFill);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (theme::text.withAlpha (isMouseOver() || active ? 1.0f : 0.95f));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setFont (theme::font (12.0f));
    g.setColour (active ? theme::boxFill : theme::text);
    g.drawText ("I/O LINK", Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 24.5f).translated (0.5f, -0.5f), Justification::centred, false);
}

void IconButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f);
    g.setColour (theme::boxFill);
    g.fillRoundedRectangle (b.reduced (0.5f), 4.0f);
    g.setColour (theme::text.withAlpha (isMouseOver() ? 1.0f : 0.95f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    const auto c = b.getCentre();
    Path p;
    if (icon == Icon::save)
    {
        // floppy disk
        const auto r = Rectangle<float> (15.0f, 15.5f).withCentre (c);
        p.startNewSubPath (r.getX() + 1.5f, r.getY());
        p.lineTo (r.getRight() - 3.0f, r.getY());
        p.lineTo (r.getRight(), r.getY() + 3.0f);
        p.lineTo (r.getRight(), r.getBottom() - 1.5f);
        p.quadraticTo (r.getRight(), r.getBottom(), r.getRight() - 1.5f, r.getBottom());
        p.lineTo (r.getX() + 1.5f, r.getBottom());
        p.quadraticTo (r.getX(), r.getBottom(), r.getX(), r.getBottom() - 1.5f);
        p.lineTo (r.getX(), r.getY() + 1.5f);
        p.quadraticTo (r.getX(), r.getY(), r.getX() + 1.5f, r.getY());
        p.closeSubPath();
        p.addRectangle (r.getX() + 3.5f, r.getY(), r.getWidth() - 8.0f, 4.5f);
        p.addRectangle (r.getX() + 3.0f, r.getY() + 8.5f, r.getWidth() - 6.0f, r.getHeight() - 8.5f);
        g.strokePath (p, PathStrokeType (1.1f));
        return;
    }
    const float d = icon == Icon::left ? -1.0f : 1.0f;
    p.startNewSubPath (c.x - 6.5f * d, c.y);
    p.lineTo (c.x + 6.5f * d, c.y);
    p.startNewSubPath (c.x + 1.0f * d, c.y - 5.5f);
    p.lineTo (c.x + 6.5f * d, c.y);
    p.lineTo (c.x + 1.0f * d, c.y + 5.5f);
    g.strokePath (p, PathStrokeType (1.1f, PathStrokeType::mitered, PathStrokeType::rounded));
}

static void triangle (Graphics& g, Rectangle<float> r, Colour c)
{
    Path t;
    t.addTriangle (r.getX(), r.getY(), r.getRight(), r.getY(), r.getCentreX(), r.getBottom());
    g.setColour (c);
    g.fillPath (t);
}

void DropBox::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f);
    g.setColour (theme::boxFill);
    g.fillRect (b);
    g.setColour (open ? theme::text : theme::boxLine);
    g.drawRect (b, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    theme::textAt (g, text, 6.5f, 16.5f);
    triangle (g, { b.getRight() - 20.0f, 10.0f, 10.0f, 4.5f }, theme::text);
}

void SegmentToggle::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withHeight (24.5f).reduced (0.5f);
    const float half = b.getWidth() * 0.5f + 0.25f;
    g.setColour (theme::boxFill);
    g.fillRoundedRectangle (b, 4.0f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (b, 4.0f);
        g.reduceClipRegion (clip);
        g.setColour (theme::text);
        g.fillRect (external ? b.withTrimmedLeft (half) : b.withWidth (half));
    }
    g.setColour (theme::text);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setFont (theme::font (12.0f));
    g.setColour (external ? theme::text : theme::boxFill);
    g.drawText ("INT", Rectangle<float> (0.0f, 0.0f, half, 24.5f).translated (0.5f, -0.5f), Justification::centred, false);
    g.setColour (external ? theme::boxFill : theme::text);
    g.drawText ("EXT", Rectangle<float> (half, 0.0f, b.getWidth() - half, 24.5f).translated (0.5f, -0.5f), Justification::centred, false);
}

void GearButton::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    Path gear;
    const int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a0 = MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
        const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / (float) (teeth * 2);
        const float r = (i % 2 == 0) ? 7.6f : 6.0f;
        const auto p0 = c.getPointOnCircumference (r, a0 + 0.06f), p1 = c.getPointOnCircumference (r, a1 - 0.06f);
        if (i == 0) gear.startNewSubPath (p0); else gear.lineTo (p0);
        gear.lineTo (p1);
    }
    gear.closeSubPath();
    g.setColour (isMouseOver() ? theme::text : theme::boxLine);
    g.strokePath (gear, PathStrokeType (1.3f, PathStrokeType::curved));
    g.drawEllipse (Rectangle<float> (6.2f, 6.2f).withCentre (c), 1.3f);
}

void Logo::paint (Graphics& g)
{
    if (isMouseOver()) { g.setColour (theme::text.withAlpha (0.06f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 2.0f); }
    // the Orrery Suite mark, flat like the reference's maker's mark (solid grey, no outline or shading): a sun with its
    // orbit, the orbit lit along its lower arc as a crescent, a planet nearer in (at this size the inner orbit is left out)
    const auto c = getLocalBounds().toFloat().getCentre();
    const float ro = 7.2f;
    g.setColour (theme::boxLine);
    g.drawEllipse (Rectangle<float> (2 * ro, 2 * ro).withCentre (c), 1.0f);                     // the orbit
    Path crescent;                                                                             // its lit lower arc
    for (int i = 0; i <= 24; ++i)                                                              // outer edge, left to right
    {
        const float a = MathConstants<float>::pi * (0.7f + 0.6f * (float) i / 24.0f);
        const Point<float> p (c.x - (ro + 0.5f) * std::sin (a), c.y - (ro + 0.5f) * std::cos (a));
        if (i == 0) crescent.startNewSubPath (p); else crescent.lineTo (p);
    }
    for (int i = 24; i >= 0; --i)                                                              // inner edge back, higher up
    {
        const float a = MathConstants<float>::pi * (0.7f + 0.6f * (float) i / 24.0f);
        crescent.lineTo (c.x - (ro + 0.5f) * std::sin (a), c.y - 1.2f - (ro - 1.6f) * std::cos (a));
    }
    crescent.closeSubPath();
    g.fillPath (crescent);
    g.fillEllipse (Rectangle<float> (5.2f, 5.2f).withCentre (c + Point<float> (0.0f, -0.6f)));   // the sun (the particle)
    g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (c + Point<float> (3.4f, -3.6f)));   // a planet nearer in
}

InfoDialog::InfoDialog (String t, std::vector<std::pair<String, String>> r)
    : Dialog (std::move (t), { 214.0f, 56.0f, 460.0f, 248.0f }, true), rows (std::move (r)) {}

void InfoDialog::paint (Graphics& g)
{
    Dialog::paint (g);
    float y = box.getY() + 54.0f;
    for (auto& [label, value] : rows)
    {
        if (label.isEmpty() && value.isEmpty()) { y += 8.0f; continue; }
        g.setFont (theme::font (11.5f));
        g.setColour (Colour (0xff8c8c8c));
        theme::textAt (g, label, box.getX() + 20.0f, y);
        g.setColour (theme::text);
        GlyphArrangement ga;
        ga.addJustifiedText (theme::font (11.5f), value, box.getX() + 112.0f, y, box.getWidth() - 132.0f, Justification::left);
        ga.draw (g);
        const int lines = jmax (1, (int) std::round ((ga.getBoundingBox (0, -1, true).getBottom() - y + 9.0f) / 15.0f));
        y += 15.0f * (float) (lines - 1) + 19.0f;
    }
}

void DropList::setItems (std::vector<Item> i, float rowHeight, bool captionsStyle)
{
    items = std::move (i);
    rowH = rowHeight;
    presetStyle = captionsStyle;
    hover = -1;
}

int DropList::rowAt (int y) const
{
    const int r = (int) std::floor ((float) y / rowH);
    return (r >= 0 && r < (int) items.size() && ! items[(size_t) r].caption) ? r : -1;
}

void DropList::mouseUp (const MouseEvent& e)
{
    const int r = rowAt (e.y);
    if (r >= 0 && onPick) onPick (r);
}

void DropList::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0c0c0c));
    g.fillRect (b);
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        const auto row = Rectangle<float> (0.0f, (float) i * rowH, b.getWidth(), rowH);
        if (presetStyle && ! it.caption && i == hover) { g.setColour (Colour (0xff1d1d1d)); g.fillRect (row); }
        if (presetStyle && it.caption) { g.setColour (Colour (0xff151515)); g.fillRect (row.withTrimmedTop (rowH * 0.45f)); }
        if (it.caption)
        {
            g.setColour (theme::menuCaption);
            auto f = theme::font (9.5f);
            f.setExtraKerningFactor (0.32f);
            g.setFont (f);
            theme::textAt (g, it.text.toUpperCase(), 6.0f, row.getY() + rowH * 0.62f + 3.0f);
        }
        else
        {
            g.setColour (presetStyle ? Colour (0xffa9a9a9) : (i == hover ? theme::text : theme::text.withAlpha (0.92f)));
            g.setFont (theme::font (12.0f));
            theme::textAt (g, it.text, 6.0f, row.getY() + rowH * 0.5f + 4.2f);
        }
    }
    g.setColour (theme::text);
    g.drawRect (b, 1.0f);
}

Dialog::Dialog (String t, Rectangle<float> r, bool dimTopBar) : box (r), title (std::move (t)), dimTop (dimTopBar)
{
    noFocus (*this);
}

void Dialog::paint (Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.62f));
    g.fillRect (dimTop ? all : all.withTrimmedTop (30.5f));
    g.setColour (Colour (0xff141414));
    g.fillRect (box);
    g.setColour (Colour (0xff050505));
    g.fillRect (box.withHeight (31.5f));
    g.setColour (theme::text);
    g.setFont (theme::font (17.5f));
    theme::textAt (g, title, box.getX() + 11.0f, box.getY() + 22.5f);
    const auto x = closeArea().reduced (5.5f, 3.5f);
    g.setColour (Colour (0xff5a5a5a));
    g.drawLine ({ x.getTopLeft(), x.getBottomRight() }, 1.0f);
    g.drawLine ({ x.getBottomLeft(), x.getTopRight() }, 1.0f);
}

void Dialog::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    if (closeArea().contains (p) || ! box.contains (p))
        if (onClose) onClose();
}
}
