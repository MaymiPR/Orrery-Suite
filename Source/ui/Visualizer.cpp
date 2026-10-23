#include "Visualizer.h"
#include "Theme.h"
#include "GlassModel.h"
#include "GlassLitModel.h"

using namespace juce;

namespace dp
{
static Path stadiumAround (float cx, float cy, float halfLength, float r)
{
    Path p;
    p.addRoundedRectangle (cx - halfLength - r, cy - r, 2.0f * (halfLength + r), 2.0f * r, r);
    return p;
}

// distance from the racetrack's centre segment, the outward unit vector and the track coordinate t in [0, 4)
// (0..1 top straight left -> right, 1..2 right end, 2..3 bottom straight right -> left, 3..4 left end)
struct TrackPos { float r, nx, ny, t; };
static TrackPos trackPos (float x, float y)
{
    constexpr float cx = Visualizer::cx, cy = Visualizer::cy, hl = Visualizer::halfLength;
    if (std::abs (x - cx) <= hl)
    {
        const bool top = y < cy;
        const float f = (x - (cx - hl)) / (2.0f * hl);
        return { std::abs (y - cy), 0.0f, top ? -1.0f : 1.0f, top ? f : 2.0f + (1.0f - f) };
    }
    const bool right = x > cx;
    const float dx = x - (right ? cx + hl : cx - hl), dy = y - cy;
    const float r = std::sqrt (dx * dx + dy * dy);
    float a = std::atan2 (dx, -dy);                     // 0 = up, clockwise
    if (a < 0.0f) a += MathConstants<float>::twoPi;
    const float t = right ? 1.0f + jlimit (0.0f, 1.0f, a / MathConstants<float>::pi)
                          : 3.0f + jlimit (0.0f, 1.0f, (a - MathConstants<float>::pi) / MathConstants<float>::pi);
    return { r, r > 0.0f ? dx / r : 0.0f, r > 0.0f ? dy / r : 0.0f, t };
}

Visualizer::Visualizer()
{
    setInterceptsMouseClicks (false, false);

    // the 1 s loop of sparks (deterministic, so it looks the same in every instance)
    Random rng (20240704);
    auto gauss = [&rng] { return (float) (std::sqrt (-2.0 * std::log (jmax (1.0e-9, rng.nextDouble()))) * std::cos (MathConstants<double>::twoPi * rng.nextDouble())); };
    // hue shares measured on the reference: white, yellow, cyan-blue, blue, orange, violet, magenta, red
    const float shares[8] { 0.34f, 0.20f, 0.16f, 0.10f, 0.09f, 0.07f, 0.025f, 0.015f };
    sparks.resize (5600);
    for (auto& s : sparks)
    {
        s.birth = rng.nextFloat();
        s.life = jmin (0.6f, 0.035f + (float) (-std::log (jmax (1.0e-6, rng.nextDouble()))) * 0.085f);
        // uniform over the ring's area
        TrackPos tp {};
        do
        {
            s.x = cx - halfLength - outerRadius + rng.nextFloat() * 2.0f * (halfLength + outerRadius);
            s.y = cy - outerRadius + rng.nextFloat() * 2.0f * outerRadius;
            tp = trackPos (s.x, s.y);
        } while (tp.r < pillRadius + 3.0f || tp.r > outerRadius - 2.0f);
        // counter-clockwise drift + scatter; the outer sparks are pulled inwards
        // measured: faster further out (~-70 / -89 / -108 px/s at r 53-70 / 70-90 / 90-112), radial scatter, inward
        // pull from mid-radius
        const float vt = 75.0f + 1.0f * (tp.r - 53.0f) + 30.0f * gauss();
        const float vr = 55.0f * gauss() - (tp.r > 70.0f ? 5.0f * (tp.r - 70.0f) : 0.0f);
        s.vx = vt * tp.ny + vr * tp.nx;
        s.vy = -vt * tp.nx + vr * tp.ny;
        s.size = jlimit (0.3f, 5.0f, 0.68f * std::exp (0.9f * gauss()));
        s.bright = 0.8f + 0.2f * rng.nextFloat();
        float pick = rng.nextFloat(), acc = 0.0f;
        s.hue = 7;
        for (int h = 0; h < 8; ++h) { acc += shares[h]; if (pick < acc) { s.hue = h; break; } }
        s.streak = std::hypot (s.vx, s.vy) > 190.0f && rng.nextFloat() < 0.5f;
    }
}

void Visualizer::advance (bool audio, float mix, bool animate)
{
    const float dt = 1.0f / 30.0f;
    colourMix = mix;
    if (audio) silentFor = 0.0; else silentFor += dt;
    const float target = (! animate) ? 0.0f : (audio ? 1.0f : (silentFor < 0.55 ? 1.0f : 0.0f));
    if (target > level) level = 1.0f;                                  // appears at once
    else level = jmax (target, level - dt / 0.35f);                    // fades out over 0.35 s
    clock += dt;                                                       // free-running, like the reference's loop
    if (level > 0.0f || target > 0.0f) repaint();
}

// glass from a measured field (stations around the track x 1 px radial steps), bilinear, 2 x 2 supersampled
template <typename Table>
static Image renderGlass (Rectangle<float> b, float scale, const Table& rgb, const int (&seg)[4], int ns, int r0, int r1)
{
    const int w = jmax (1, roundToInt (b.getWidth() * scale)), h = jmax (1, roundToInt (b.getHeight() * scale));
    Image img (Image::ARGB, w, h, true, SoftwareImageType());
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    const int nr = r1 - r0 + 1;
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < w; ++px)
        {
            float acc[3] {};
            for (int sy = 0; sy < 2; ++sy)
                for (int sx = 0; sx < 2; ++sx)
                {
                    const float x = b.getX() + ((float) px + 0.25f + 0.5f * (float) sx) / scale;
                    const float y = b.getY() + ((float) py + 0.25f + 0.5f * (float) sy) / scale;
                    const auto tp = trackPos (x, y);
                    if (tp.r > (float) r1 + 0.5f) { for (auto& v : acc) v += 2.0f; continue; }   // panel outside (grey 2)
                    // stations per segment differ (fine on the curved ends): position in station units
                    const int sg = jlimit (0, 3, (int) tp.t);
                    int offset = 0;
                    for (int k = 0; k < sg; ++k) offset += seg[k];
                    const float fi = (float) offset + (tp.t - (float) sg) * (float) seg[sg] - 0.5f;
                    const int i0 = (int) std::floor (fi);
                    const float ft = fi - (float) i0;
                    const int ia = (i0 % ns + ns) % ns, ib = (ia + 1) % ns;
                    const float fr = jlimit (0.0f, (float) (nr - 1), tp.r - (float) r0);
                    const int j0 = jmin (nr - 2, (int) fr);
                    const float fj = fr - (float) j0;
                    for (int c = 0; c < 3; ++c)
                    {
                        const float v00 = rgb[ia][j0][c], v01 = rgb[ia][j0 + 1][c], v10 = rgb[ib][j0][c], v11 = rgb[ib][j0 + 1][c];
                        acc[c] += (v00 * (1 - fj) + v01 * fj) * (1 - ft) + (v10 * (1 - fj) + v11 * fj) * ft;
                    }
                }
            bd.setPixelColour (px, py, Colour ((uint8) (acc[0] * 0.25f), (uint8) (acc[1] * 0.25f), (uint8) (acc[2] * 0.25f)));
        }
    return img;
}

