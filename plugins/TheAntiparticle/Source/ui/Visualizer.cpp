#include "Visualizer.h"
#include <complex>
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

    // the loop of sparks (deterministic, so it looks the same in every instance): the reference's 1 s loop of short-
    // lived sparks; ours glow longer, over a 4 s loop (as many on screen), and each follows its path round the black
    // hole in the middle, worked out here
    Random rng (20240704);
    auto gauss = [&rng] { return (float) (std::sqrt (-2.0 * std::log (jmax (1.0e-9, rng.nextDouble()))) * std::cos (MathConstants<double>::twoPi * rng.nextDouble())); };
    // hue shares measured on the reference: white, yellow, cyan-blue, blue, orange, violet, magenta, red
    const float shares[8] { 0.245f, 0.25f, 0.165f, 0.105f, 0.105f, 0.08f, 0.03f, 0.02f };
    sparks.resize (4400);
    tracks.reserve (sparks.size() * 40);
    for (auto& s : sparks)
    {
        s.birth = godLoop * rng.nextFloat();
        s.life = jmin (0.9f, 0.1f + (float) (-std::log (jmax (1.0e-6, rng.nextDouble()))) * 0.17f);
        // uniform over the ring's area
        TrackPos tp {};
        float x, y;
        do
        {
            x = cx - halfLength - outerRadius + rng.nextFloat() * 2.0f * (halfLength + outerRadius);
            y = cy - outerRadius + rng.nextFloat() * 2.0f * outerRadius;
            tp = trackPos (x, y);
        } while (tp.r < pillRadius + 3.0f || tp.r > outerRadius - 2.0f);
        // counter-clockwise drift + scatter; the outer sparks are pulled inwards
        // measured: faster further out (~-70 / -89 / -108 px/s at r 53-70 / 70-90 / 90-112), radial scatter, inward
        // pull from mid-radius
        const float vt = 75.0f + 1.0f * (tp.r - 53.0f) + 30.0f * gauss();
        const float vr = 55.0f * gauss() - (tp.r > 70.0f ? 5.0f * (tp.r - 70.0f) : 0.0f);
        float vx = vt * tp.ny + vr * tp.nx, vy = -vt * tp.nx + vr * tp.ny;
        s.size = jlimit (0.3f, 5.0f, 0.68f * std::exp (0.9f * gauss()));
        s.bright = 0.9f + 0.1f * rng.nextFloat();
        float pick = rng.nextFloat(), acc = 0.0f;
        s.hue = 7;
        for (int h = 0; h < 8; ++h) { acc += shares[h]; if (pick < acc) { s.hue = h; break; } }
        // (ours) its path: the black hole draws it in (pull ~ 1 / (distance + 8)^2, and along the flow half as much, so
        // it spirals in faster the closer); at the hole's edge it is gone
        s.first = (int) tracks.size();
        const int steps = (int) std::ceil (s.life / trackStep) + 1;
        for (int k = 0; k < steps; ++k)
        {
            tracks.push_back ({ x, y });
            const auto here = trackPos (x, y);
            const float d = here.r - pillRadius;
            if (d < 1.0f) { s.life = jmax (trackStep, (float) k * trackStep); break; }
            const float pull = 7.0e5f / ((d + 8.0f) * (d + 8.0f));
            vx += (-here.nx * pull + 0.5f * here.ny * pull) * trackStep;
            vy += (-here.ny * pull - 0.5f * here.nx * pull) * trackStep;
            x += vx * trackStep; y += vy * trackStep;
        }
        s.count = (int) tracks.size() - s.first;
    }

    // antiparticle: a 3 s loop of pair creations and annihilations, and the counter-flow of specks
    Random arng (1932);                                     // the year the positron was found
    auto inRing = [&arng] (float& x, float& y, float margin)
    {
        TrackPos tp {};
        do
        {
            x = cx - halfLength - outerRadius + arng.nextFloat() * 2.0f * (halfLength + outerRadius);
            y = cy - outerRadius + arng.nextFloat() * 2.0f * outerRadius;
            tp = trackPos (x, y);
        } while (tp.r < pillRadius + margin || tp.r > outerRadius - margin);
        return tp;
    };
    pairs.resize (30);
    for (int k = 0; k < (int) pairs.size(); ++k)
    {
        auto& pr = pairs[(size_t) k];
        pr.birth = 3.0f * ((float) k + 0.5f * arng.nextFloat()) / (float) pairs.size();
        const auto tp = inRing (pr.x, pr.y, 12.0f);
        // along the flow, so the spirals stay in the ring
        pr.dir = std::atan2 (-tp.nx, tp.ny) + (arng.nextFloat() - 0.5f) * 1.2f;
        pr.speed = 140.0f + 90.0f * arng.nextFloat();
        pr.omega = 4.0f + 5.0f * arng.nextFloat();
        pr.tau = 0.35f + 0.5f * arng.nextFloat();
        pr.life = 1.3f + 0.5f * arng.nextFloat();
    }
    annihilations.resize (18);
    for (int k = 0; k < (int) annihilations.size(); ++k)
    {
        auto& an = annihilations[(size_t) k];
        an.birth = 3.0f * ((float) k + 0.3f + 0.4f * arng.nextFloat()) / (float) annihilations.size();
        inRing (an.x, an.y, 14.0f);
        an.dir = arng.nextFloat() * MathConstants<float>::twoPi;
        an.approach = 28.0f + 22.0f * arng.nextFloat();
        an.curl = (arng.nextFloat() < 0.5f ? -1.0f : 1.0f) * (0.8f + 0.8f * arng.nextFloat());
    }
    specks.resize (1500);
    for (auto& sp : specks)
    {
        sp.anti = arng.nextFloat() < 0.5f;
        sp.birth = arng.nextFloat() * 3.0f;
        sp.life = 0.3f + 0.7f * arng.nextFloat();
        const auto tp = inRing (sp.x, sp.y, 3.0f);
        const float vt = (sp.anti ? 1.0f : -1.0f) * (40.0f + 40.0f * arng.nextFloat());   // opposite directions
        sp.vx = vt * tp.ny;
        sp.vy = -vt * tp.nx;
        sp.size = 0.5f + 0.8f * arng.nextFloat();
    }
}

