#include "Visualizer.h"

using namespace juce;

namespace dp
{
static Path stadium (float x, float y, float w, float h)
{
    Path p;
    p.addRoundedRectangle (x, y, w, h, h * 0.5f);
    return p;
}

static Path stadiumAround (float cx, float cy, float halfLength, float r)
{
    return stadium (cx - halfLength - r, cy - r, 2.0f * (halfLength + r), 2.0f * r);
}

Visualizer::Visualizer()
{
    setInterceptsMouseClicks (false, false);
    Random rng (20240704);
    particles.resize (480);
    for (auto& p : particles)
    {
        p.u = rng.nextFloat();
        p.d = std::pow (rng.nextFloat(), 0.7f);                 // denser towards the outside of the ring
        p.speed = (0.004f + 0.012f * rng.nextFloat()) * (rng.nextFloat() < 0.8f ? 1.0f : -1.0f);
        p.size = 0.5f + 2.0f * std::pow (rng.nextFloat(), 2.4f);
        p.phase = rng.nextFloat() * MathConstants<float>::twoPi;
        p.twinkle = 0.5f + 2.5f * rng.nextFloat();
        p.hue = rng.nextInt (7);
        p.streak = rng.nextFloat() < 0.3f;           // drawn as an ellipse along the flow
    }
}

Point<float> Visualizer::racetrack (float u, float r) const
{
    // u in [0, 1): around the racetrack clockwise from the top-left end of the top straight
    const float straight = 2.0f * halfLength, arc = MathConstants<float>::pi * r, total = 2.0f * straight + 2.0f * arc;
    float s = (u - std::floor (u)) * total;
    if (s < straight) return { cx - halfLength + s, cy - r };
    s -= straight;
    if (s < arc) { const float a = s / r; return { cx + halfLength + r * std::sin (a), cy - r * std::cos (a) }; }
    s -= arc;
    if (s < straight) return { cx + halfLength - s, cy + r };
    s -= straight;
    const float a = s / r;
    return { cx - halfLength - r * std::sin (a), cy + r * std::cos (a) };
}

void Visualizer::advance (bool audio, float mix, bool animate)
{
    const float dt = 1.0f / 30.0f;
    colourMix = mix;
    if (audio) silentFor = 0.0; else silentFor += dt;
    const float target = (! animate) ? 0.0f : (audio ? 1.0f : (silentFor < 0.55 ? 1.0f : 0.0f));
    if (target > level) level = 1.0f;                                  // appears at once
    else level = jmax (target, level - dt / 0.35f);                    // fades out over 0.35 s
    if (level > 0.0f)
    {
        time += dt;
        for (auto& p : particles) p.u += p.speed * dt;
    }
    repaint();
}

void Visualizer::renderFrame (float scale)
{
    const auto b = getBounds().toFloat();                          // editor design coordinates
    frame = Image (Image::ARGB, jmax (1, roundToInt (b.getWidth() * scale)), jmax (1, roundToInt (b.getHeight() * scale)), true,
                   SoftwareImageType());
    {
    Graphics g (frame);
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()).scaled (scale));

    // Glass racetrack, measured in polar coordinates around the two ends (RESEARCH.md): an almost black body, faint
    // edge lines, two white crescents on the lower left and a cold band on the lower right.
    auto outer = stadiumAround (cx, cy, halfLength, outerRadius);
    g.setColour (Colour (0xff030303));
    g.fillPath (outer);
    g.setColour (Colour (0xff131313));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 0.5f), PathStrokeType (1.0f));
    g.setColour (Colour (0xff0e0e0e));
    g.strokePath (stadiumAround (cx, cy, halfLength, 71.0f), PathStrokeType (1.0f));

    // the amount pill
    auto pill = stadiumAround (cx, cy, halfLength, pillRadius);
    g.setColour (Colours::black);
    g.fillPath (pill);
    g.setColour (Colour (0xffa6a3a4));
    g.strokePath (pill, PathStrokeType (1.0f));
    }   // the drawing context is flushed before the pixel pass

    // reflections, added per pixel: crescents around the racetrack's ends with a gaussian radial profile and a
    // smooth angular envelope (centre radius, radial sigma, start / peak / end angle in degrees from up, peak grey)
    struct Crescent { bool leftEnd; float r, sigma, a0, aPeak, a1, peak; Colour tint; };
    const Crescent cs[] { { true, 98.0f, 4.6f, 190.0f, 245.0f, 290.0f, 158.0f, Colours::white },
                          { true, 84.5f, 3.4f, 218.0f, 255.0f, 279.0f, 160.0f, Colours::white },
                          { true, 74.0f, 0.6f, 232.0f, 250.0f, 266.0f, 70.0f, Colours::white },
                          { true, 77.0f, 0.6f, 290.0f, 300.0f, 312.0f, 62.0f, Colours::white },
                          { false, 93.5f, 2.6f, 128.0f, 150.0f, 172.0f, 62.0f, Colour (0xffbfe3ea) },
                          { false, 71.0f, 0.5f, 36.0f, 62.0f, 100.0f, 40.0f, Colours::white } };
    Image::BitmapData bd (frame, Image::BitmapData::readWrite);
    for (int py = 0; py < frame.getHeight(); ++py)
        for (int px = 0; px < frame.getWidth(); ++px)
        {
            const float x = b.getX() + ((float) px + 0.5f) / scale, y = b.getY() + ((float) py + 0.5f) / scale;
            float add[3] {};
            for (auto& c : cs)
            {
                const float ex = c.leftEnd ? cx - halfLength : cx + halfLength;
                const float dx = x - ex, dy = y - cy;
                if (c.leftEnd ? dx > 0.0f : dx < 0.0f) continue;
                const float r = std::sqrt (dx * dx + dy * dy);
                float a = radiansToDegrees (std::atan2 (dx, -dy));
                if (a < 0.0f) a += 360.0f;
                if (a < c.a0 || a > c.a1) continue;
                const float t = a < c.aPeak ? (a - c.a0) / (c.aPeak - c.a0) : (c.a1 - a) / (c.a1 - c.aPeak);
                const float env = std::sin (MathConstants<float>::halfPi * jlimit (0.0f, 1.0f, t * 2.4f));
                const float rad = std::exp (-0.5f * (r - c.r) * (r - c.r) / (c.sigma * c.sigma + 0.25f / (scale * scale)));
                const float v = c.peak * env * rad;
                add[0] += v * c.tint.getFloatRed(); add[1] += v * c.tint.getFloatGreen(); add[2] += v * c.tint.getFloatBlue();
            }
            if (add[0] + add[1] + add[2] <= 0.5f) continue;
            const auto old = bd.getPixelColour (px, py);
            bd.setPixelColour (px, py, Colour ((uint8) jmin (255.0f, old.getRed() + add[0]), (uint8) jmin (255.0f, old.getGreen() + add[1]),
                                               (uint8) jmin (255.0f, old.getBlue() + add[2]), (uint8) 255));
        }
    framePx = scale;
}

