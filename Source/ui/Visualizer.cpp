#include "Visualizer.h"
#include "Theme.h"
#include "BinaryData.h"

using namespace juce;

namespace ap
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
    const float shares[8] { 0.20f, 0.26f, 0.175f, 0.115f, 0.115f, 0.085f, 0.03f, 0.02f };
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

    // devily: a 2 s loop of embers rising from the lower ring and swirling clockwise, plus dark smoke wisps
    Random erng (666);
    auto egauss = [&erng] { return (float) (std::sqrt (-2.0 * std::log (jmax (1.0e-9, erng.nextDouble()))) * std::cos (MathConstants<double>::twoPi * erng.nextDouble())); };
    embers.resize (2700);
    for (int k = 0; k < (int) embers.size(); ++k)
    {
        auto& e = embers[(size_t) k];
        e.smoke = k < 90;
        e.birth = erng.nextFloat() * 2.0f;
        e.life = e.smoke ? 1.2f + 0.8f * erng.nextFloat() : 0.35f + 0.85f * std::pow (erng.nextFloat(), 1.5f);
        TrackPos tp {};
        do
        {
            e.x = cx - halfLength - outerRadius + erng.nextFloat() * 2.0f * (halfLength + outerRadius);
            // more of them start low: embers rise
            e.y = cy - outerRadius + std::pow (erng.nextFloat(), 0.6f) * 2.0f * outerRadius;
            tp = trackPos (e.x, e.y);
        } while (tp.r < pillRadius + 3.0f || tp.r > outerRadius - 2.0f);
        const float vt = -(45.0f + 0.6f * (tp.r - 53.0f) + 20.0f * egauss());       // clockwise
        e.vx = vt * tp.ny + 18.0f * egauss();
        e.vy = -vt * tp.nx - (e.smoke ? 22.0f : 55.0f + 25.0f * erng.nextFloat()) + 12.0f * egauss();
        e.size = e.smoke ? 8.0f + 9.0f * erng.nextFloat() : jlimit (0.5f, 4.0f, 1.05f * std::exp (0.6f * egauss()));
        e.bright = 0.7f + 0.3f * erng.nextFloat();
        e.heat = 0.85f + 0.15f * erng.nextFloat();                                  // how hot it starts
        e.wobble = 2.0f + 5.0f * erng.nextFloat();
        e.wobbleHz = 2.0f + 5.0f * erng.nextFloat();
        e.phase = erng.nextFloat() * MathConstants<float>::twoPi;
        e.streak = ! e.smoke && erng.nextFloat() < 0.12f;
    }
}

void Visualizer::advance (bool audio, float mix, bool animate, bool devily)
{
    devilish = devily;
    const float dt = 1.0f / 30.0f;
    colourMix = mix;
    if (audio) silentFor = 0.0; else silentFor += dt;
    const float target = (! animate) ? 0.0f : (audio ? 1.0f : (silentFor < 0.55 ? 1.0f : 0.0f));
    if (target > level) level = 1.0f;                                  // appears at once
    else level = jmax (target, level - dt / 0.35f);                    // fades out over 0.35 s
    clock += dt;                                                       // free-running, like the reference's loop
    if (level > 0.0f || target > 0.0f) repaint();
}

// The glass: a measured field (resources/glass/*.png, tools/fit/fit_glass.py): x = station around the track (25 per
// straight, 144 per curved end), y = radius 52 .. 114 in 0.5 px steps, RGB. Interpolated with Catmull-Rom in both
// directions so the reflections' slanted edges stay smooth.
struct GlassField
{
    static constexpr int seg[4] { 25, 144, 25, 144 };
    static constexpr float r0 = 52.0f, rStep = 0.5f;
    int ns = 0, nr = 0;
    std::vector<float> rgb;                          // [r][station][3]
    explicit GlassField (const void* png, size_t size)
    {
        auto img = ImageFileFormat::loadFrom (png, size);
        ns = img.getWidth(); nr = img.getHeight();
        rgb.resize ((size_t) (ns * nr * 3));
        Image::BitmapData bd (img, Image::BitmapData::readOnly);
        for (int y = 0; y < nr; ++y)
            for (int x = 0; x < ns; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                float* p = &rgb[(size_t) ((y * ns + x) * 3)];
                p[0] = c.getRed(); p[1] = c.getGreen(); p[2] = c.getBlue();
            }
    }
    static float cr (float p0, float p1, float p2, float p3, float t)  // Catmull-Rom
    {
        return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
    }
    void at (float t, float r, float* out) const
    {
        const int sg = jlimit (0, 3, (int) t);
        int offset = 0;
        for (int k = 0; k < sg; ++k) offset += seg[k];
        const float fi = (float) offset + (t - (float) sg) * (float) seg[sg] - 0.5f;
        const int i1 = (int) std::floor (fi);
        const float ft = fi - (float) i1;
        const float fr = jlimit (0.0f, (float) (nr - 1), (r - r0) / rStep);
        const int j1 = jmin (nr - 2, (int) fr);
        const float fj = fr - (float) j1;
        for (int c = 0; c < 3; ++c)
        {
            float col[4];
            for (int dj = -1; dj <= 2; ++dj)
            {
                const int j = jlimit (0, nr - 1, j1 + dj);
                float row[4];
                for (int di = -1; di <= 2; ++di)
                {
                    const int i = ((i1 + di) % ns + ns) % ns;
                    row[di + 1] = rgb[(size_t) ((j * ns + i) * 3 + c)];
                }
                col[dj + 1] = cr (row[0], row[1], row[2], row[3], ft);
            }
            out[c] = jlimit (0.0f, 255.0f, cr (col[0], col[1], col[2], col[3], fj));
        }
    }
};