void Visualizer::setTempo (float bpm)
{
    paceTarget = bpm > 0.0f ? jlimit (0.5f, 1.75f, bpm / 120.0f) : 1.0f;
}

// (ours) the animation's clock: 60 frames/s, at the tempo's pace, easing to a new one over ~0.5 s
void Visualizer::timerCallback()
{
    const float dt = (float) getTimerInterval() * 0.001f;
    pace += (paceTarget - pace) * (1.0f - std::exp (-dt / 0.5f));
    clock += (double) (dt * pace);
    if (level > 0.0f || glow > 0.0f) repaint();
    else { stopTimer(); animating = false; }
}

void Visualizer::advance (float inputPeakDb, float mix, bool animate, bool antiparticle)
{
    antiStyle = antiparticle;
    const float dt = 1.0f / 30.0f;
    colourMix = mix;
    // the reference's light follows a peak meter on the input: up at once, down 90 dB/s. Both layers are functions of
    // v = (meter + 75) / 35: the glass glow v^1.6 (quick at first, soft tail), the sparks ((v - 0.4) / 0.6)^0.7 (they
    // go out first: after a stop they last ~0.25 s of the glow's ~0.4 s; steady at -55 dBFS about half are lit)
    // (it falls with the time that really passed: a busy message thread must not stretch the fade)
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float elapsed = lastAdvance > 0.0 ? (float) juce::jlimit (0.0, 0.25, now - lastAdvance) : dt;
    lastAdvance = now;
    meterDb = jmax (inputPeakDb, jmax (-200.0f, meterDb - 90.0f * elapsed));
    const float v = jlimit (0.0f, 1.0f, (meterDb + 75.0f) / 35.0f);
    const bool wasOn = level > 0.0f || glow > 0.0f;
    if (! animate) { level = glow = 0.0f; }
    else
    {
        level = std::pow (jlimit (0.0f, 1.0f, (v - 0.4f) / 0.6f), 0.7f);
        glow = std::pow (v, 1.6f);
    }
    if ((level > 0.0f || glow > 0.0f) && ! animating) { animating = true; startTimerHz (60); }
    if (wasOn || level > 0.0f || glow > 0.0f) repaint();
}