void Visualizer::paint (Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (std::abs (scale - framePx) > 0.001f || frame.isNull()) renderFrame (scale);
    const auto b = getBounds().toFloat();
    g.drawImage (frame, getLocalBounds().toFloat());
    if (level <= 0.0f) return;

    static const Colour palette[7] { Colour (0xffffc933), Colour (0xffffffff), Colour (0xff4d8dff), Colour (0xff9b5cff),
                                     Colour (0xffff4646), Colour (0xff46e0ff), Colour (0xffff9a2e) };
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));

    // while particles show, the pill rim catches their light
    g.setColour (Colours::white.withAlpha (0.35f * level));
    g.strokePath (stadiumAround (cx, cy, halfLength, pillRadius), PathStrokeType (1.6f));
    g.setColour (Colours::white.withAlpha (0.08f * level));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 0.5f), PathStrokeType (1.2f));

    Graphics::ScopedSaveState s (g);
    auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 2.0f);
    ring.setUsingNonZeroWinding (false);
    ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.5f));
    g.reduceClipRegion (ring);
    // faint coloured haze in the ring
    {
        const auto hazeA = Colour (0xff6a4cff).interpolatedWith (Colours::white, 1.0f - colourMix);
        const auto hazeB = Colour (0xffffb04a).interpolatedWith (Colours::white, 1.0f - colourMix);
        for (int i = 0; i < 8; ++i)                 // stacked strokes: a smooth band, densest in the middle
        {
            g.setColour (hazeA.withAlpha (0.012f * level));
            g.strokePath (stadiumAround (cx, cy, halfLength, 82.0f), PathStrokeType (8.0f + 6.0f * (float) i));
            g.setColour (hazeB.withAlpha (0.007f * level));
            g.strokePath (stadiumAround (cx, cy, halfLength, 97.0f), PathStrokeType (4.0f + 3.0f * (float) i));
        }
    }
    for (auto& p : particles)
    {
        const float r = pillRadius + 5.0f + p.d * (outerRadius - pillRadius - 9.0f);
        const auto pos = racetrack (p.u, r);
        const float tw = 0.6f + 0.4f * std::sin (p.phase + time * p.twinkle);
        const auto hue = Colours::white.interpolatedWith (palette[p.hue], colourMix);
        const float sz = p.size;
        // direction of travel, for the elongated ones
        const auto ahead = racetrack (p.u + 0.002f, r);
        const float ang = std::atan2 (ahead.y - pos.y, ahead.x - pos.x);
        const float stretch = p.streak ? 1.9f : 1.0f;
        const auto t = AffineTransform::scale (stretch, 1.0f).rotated (ang).translated (pos);
        // soft halo
        g.setGradientFill (ColourGradient (hue.withAlpha (0.6f * level * tw), pos, hue.withAlpha (0.0f), pos.translated (sz * 2.6f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (sz * 5.2f, sz * 5.2f).withCentre (pos));
        // body and hot core
        g.setColour (hue.withAlpha (0.95f * level * tw));
        g.fillPath ([&] { Path e; e.addEllipse (-sz * 0.75f, -sz * 0.75f, sz * 1.5f, sz * 1.5f); return e; }(), t);
        g.setColour (Colours::white.withAlpha (0.9f * level * tw));
        g.fillPath ([&] { Path e; e.addEllipse (-sz * 0.35f, -sz * 0.35f, sz * 0.7f, sz * 0.7f); return e; }(), t);
    }
}
}