void Visualizer::renderFrame (float scale)
{
    const auto b = getBounds().toFloat();                          // editor design coordinates
    frame = renderGlass (b, scale, glassmodel::rgb, glassmodel::segStations, glassmodel::stations, glassmodel::r0, glassmodel::r1);
    lit = renderGlass (b, scale, glasslitmodel::rgb, glasslitmodel::segStations, glasslitmodel::stations, glasslitmodel::r0, glasslitmodel::r1);
    litGrey = lit.createCopy();
    {
        Image::BitmapData bd (litGrey, Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                const auto v = (uint8) jmin (255, (c.getRed() + c.getGreen() + c.getBlue()) / 3);
                bd.setPixelColour (x, y, Colour (v, v, v));
            }
    }
    for (auto* img : { &frame, &lit, &litGrey })
    {
        // the amount pill on top: black inside, a thin grey edge with a faint glow (measured: peak ~ grey 130,
        // about 0.7 px wide)
        Graphics g (*img);
        g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()).scaled (scale));
        auto pill = stadiumAround (pillCx, pillCy, pillHalfLength, pillR);
        g.setColour (Colours::black);
        g.fillPath (pill);
        g.setColour (Colour (0x1f8a8889));
        g.strokePath (pill, PathStrokeType (2.2f));
        g.setColour (Colour (0xff8c8a8b));
        g.strokePath (pill, PathStrokeType (0.75f));
    }
    framePx = scale;
}