// The glass: our vector drawing rendered as a field (resources/glass/*.png from tools/fit/draw_glass.py and its
// glass_params.json: rim and sheen lines, the reflections as soft-edged shaded polygons, the sparks' light as our own
// streaks; shaped to match the reference's look): x = station around the track (25 per
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
    litViolet = litGrey.createCopy();
    {
        // antiparticle: the lit glow's brightness, tinted violet (between the cyan and magenta tracks)
        Image::BitmapData bd (litViolet, Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const float v = (float) bd.getPixelColour (x, y).getRed();
                bd.setPixelColour (x, y, Colour ((uint8) jmin (255.0f, v * 0.6f), (uint8) jmin (255.0f, v * 0.42f), (uint8) jmin (255.0f, v * 0.95f)));
            }
    }
    for (auto* img : { &frame, &lit, &litGrey, &litViolet })
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
    if (frame.isNull() || (std::abs (scale - framePx) > 0.001f && ! theme::liveResizing (*this))) renderFrame (scale);
    g.drawImage (frame, getLocalBounds().toFloat());
    if (level > 0.0f || glow > 0.0f)
    {
        Graphics::ScopedSaveState s (g);
        if (antiStyle) paintAntiparticle (g); else paintGodly (g);
    }
    paintGlass (g);
}

// (ours) clear glass over it all: a faint sheen across the top of the ring (and fainter along the bottom), a streak of
// light along its upper outer curve, brightest up and to the left, and thin bright edges where the glass turns, at its
// outer and inner rims (a little brighter while it is lit)
void Visualizer::paintGlass (Graphics& g)
{
    const auto b = getBounds().toFloat();
    Graphics::ScopedSaveState s (g);
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));
    const float top = cy - outerRadius, bottom = cy + outerRadius;
    {
        Graphics::ScopedSaveState s1 (g);
        auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 1.0f);
        ring.setUsingNonZeroWinding (false);
        ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.0f));
        g.reduceClipRegion (ring);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.075f), cx, top, Colours::white.withAlpha (0.0f), cx, cy - pillRadius, false));
        g.fillPath (ring);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), cx, cy + pillRadius, Colours::white.withAlpha (0.03f), cx, bottom, false));
        g.fillPath (ring);
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.3f), cx - halfLength - 50.0f, top + 12.0f,
                                       Colours::white.withAlpha (0.0f), cx + 0.6f * halfLength, cy + 10.0f, false));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 6.5f), PathStrokeType (1.8f));
    g.setColour (Colours::white.withAlpha (0.1f + 0.1f * glow));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 1.6f), PathStrokeType (0.8f));
    g.setColour (Colours::white.withAlpha (0.07f + 0.08f * glow));
    g.strokePath (stadiumAround (cx, cy, halfLength, pillRadius + 2.4f), PathStrokeType (0.7f));
}