static Image renderGlass (Rectangle<float> b, float scale, const GlassField& field)
{
    const int w = jmax (1, roundToInt (b.getWidth() * scale)), h = jmax (1, roundToInt (b.getHeight() * scale));
    Image img (Image::ARGB, w, h, true, SoftwareImageType());
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    const float rMax = field.r0 + field.rStep * (float) (field.nr - 1);
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
                    if (tp.r > rMax + 0.5f) { for (auto& v : acc) v += 2.0f; continue; }   // panel outside (grey 2)
                    float c[3];
                    field.at (tp.t, tp.r, c);
                    for (int k = 0; k < 3; ++k) acc[k] += c[k];
                }
            bd.setPixelColour (px, py, Colour ((uint8) (acc[0] * 0.25f), (uint8) (acc[1] * 0.25f), (uint8) (acc[2] * 0.25f)));
        }
    return img;
}

void Visualizer::renderFrame (float scale)
{
    const auto b = getBounds().toFloat();                          // editor design coordinates
    static const GlassField off (BinaryData::glass_off_png, BinaryData::glass_off_pngSize);
    static const GlassField on (BinaryData::glass_lit_png, BinaryData::glass_lit_pngSize);
    frame = renderGlass (b, scale, off);
    lit = renderGlass (b, scale, on);
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
    litRed = litGrey.createCopy();
    {
        // hellfire: the lit glow's brightness, tinted deep red
        Image::BitmapData bd (litRed, Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const float v = (float) bd.getPixelColour (x, y).getRed();
                bd.setPixelColour (x, y, Colour ((uint8) jmin (255.0f, v * 1.05f), (uint8) jmin (255.0f, v * 0.28f), (uint8) jmin (255.0f, v * 0.16f)));
            }
    }
    for (auto* img : { &frame, &lit, &litGrey, &litRed })
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
    g.drawImage (frame, getLocalBounds().toFloat());
    if (level <= 0.0f) return;
    if (devilish) paintDevily (g); else paintGodly (g);
}