void Visualizer::paint (Graphics& g)
{
    const float scale = theme::pixelScale (g, *this);
    if (std::abs (scale - framePx) > 0.001f || frame.isNull()) renderFrame (scale);
    const auto b = getBounds().toFloat();
    g.drawImage (frame, getLocalBounds().toFloat());
    if (level <= 0.0f) return;
    {
        // the glass lit by the sparks (measured as the median of the reference's active frames); desaturated with
        // the sparks' colour mix
        Graphics::ScopedSaveState s0 (g);
        g.setOpacity (level * colourMix);
        g.drawImage (lit, getLocalBounds().toFloat());
        g.setOpacity (level * (1.0f - colourMix));
        g.drawImage (litGrey, getLocalBounds().toFloat());
    }

    // saturated versions of the measured hue groups (white, yellow, cyan-blue, blue, orange, violet, magenta, red)
    static const Colour palette[8] { Colour (0xfff5eee1), Colour (0xffffd23c), Colour (0xff5ac8ff), Colour (0xff5064ff),
                                     Colour (0xffff8c1e), Colour (0xffa06eff), Colour (0xffff3c78), Colour (0xffff3c28) };
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));

    // while the sparks show, the pill rim and the outer edge catch their light
    g.setColour (Colours::white.withAlpha (0.3f * level));
    g.strokePath (stadiumAround (cx, cy, halfLength, pillRadius), PathStrokeType (1.4f));
    g.setColour (Colours::white.withAlpha (0.07f * level));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 0.5f), PathStrokeType (1.2f));

    Graphics::ScopedSaveState s (g);
    auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 1.0f);
    ring.setUsingNonZeroWinding (false);
    ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.0f));
    g.reduceClipRegion (ring);
    // 30 frames per second, looping every second
    const float t = (float) (std::floor (std::fmod (clock, 1.0) * 30.0) / 30.0);
    for (auto& sp : sparks)
    {
        float age = t - sp.birth;
        if (age < 0.0f) age += 1.0f;
        if (age > sp.life) continue;
        const float env = std::sqrt (std::sin (MathConstants<float>::pi * age / sp.life));
        const Point<float> pos (sp.x + sp.vx * age, sp.y + sp.vy * age);
        const auto tp = trackPos (pos.x, pos.y);
        if (tp.r < pillRadius + 1.0f || tp.r > outerRadius - 1.0f) continue;
        const auto hue = Colours::white.interpolatedWith (palette[sp.hue], colourMix);
        const float a = level * env * sp.bright, sz = sp.size;
        if (sp.streak)
        {
            // motion blur over a frame
            const Point<float> tail (pos.x - sp.vx / 75.0f, pos.y - sp.vy / 75.0f);
            g.setColour (hue.withAlpha (0.55f * a));
            g.drawLine ({ tail, pos }, sz * 0.9f);
        }
        g.setGradientFill (ColourGradient (hue.withAlpha (0.5f * a), pos, hue.withAlpha (0.0f), pos.translated (sz * 2.4f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (sz * 4.8f, sz * 4.8f).withCentre (pos));
        g.setColour (hue.withAlpha (a));
        g.fillEllipse (Rectangle<float> (sz * 1.7f, sz * 1.7f).withCentre (pos));
        if (sz > 1.0f)                                                 // only the bigger sparks burn white in the middle
        {
            g.setColour (Colours::white.withAlpha (0.8f * a));
            g.fillEllipse (Rectangle<float> (sz * 0.7f, sz * 0.7f).withCentre (pos));
        }
    }
}
}