void Visualizer::paintAntiparticle (Graphics& g)
{
    const auto b = getBounds().toFloat();
    {
        Graphics::ScopedSaveState s0 (g);
        g.setOpacity (0.65f * glow * colourMix);                     // (ours: lighter, clearer glass)
        g.drawImage (litViolet, getLocalBounds().toFloat());
        g.setOpacity (0.65f * glow * (1.0f - colourMix));
        g.drawImage (litGrey, getLocalBounds().toFloat());
    }
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));
    g.setColour (Colour (0xffb07cff).withAlpha (0.25f * glow));
    g.strokePath (stadiumAround (pillCx, pillCy, pillHalfLength, pillR), PathStrokeType (1.4f));

    Graphics::ScopedSaveState s (g);
    auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 1.0f);
    ring.setUsingNonZeroWinding (false);
    ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.0f));
    g.reduceClipRegion (ring);

    const Colour matter = Colours::white.interpolatedWith (Colour (0xff7cf0ff), colourMix);
    const Colour antimatter = Colours::white.interpolatedWith (Colour (0xffff5ad2), colourMix);
    const Colour gammaCol = Colours::white.interpolatedWith (Colour (0xfffff3c4), colourMix);
    const float loop = 3.0f;
    const float t = (float) std::fmod (clock, (double) loop);
    auto ageOf = [t, loop] (float birth) { float a = t - birth; return a < 0.0f ? a + loop : a; };
    auto glowDot = [&g] (Point<float> p, Colour c, float r, float a)
    {
        g.setGradientFill (ColourGradient (c.withAlpha (0.55f * a), p, c.withAlpha (0.0f), p.translated (r * 3.0f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (p));
        g.setColour (c.withAlpha (a));
        g.fillEllipse (Rectangle<float> (r * 1.6f, r * 1.6f).withCentre (p));
        g.setColour (Colours::white.withAlpha (0.8f * a));
        g.fillEllipse (Rectangle<float> (r * 0.7f, r * 0.7f).withCentre (p));
    };

    // the counter-flow: matter one way, antimatter the other
    for (auto& sp : specks)
    {
        const float age = ageOf (sp.birth);
        if (age > sp.life) continue;
        const float a = level * 0.85f * std::sin (MathConstants<float>::pi * age / sp.life);
        const Point<float> p (sp.x + sp.vx * age, sp.y + sp.vy * age);
        g.setColour ((sp.anti ? antimatter : matter).withAlpha (a));
        g.fillEllipse (Rectangle<float> (2.0f * sp.size, 2.0f * sp.size).withCentre (p));
    }

    // pair creation: z(s) = z0 + v0 e^{i dir} (e^{(-1/tau + i w) s} - 1) / (-1/tau + i w), w = +omega / -omega
    using C = std::complex<float>;
    for (auto& pr : pairs)
    {
        const float age = ageOf (pr.birth);
        if (age > pr.life + 0.2f) continue;
        const C z0 (pr.x, pr.y);
        // the incoming gamma ray: a dashed streak arriving in the last 0.2 s before the split
        if (age < 0.2f || age > pr.life)
        {
            const float before = age > pr.life ? age - loop : age;      // negative: still on its way
            if (before < 0.0f && before > -0.2f)
            {
                const C dirv = std::polar (1.0f, pr.dir + MathConstants<float>::pi);
                const C head = z0 + dirv * (-before * 450.0f), tail = head + dirv * 26.0f;
                g.setColour (gammaCol.withAlpha (0.7f * level));
                Path ray; ray.startNewSubPath (head.real(), head.imag()); ray.lineTo (tail.real(), tail.imag());
                const float dashes[] { 3.0f, 2.5f };
                Path dashed; PathStrokeType (0.8f).createDashedStroke (dashed, ray, dashes, 2);
                g.fillPath (dashed);
            }
            if (age > pr.life) continue;
        }
        if (age < 0.0f) continue;
        // vertex flash
        if (age < 0.12f) glowDot ({ pr.x, pr.y }, gammaCol, 2.2f * (1.0f - age / 0.12f) + 0.6f, level);
        for (int q = 0; q < 2; ++q)
        {
            const float w = q == 0 ? pr.omega : -pr.omega;
            const C k (-1.0f / pr.tau, w);
            const C start = std::polar (pr.speed, pr.dir + (q == 0 ? 0.25f : -0.25f));
            auto pos = [&] (float u) { const C z = z0 + start * (std::exp (k * u) - C (1.0f, 0.0f)) / k; return Point<float> (z.real(), z.imag()); };
            const auto col = q == 0 ? matter : antimatter;
            const float fade = level * jlimit (0.0f, 1.0f, (pr.life - age) / 0.4f);
            // the track: the last 0.7 s of the path, fading towards its old end
            const float s0 = jmax (0.0f, age - 0.7f);
            const int n = 28;
            Point<float> prev = pos (s0);
            for (int i = 1; i <= n; ++i)
            {
                const float sc = s0 + (age - s0) * (float) i / (float) n;
                const auto pt = pos (sc);
                g.setColour (col.withAlpha (fade * 0.95f * (float) i / (float) n));
                g.drawLine ({ prev, pt }, 1.1f);
                prev = pt;
            }
            glowDot (prev, col, 1.5f, fade);
        }
    }

    // annihilation: two curling tracks meet, a flash, two gamma rays fly apart
    for (auto& an : annihilations)
    {
        const float age = ageOf (an.birth);                  // 0 .. 0.5 s approach, then the flash
        if (age > 0.95f) continue;
        const Point<float> m (an.x, an.y);
        if (age < 0.5f)
        {
            const float u = age / 0.5f;
            for (int q = 0; q < 2; ++q)
            {
                const float side = q == 0 ? 1.0f : -1.0f;
                const auto col = q == 0 ? matter : antimatter;
                Point<float> prev;
                for (int i = 0; i <= 18; ++i)
                {
                    const float uu = jmax (0.0f, u - 0.45f) + (u - jmax (0.0f, u - 0.45f)) * (float) i / 18.0f;
                    const float d = an.approach * (1.0f - uu);
                    const float ang = an.dir + MathConstants<float>::halfPi * side + an.curl * (1.0f - uu) * side;
                    const Point<float> pt (m.x + d * std::cos (ang), m.y + d * std::sin (ang));
                    if (i > 0) { g.setColour (col.withAlpha (level * 0.7f * (float) i / 18.0f)); g.drawLine ({ prev, pt }, 0.9f); }
                    prev = pt;
                }
                glowDot (prev, col, 1.0f, level);
            }
        }
        else
        {
            const float u = (age - 0.5f) / 0.45f;            // 0 .. 1
            // the flash: a bright core and an expanding ring
            glowDot (m, gammaCol, 3.2f * (1.0f - u) + 0.5f, level * (1.0f - u));
            g.setColour (gammaCol.withAlpha (level * 0.5f * (1.0f - u)));
            g.drawEllipse (Rectangle<float> (2.0f + 30.0f * u, 2.0f + 30.0f * u).withCentre (m), 1.0f);
            // two gamma rays, back to back
            for (float side : { 1.0f, -1.0f })
            {
                const Point<float> head (m.x + side * 160.0f * u * std::cos (an.dir), m.y + side * 160.0f * u * std::sin (an.dir));
                const Point<float> tail (head.x - side * 22.0f * std::cos (an.dir), head.y - side * 22.0f * std::sin (an.dir));
                Path ray; ray.startNewSubPath (tail); ray.lineTo (head);
                const float dashes[] { 3.0f, 2.5f };
                Path dashed; PathStrokeType (0.8f).createDashedStroke (dashed, ray, dashes, 2);
                g.setColour (gammaCol.withAlpha (level * 0.8f * (1.0f - u)));
                g.fillPath (dashed);
            }
        }
    }
}

void Visualizer::paintGodly (Graphics& g)
{
    const auto b = getBounds().toFloat();
    {
        // the glass lit by the sparks (drawn to match the reference's active frames on average); desaturated with
        // the sparks' colour mix
        Graphics::ScopedSaveState s0 (g);
        g.setOpacity (0.65f * glow * colourMix);                     // (ours: lighter, clearer glass)
        g.drawImage (lit, getLocalBounds().toFloat());
        g.setOpacity (0.65f * glow * (1.0f - colourMix));
        g.drawImage (litGrey, getLocalBounds().toFloat());
    }

    // saturated versions of the measured hue groups (white, yellow, cyan-blue, blue, orange, violet, magenta, red)
    // per hue group: between the reference's measured spark cores and fully saturated (our white core and halo
    // wash small sparks out a little, so the drawn colour sits above the measured one)
    static const Colour palette[8] { Colour (0xfff6f9ff), Colour (0xffffd84a), Colour (0xff5ed2ff), Colour (0xff5a6eff),
                                     Colour (0xffff9024), Colour (0xffa678ff), Colour (0xffff4078), Colour (0xffff422c) };
    g.addTransform (AffineTransform::translation (-b.getX(), -b.getY()));

    // while the sparks show, the outer edge catches their light
    g.setColour (Colours::white.withAlpha (0.07f * glow));
    g.strokePath (stadiumAround (cx, cy, halfLength, outerRadius - 0.5f), PathStrokeType (1.2f));

    {
        Graphics::ScopedSaveState s (g);
        auto ring = stadiumAround (cx, cy, halfLength, outerRadius - 1.0f);
        ring.setUsingNonZeroWinding (false);
        ring.addPath (stadiumAround (cx, cy, halfLength, pillRadius + 1.0f));
        g.reduceClipRegion (ring);
        // (ours) the sparks' light added up as light is (where they crowd it burns whiter), then laid over the glass:
        // each a core a little whiter than its colour, a halo of the pure colour and a wide faint bloom
        const float scale = theme::pixelScale (g, *this);
        const int w = jmax (1, roundToInt (b.getWidth() * scale)), h = jmax (1, roundToInt (b.getHeight() * scale));
        lightBuf.assign ((size_t) (w * h * 3), 0.0f);
        float gx[256], gy[256];
        auto addGlow = [&] (Point<float> at, float sigma, float k, Colour c)
        {
            const float px = (at.x - b.getX()) * scale, py = (at.y - b.getY()) * scale;
            const float sg = jlimit (0.35f, 48.0f, sigma * scale), R = 2.5f * sg;
            const int i0 = jmax (0, (int) (px - R)), i1 = jmin (w - 1, (int) (px + R));
            const int j0 = jmax (0, (int) (py - R)), j1 = jmin (h - 1, (int) (py + R));
            if (i0 > i1 || j0 > j1) return;
            for (int i = i0; i <= i1; ++i) { const float q = ((float) i + 0.5f - px) / sg; gx[i - i0] = std::exp (-0.5f * q * q); }
            for (int j = j0; j <= j1; ++j) { const float q = ((float) j + 0.5f - py) / sg; gy[j - j0] = k * std::exp (-0.5f * q * q); }
            const float cr = c.getFloatRed(), cg = c.getFloatGreen(), cb = c.getFloatBlue();
            for (int j = j0; j <= j1; ++j)
            {
                float* px3 = lightBuf.data() + (size_t) ((j * w + i0) * 3);
                const float wy = gy[j - j0];
                for (int i = i0; i <= i1; ++i, px3 += 3)
                {
                    const float wgt = wy * gx[i - i0];
                    px3[0] += wgt * cr; px3[1] += wgt * cg; px3[2] += wgt * cb;
                }
            }
        };
        const float t = (float) std::fmod (clock, (double) godLoop);
        for (auto& sp : sparks)
        {
            float age = t - sp.birth;
            if (age < 0.0f) age += godLoop;
            if (age > sp.life || sp.count < 2) continue;
            // where it is on its path, and how fast it goes
            const float f = age / trackStep;
            const int k = jlimit (0, sp.count - 2, (int) f);
            const auto& p0 = tracks[(size_t) (sp.first + k)];
            const auto& p1 = tracks[(size_t) (sp.first + k + 1)];
            const float u = jlimit (0.0f, 1.0f, f - (float) k);
            const Point<float> pos = p0 + (p1 - p0) * u, vel = (p1 - p0) / trackStep;
            const auto tp = trackPos (pos.x, pos.y);
            if (tp.r < pillRadius + 1.0f || tp.r > outerRadius - 1.0f) continue;
            // glowing up and dying away, like a neon tube switched on and off
            const float env = std::pow (std::sin (MathConstants<float>::pi * age / sp.life), 0.6f);
            const auto hue = Colours::white.interpolatedWith (palette[sp.hue], colourMix);
            const float d = tp.r - pillRadius, thin = jlimit (0.35f, 1.0f, 0.3f + d / 25.0f);   // (thinner near the hole)
            const float a = level * env * sp.bright, sz = sp.size * thin;
            addGlow (pos, 3.5f + 1.5f * sp.size * thin, 0.1f * a, hue);                        // (the bloom)
            addGlow (pos, (1.1f + 0.9f * sp.size) * thin, 0.45f * a, hue);                       // (the halo)
            // the core, drawn out along its way (1/120 s exposure; longer as the hole speeds it up)
            const Point<float> half = vel * (1.0f / 240.0f);
            const int n = 1 + jmin (12, (int) (2.0f * half.getDistanceFromOrigin() / 0.6f));
            const auto core = hue.interpolatedWith (Colours::white, sz > 0.9f ? 0.35f : 0.15f);
            for (int q = 0; q < n; ++q)
                addGlow (n == 1 ? pos : pos - half + half * (2.0f * (float) q / (float) (n - 1)), 0.3f + 0.4f * sz,
                         1.1f * a / std::sqrt ((float) n), core);
            // its reflections in the glass: the rims are curved mirrors, so each image is squeezed against the rim
            // (a tenth of its distance from it) and drawn out along it, fainter the further the spark is
            const Point<float> out (tp.nx, tp.ny), along (tp.ny, -tp.nx);
            const float dOut = outerRadius - tp.r;
            for (int m = 0; m < 2; ++m)
            {
                const float dist = m == 0 ? dOut : d;
                const float rr = m == 0 ? outerRadius - 1.8f - 0.1f * dist : pillRadius + 1.8f + 0.1f * dist;
                const float kr = (m == 0 ? 0.7f : 0.55f) * a * std::exp (-dist / 40.0f);
                if (kr < 0.004f) continue;
                const Point<float> at = pos + out * (rr - tp.r);
                for (float o : { -2.6f, -1.3f, 0.0f, 1.3f, 2.6f })
                    addGlow (at + along * o, 0.6f, (1.0f - 0.25f * std::abs (o)) * 0.5f * kr, core);
            }
        }
        // as an image over the glass: the light itself (past white it spills into the other channels), as opaque as
        // it is bright
        if (lightImg.getWidth() != w || lightImg.getHeight() != h) lightImg = Image (Image::ARGB, w, h, true, SoftwareImageType());
        {
            Image::BitmapData bd (lightImg, Image::BitmapData::writeOnly);
            for (int j = 0; j < h; ++j)
            {
                const float* l = lightBuf.data() + (size_t) (j * w * 3);
                for (int i = 0; i < w; ++i, l += 3)
                {
                    float r = l[0], gg = l[1], bb = l[2];
                    const float m = jmax (r, gg, bb);
                    if (m > 1.0f) { const float spill = (m - 1.0f) * 0.5f; r += spill; gg += spill; bb += spill; }
                    r = jmin (1.0f, r); gg = jmin (1.0f, gg); bb = jmin (1.0f, bb);
                    const float al = jmax (r, gg, bb);
                    reinterpret_cast<PixelARGB*> (bd.getPixelPointer (i, j))->setARGB ((uint8) (255.0f * al), (uint8) (255.0f * r),
                                                                                       (uint8) (255.0f * gg), (uint8) (255.0f * bb));
                }
            }
        }
        g.setOpacity (1.0f);                                        // (drawImage takes the current colour's alpha)
        g.drawImage (lightImg, b);
    }

    // (ours) the black hole's edge: a soft glow of the light falling in, and a thin ring of bent light hugging the pill,
    // brighter on one side, turning with the flow
    const auto pill = stadiumAround (pillCx, pillCy, pillHalfLength, pillR + 0.6f);
    const float turn = (float) std::fmod (clock * -0.9, MathConstants<double>::twoPi);
    const Point<float> centre (pillCx, pillCy), across (std::cos (turn) * (pillHalfLength + pillR), std::sin (turn) * pillR);
    const Colour warm = Colours::white.interpolatedWith (Colour (0xffffd9a0), colourMix);
    g.setColour (warm.withAlpha (0.09f * glow));
    g.strokePath (stadiumAround (pillCx, pillCy, pillHalfLength, pillR + 3.5f), PathStrokeType (6.0f));
    g.setColour (warm.withAlpha (0.16f * glow));
    g.strokePath (stadiumAround (pillCx, pillCy, pillHalfLength, pillR + 1.6f), PathStrokeType (2.4f));
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.85f * glow), centre + across,
                                       warm.withAlpha (0.25f * glow), centre - across, false));
    g.strokePath (pill, PathStrokeType (0.9f));
}
}