void Visualizer::paintDevily (Graphics& g)
{
    const auto b = getBounds().toFloat();
    {
        Graphics::ScopedSaveState s0 (g);
        g.setOpacity (level * colourMix);
        g.drawImage (litRed, getLocalBounds().toFloat());
        g.setOpacity (level * (1.0f - colourMix));
        g.drawImage (litGrey, getLocalBounds().toFloat());
    }
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));
    g.setColour (Colour (0xffff5a2a).withAlpha (0.28f * level * colourMix));
    g.strokePath (stadiumAround (pillCx, pillCy, pillHalfLength, pillR), PathStrokeType (1.4f));

    Graphics::ScopedSaveState s (g);
    auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 1.0f);
    ring.setUsingNonZeroWinding (false);
    ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.0f));
    g.reduceClipRegion (ring);

    // ember colour by heat: yellow-white -> orange -> red -> ember dark; ash grey when the colour mix is low
    auto heatColour = [this] (float h)
    {
        const Colour c0 (0xff5a0a08), c1 (0xffd8281a), c2 (0xffff8a28), c3 (0xfffff0b4);
        const Colour c = h < 0.33f ? c0.interpolatedWith (c1, h / 0.33f)
                       : h < 0.66f ? c1.interpolatedWith (c2, (h - 0.33f) / 0.33f)
                                   : c2.interpolatedWith (c3, (h - 0.66f) / 0.34f);
        const auto grey = (uint8) ((c.getRed() + c.getGreen() + c.getBlue()) / 3);
        return Colour (grey, grey, grey).interpolatedWith (c, colourMix);
    };
    const float t = (float) (std::floor (std::fmod (clock, 2.0) * 30.0) / 30.0);
    {
        // flames licking up from the lower half of the ring: soft orange-red glows that pulse and drift up
        const float tt = (float) clock;
        for (int i = 0; i < 18; ++i)
        {
            const float fu = (float) i / 18.0f;                                 // along the lower half
            const float ang = MathConstants<float>::pi * (0.5f + fu);           // 90 .. 270 deg round the whole track
            const float x = cx + (halfLength + 82.0f) * std::sin (ang) * (std::abs (std::sin (ang)) > 0.7f ? 1.0f : 0.95f) * 1.0f;
            const float y = cy + 86.0f * std::abs (std::cos (ang * 0.5f + 0.3f)) + 10.0f;
            const float pulse = 0.5f + 0.5f * std::sin (tt * (3.0f + (float) (i % 5)) + (float) i * 1.7f);
            const float rise = 8.0f * std::fmod (tt * (0.6f + 0.07f * (float) i) + fu, 1.0f);
            const auto c = heatColour (0.55f + 0.25f * pulse).withAlpha (level * (0.10f + 0.10f * pulse));
            const Point<float> p (x + 6.0f * std::sin (tt * 2.0f + (float) i), y - rise);
            g.setGradientFill (ColourGradient (c, p, c.withAlpha (0.0f), p.translated (22.0f + 8.0f * pulse, 0.0f), true));
            g.fillEllipse (Rectangle<float> (44.0f + 16.0f * pulse, 60.0f + 20.0f * pulse).withCentre (p));
        }
    }
    for (int pass = 0; pass < 2; ++pass)                     // smoke first, embers on top
        for (auto& e : embers)
        {
            if (e.smoke != (pass == 0)) continue;
            float age = t - e.birth;
            if (age < 0.0f) age += 2.0f;
            if (age > e.life) continue;
            const float u = age / e.life;
            const float wob = e.wobble * std::sin (e.phase + MathConstants<float>::twoPi * e.wobbleHz * age);
            const Point<float> pos (e.x + e.vx * age + wob, e.y + e.vy * age);
            const auto tp = trackPos (pos.x, pos.y);
            if (tp.r < pillRadius + 1.0f || tp.r > outerRadius - 1.0f) continue;
            if (e.smoke)
            {
                const float a = level * 0.4f * std::sin (MathConstants<float>::pi * u);
                const float sz = e.size * (0.7f + 0.6f * u);
                g.setGradientFill (ColourGradient (Colour (0xff120606).withAlpha (a), pos, Colour (0x00120606), pos.translated (sz, 0.0f), true));
                g.fillEllipse (Rectangle<float> (2.0f * sz, 2.0f * sz).withCentre (pos));
                continue;
            }
            const float heat = jlimit (0.0f, 1.0f, e.heat * (1.0f - u * 0.8f));
            const float flicker = 0.75f + 0.25f * std::sin (e.phase * 3.0f + 40.0f * age);
            const float a = level * e.bright * flicker * std::min (1.0f, 6.0f * u) * (1.0f - std::pow (u, 3.0f));
            const float sz = e.size * (1.0f - 0.5f * u);
            const auto col = heatColour (heat);
            if (e.streak)
            {
                const Point<float> tail (pos.x - e.vx / 20.0f, pos.y - e.vy / 20.0f);
                g.setColour (col.withAlpha (0.5f * a));
                g.drawLine ({ tail, pos }, sz * 0.8f);
            }
            g.setGradientFill (ColourGradient (col.withAlpha (0.55f * a), pos, col.withAlpha (0.0f), pos.translated (sz * 3.0f, 0.0f), true));
            g.fillEllipse (Rectangle<float> (sz * 6.0f, sz * 6.0f).withCentre (pos));
            g.setColour (col.withAlpha (a));
            g.fillEllipse (Rectangle<float> (sz * 1.6f, sz * 1.6f).withCentre (pos));
            if (heat > 0.6f && sz > 0.9f)
            {
                g.setColour (heatColour (1.0f).withAlpha (0.8f * a));
                g.fillEllipse (Rectangle<float> (sz * 0.7f, sz * 0.7f).withCentre (pos));
            }
        }
}

void Visualizer::paintGodly (Graphics& g)
{
    const auto b = getBounds().toFloat();
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
    // per hue group: between the reference's measured spark cores and fully saturated (our white core and halo
    // wash small sparks out a little, so the drawn colour sits above the measured one)
    static const Colour palette[8] { Colour (0xfff6f9ff), Colour (0xffffe363), Colour (0xff86e0ff), Colour (0xff8294ff),
                                     Colour (0xffffa832), Colour (0xffbc97ff), Colour (0xffff537c), Colour (0xffff543c) };
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
