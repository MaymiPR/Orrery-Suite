#include "Stripe.h"
#include "StripeGlow.h"
#include "shared/ui/Theme.h"

using namespace juce;
namespace theme = orrery::theme;

namespace sc
{
namespace
{
// the particles' colours and their shares (measured: the hues of the reference's saturated particles; about half of
// its particles read nearly white)
struct Tint { float r, g, b, share; };
constexpr Tint palette[] {
    { 1.00f, 0.96f, 0.94f, 0.18f },     // white
    { 1.00f, 0.88f, 0.72f, 0.10f },     // warm white
    { 1.00f, 0.28f, 0.06f, 0.09f },     // red-orange (hue 15)
    { 1.00f, 0.60f, 0.08f, 0.19f },     // amber (45)
    { 1.00f, 0.92f, 0.15f, 0.03f },     // yellow (75)
    { 0.05f, 0.78f, 1.00f, 0.15f },     // cyan (195)
    { 0.12f, 0.45f, 1.00f, 0.05f },     // azure (225)
    { 0.38f, 0.28f, 1.00f, 0.07f },     // blue-violet (255)
    { 0.72f, 0.25f, 1.00f, 0.01f },     // violet (285)
    { 1.00f, 0.18f, 0.40f, 0.08f } };   // pink-red (345)
constexpr Tint flareTints[] {
    { 1.00f, 0.95f, 0.80f, 0.25f }, { 1.00f, 0.90f, 0.35f, 0.25f }, { 0.25f, 0.88f, 1.00f, 0.20f },
    { 1.00f, 0.60f, 0.15f, 0.12f }, { 1.00f, 0.25f, 0.25f, 0.08f }, { 0.70f, 0.50f, 1.00f, 0.10f } };
// ours (SCORPIUS): warm in the middle (Antares crimson, red-orange, amber, warm white), cool towards the ends (the
// controls' mint, teal, ice blue, white); the stars that flare
constexpr Tint warmTints[] { { 1.00f, 0.13f, 0.07f, 0.25f }, { 1.00f, 0.32f, 0.06f, 0.25f }, { 1.00f, 0.62f, 0.12f, 0.30f },
                             { 1.00f, 0.88f, 0.74f, 0.20f } };
constexpr Tint coolTints[] { { 0.42f, 1.00f, 0.72f, 0.35f }, { 0.08f, 0.86f, 0.80f, 0.30f }, { 0.58f, 0.84f, 1.00f, 0.20f },
                             { 0.94f, 0.98f, 1.00f, 0.15f } };
constexpr Tint starTints[] { { 0.88f, 0.96f, 1.00f, 0.40f }, { 0.42f, 1.00f, 0.72f, 0.30f }, { 1.00f, 0.25f, 0.10f, 0.15f },
                             { 1.00f, 0.70f, 0.25f, 0.15f } };
constexpr float heartX = 455.0f;                               // the middle of the band (where the flares gather)
constexpr float stingerTipX = 39.0f, stingerTipY = 290.8f;     // ours: where the venom leaves the stinger
constexpr float tubeTop = 276.0f, tubeBottom = 305.5f;         // (the venom glances off these)
constexpr size_t maxDrops = 2600;
// the venom's hues (acid green, teal, electric violet, magenta, amber): each glob takes one, shifted by the drift
constexpr float venomHues[] { 0.27f, 0.47f, 0.74f, 0.88f, 0.09f };
// how warm a place is: 1 at the heart, 0 at the ends
float warmth (float x) { return std::pow (1.0f - jlimit (0.0f, 1.0f, std::abs (x - heartX) / 420.0f), 1.3f); }

constexpr int numParticles = 1700, numFlares = 10;
constexpr float lanes[] { 289.1f, 291.6f, 294.25f, 302.5f };   // the rows the band's dashes keep to (the last one less)
// where the reference's particles are (local maxima per frame, measured): along the stripe in bins of 18.5 design px
// from x 0 (thinned towards the ends, where ours stood out more: the reference has ~60 per frame in each end sixth),
// and across it in bins of 2 design px from y 272.5
constexpr float alongBins[48] { 0, 0.5f, 1.6f, 4.1f, 7.7f, 10.5f, 13.7f, 18, 22.9f, 29.1f, 36, 37, 39, 42, 43, 40, 42, 42, 47, 48, 50, 45, 49, 47,
                                55, 55, 45, 52, 46, 43, 47, 44, 42, 47, 43, 38, 38, 35, 29.1f, 22.9f, 17.2f, 12.9f, 10.5f, 7, 3.5f, 1, 0, 0 };
constexpr float acrossBins[17] { 31, 16, 21, 23, 28, 31, 47, 103, 240, 241, 209, 161, 124, 100, 88, 45, 15 };
// the same for the dots and blobs alone (the band's dashes are added on top of them)
constexpr float dotBins[17] { 3, 4, 6, 8, 10, 13, 22, 50, 90, 90, 85, 140, 90, 45, 25, 10, 3 };

float gauss (Random& r)
{
    const float u = jmax (1.0e-6f, r.nextFloat()), v = r.nextFloat();
    return std::sqrt (-2.0f * std::log (u)) * std::cos (MathConstants<float>::twoPi * v);
}

template <size_t n>
const Tint& pick (Random& r, const Tint (&tints)[n])
{
    float total = 0.0f;
    for (auto& t : tints) total += t.share;
    float u = r.nextFloat() * total;
    for (auto& t : tints)
        if ((u -= t.share) <= 0.0f) return t;
    return tints[n - 1];
}

float uniform (Random& r, float a, float b) { return a + (b - a) * r.nextFloat(); }

// a value from a histogram (bins of a width from a start), uniform within its bin
template <size_t n>
float fromBins (Random& r, const float (&bins)[n], float start, float width)
{
    float total = 0.0f;
    for (auto b : bins) total += b;
    float u = r.nextFloat() * total;
    for (size_t i = 0; i < n; ++i)
        if ((u -= bins[i]) <= 0.0f) return start + width * ((float) i + r.nextFloat());
    return start + width * (float) n;
}
}

Stripe::Stripe()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);           // (it draws what is under it itself, so the editor does not repaint 60 times a second)
    particles.resize (numParticles + numFlares);
    for (size_t i = 0; i < particles.size(); ++i)
    {
        particles[i].kind = i < (size_t) numParticles ? Kind::dot : Kind::flare;
        spawn (particles[i], 0.0, true);
    }
}

void Stripe::spawn (Particle& p, double now, bool anyAge)
{
    if (p.kind == Kind::flare)
    {
        // a flare: a narrow bar that swells to its height and shrinks again while it slides sideways
        p.x = jlimit (40.0f, 840.0f, 455.0f + 190.0f * gauss (rng));
        const auto& t = style == Style::scorpius ? pick (rng, starTints) : pick (rng, flareTints);
        p.r = t.r; p.g = t.g; p.b = t.b;
        p.y = 287.0f + 4.0f * gauss (rng);              // (mostly a little above the band)
        p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 40.0f, 180.0f);
        p.vy = 0.0f;
        p.w = uniform (rng, 2.4f, 4.0f);                  // (glowing ovals, as the reference's)
        p.h = p.w + uniform (rng, 2.0f, 11.0f);
        p.level = uniform (rng, 1.05f, 1.5f);
        p.life = uniform (rng, 0.10f, 0.22f);
        p.born = (float) (now + (anyAge ? uniform (rng, -0.2f, 0.3f) : uniform (rng, 0.0f, 0.15f)));
        p.flicker = 0.0f; p.rate = 0.0f;
        return;
    }
    p.x = fromBins (rng, alongBins, 0.0f, 18.5f);
    const auto& t = style == Style::orion ? pick (rng, palette)
                  : rng.nextFloat() < 0.08f + 0.84f * warmth (p.x) ? pick (rng, warmTints) : pick (rng, coolTints);
    p.r = t.r; p.g = t.g; p.b = t.b;
    // a dense layer of short dashes through the band (on its lanes or between them), the rest spread across as
    // measured: dots, faint dots and round blobs
    const float u = rng.nextFloat();
    if (u < 0.3f)
    {
        p.kind = Kind::dash;
        const float v = rng.nextFloat();
        p.y = v < 0.45f ? lanes[rng.nextFloat() < 0.92f ? rng.nextInt (3) : 3] + 0.12f * gauss (rng) : uniform (rng, 286.5f, 302.5f);
    }
    else
    {
        const float v = rng.nextFloat();
        p.kind = v < 0.64f ? Kind::dot : v < 0.94f ? Kind::faint : Kind::blob;
        if (style == Style::scorpius && p.kind == Kind::blob && rng.nextFloat() < 0.6f) p.kind = Kind::star;   // (ours)
        p.y = fromBins (rng, dotBins, 272.5f, 2.0f);
    }
    // moving (measured with frame-to-frame cross-correlation): in the band sideways, either way, at 90 ... 200 px/s;
    // above and below it outwards (rising above, sinking below) and sideways at up to ~100 px/s
    const float off = p.y - 291.0f;
    if (std::abs (off) < 4.5f) { p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 90.0f, 200.0f); p.vy = 0.0f; }
    else { p.vx = 50.0f * gauss (rng); p.vy = (off > 0.0f ? 1.0f : -1.0f) * uniform (rng, 0.0f, 60.0f); }
    switch (p.kind)
    {
        case Kind::dot:   p.w = uniform (rng, 0.5f, 1.0f); p.h = uniform (rng, 0.5f, 0.8f); p.level = uniform (rng, 0.6f, 1.25f); break;
        case Kind::dash:  p.w = uniform (rng, 1.8f, 5.0f); p.h = uniform (rng, 0.7f, 1.0f); p.level = uniform (rng, 0.5f, 1.0f); break;
        case Kind::faint: p.w = 0.6f; p.h = 0.6f; p.level = uniform (rng, 0.3f, 0.6f); break;
        case Kind::blob:  p.w = uniform (rng, 1.6f, 3.5f); p.h = p.w * uniform (rng, 0.8f, 1.3f); p.level = uniform (rng, 0.6f, 1.1f); break;
        case Kind::star:  p.w = uniform (rng, 3.0f, 5.5f); p.h = p.w * uniform (rng, 0.7f, 1.0f); p.level = uniform (rng, 0.7f, 1.2f); break;
        case Kind::flare: break;
    }
    p.life = uniform (rng, 0.05f, 0.3f);
    p.born = (float) (anyAge ? now - uniform (rng, 0.0f, p.life) : now);
    p.flicker = uniform (rng, 0.0f, MathConstants<float>::twoPi);
    p.rate = uniform (rng, 15.0f, 45.0f);
}

void Stripe::setInput (float levelDb, bool jumpedUp, bool anim)
{
    if (frozen) return;
    animated = anim;
    levelIn = animated ? levelDb : -200.0f;
    beat = beat || (animated && jumpedUp);
    // (moved on by the editor's clock, so the whole editor repaints once per frame)
    const double now = clock();
    if (envelope > 0.002f || brightness (levelIn) > 0.0f)
    {
        if (! moving) lastTime = now;
        moving = true;
        advance (now);
        repaint();
    }
    else if (moving)
    {
        moving = false;
        envelope = 0.0f;
        repaint();
    }
}

void Stripe::freeze (float b)
{
    frozen = true;
    envelope = b;
    heart = 0.4f;
    // a moment well into the animation, so ages and flares are spread as while it runs (ours: two volleys in flight)
    for (int i = 0; i < 60; ++i)
    {
        elapsed += 1.0 / 60.0;
        for (auto& p : particles) if (elapsed - p.born > p.life) spawn (p, elapsed, false);
        if (style == Style::scorpius)
        {
            if (i == 8 || i == 40) fire (true);
            advanceVenom (1.0f / 60.0f);
        }
    }
    repaint();
}

void Stripe::setStyle (Style s)
{
    if (s == style) return;
    style = s;
    drops.clear();
    for (auto& p : particles) spawn (p, elapsed, true);     // (each style's own colours and shapes)
    glowScale = 0.0f;                                       // (and its own glow)
    repaint();
}

void Stripe::advance (double now)
{
    const double dt = jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    elapsed += dt;
    // the light eases to the level's brightness; ours: the heart beats
    envelope += (brightness (levelIn) - envelope) * (float) (1.0 - std::exp (-dt / easing));
    heart = beat ? 1.0f : heart * (float) std::exp (-dt / 0.35);
    if (style == Style::scorpius)
    {
        if (beat && envelope > 0.02f && elapsed - lastVolley > 0.28) { fire (true); lastVolley = elapsed; }
        advanceVenom ((float) dt);
    }
    beat = false;
    for (auto& p : particles)
    {
        if (elapsed - p.born > p.life) spawn (p, elapsed, false);
        p.x += p.vx * (float) dt;
        p.y += p.vy * (float) dt;
    }
}

void Stripe::renderGlow (int w, int h, float s)
{
    glow.assign ((size_t) (w * h * 3), 0.0f);
    glowScale = s;
    namespace gl = stripeglow;
    for (int j = 0; j < h; ++j)
    {
        const float y = top + ((float) j + 0.5f) / s;
        const float u = (y - gl::rowsFrom) / gl::rowStep;
        const int r0 = jlimit (0, gl::numRows - 1, (int) std::floor (u)), r1 = jmin (gl::numRows - 1, r0 + 1);
        const float f = jlimit (0.0f, 1.0f, u - (float) r0);
        const float row = u < 0.0f || u > (float) (gl::numRows - 1) ? 0.0f : gl::rows[r0] * (1.0f - f) + gl::rows[r1] * f;
        for (int i = 0; i < w; ++i)
        {
            const float x = left + ((float) i + 0.5f) / s;
            for (int c = 0; c < 3; ++c)
            {
                const auto& b = gl::along[c];
                const float d = x - b.c;
                const float v = b.a * std::exp (-(d / b.s) * (d / b.s)) + b.b * std::exp (-std::abs (d) / b.t);
                glow[(size_t) ((j * w + i) * 3 + c)] = row * v / 255.0f;
            }
        }
    }
    glowHeart.assign (glow.size(), 0.0f);
    if (style != Style::scorpius) return;
    // ours: a dimmer haze of venom, magenta and amber at the stinger, acid green through the middle, violet at the far
    // end; a beat flares the stinger's end
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const float x = left + ((float) i + 0.5f) / s, u = jlimit (0.0f, 1.0f, (x - left) / width);
            float* px = glow.data() + (size_t) ((j * w + i) * 3);
            const float lum = 0.55f * (px[0] + px[1] + px[2]) / 3.0f;
            const auto c = Colour::fromHSV (std::fmod (0.92f + 0.55f * u, 1.0f), 0.85f, 1.0f, 1.0f);
            px[0] = lum * c.getFloatRed(); px[1] = lum * c.getFloatGreen(); px[2] = lum * c.getFloatBlue();
            const float beatShare = 1.6f * lum * std::exp (-(x - stingerTipX) / 90.0f);
            float* hp = glowHeart.data() + (size_t) ((j * w + i) * 3);
            hp[0] = beatShare * 1.0f; hp[1] = beatShare * 0.25f; hp[2] = beatShare * 0.7f;
        }
}

// adds one particle's light: a soft-edged box (its core) and a wider halo
void Stripe::splat (const Particle& p, float level, float s)
{
    const float cx = (p.x - left) * s, cy = (p.y - top) * s;
    float hw = p.w * 0.5f * s, hh = p.h * 0.5f * s;
    if (p.kind == Kind::flare)
    {
        const float u = (float) ((elapsed - p.born) / p.life);
        hh = p.h * 0.5f * s * std::sin (MathConstants<float>::pi * jlimit (0.0f, 1.0f, u));
    }
    // the small ones (measured on isolated particles): a box with a pixel of soft edge and a faint halo ~1.5 px wide at
    // a tenth of the core, separable (cheap); the big bright ones: a rounded box (a capsule when long, a disc when
    // square) that blooms wider
    const bool big = p.kind == Kind::blob || p.kind == Kind::flare;
    const float edge = jmax (0.4f, 0.3f * s), halo = (big ? 1.5f : 0.6f) * s;
    const float haloShare = big ? 0.25f : 0.07f;
    const float reach = (big ? 3.0f : 2.5f) * halo;
    const int i0 = jmax (0, (int) std::floor (cx - hw - reach)), i1 = jmin (bufW - 1, (int) std::ceil (cx + hw + reach));
    const int j0 = jmax (0, (int) std::floor (cy - hh - reach)), j1 = jmin (bufH - 1, (int) std::ceil (cy + hh + reach));
    if (i0 > i1 || j0 > j1) return;
    if (! big)
    {
        float ex[48], hx[48];
        const int n = jmin (48, i1 - i0 + 1);
        for (int i = 0; i < n; ++i)
        {
            const float d = std::abs ((float) (i0 + i) + 0.5f - cx) - hw;
            ex[i] = d <= 0.0f ? 1.0f : std::exp (-d / edge);
            hx[i] = d <= 0.0f ? 1.0f : std::exp (-d / halo);
        }
        for (int j = j0; j <= j1; ++j)
        {
            const float d = std::abs ((float) j + 0.5f - cy) - hh;
            const float ey = (1.0f - haloShare) * level * (d <= 0.0f ? 1.0f : std::exp (-d / edge));
            const float hy = haloShare * level * (d <= 0.0f ? 1.0f : std::exp (-d / halo));
            float* row = light.data() + (size_t) (j * bufW + i0) * 3;
            for (int i = 0; i < n; ++i, row += 3)
            {
                const float v = ex[i] * ey + hx[i] * hy;
                row[0] += v * p.r; row[1] += v * p.g; row[2] += v * p.b;
            }
        }
        return;
    }
    const float corner = jmin (hw, hh);
    for (int j = j0; j <= j1; ++j)
    {
        const float qy = std::abs ((float) j + 0.5f - cy) - (hh - corner);
        float* row = light.data() + (size_t) (j * bufW + i0) * 3;
        for (int i = i0; i <= i1; ++i, row += 3)
        {
            const float qx = std::abs ((float) i + 0.5f - cx) - (hw - corner);
            const float d = std::hypot (jmax (qx, 0.0f), jmax (qy, 0.0f)) + jmin (jmax (qx, qy), 0.0f) - corner;
            const float core = d <= 0.0f ? 1.0f : std::exp (-d / edge), glowAmt = d <= 0.0f ? 1.0f : std::exp (-d / halo);
            const float v = level * ((1.0f - haloShare) * core + haloShare * glowAmt);
            row[0] += v * p.r; row[1] += v * p.g; row[2] += v * p.b;
        }
    }
}

// ours: a four-pointed star, a round core with a long thin ray across and a shorter one up and down (a flare's rays
// swell and shrink with it)
void Stripe::splatStar (const Particle& p, float level, float s)
{
    float size = 1.0f;
    if (p.kind == Kind::flare) size = std::sin (MathConstants<float>::pi * jlimit (0.0f, 1.0f, (float) ((elapsed - p.born) / p.life)));
    const float span = (p.kind == Kind::flare ? p.h * 1.3f : p.w) * size;
    if (span < 0.2f) return;
    Particle ray = p;
    ray.kind = Kind::dot;                                   // (thin rays: the soft box)
    ray.w = span; ray.h = 0.45f;
    splat (ray, level * 0.8f, s);
    ray.w = 0.45f; ray.h = span * 0.6f;
    splat (ray, level * 0.8f, s);
    Particle core = p;
    core.kind = Kind::blob;
    core.w = core.h = jmax (0.8f, (p.kind == Kind::flare ? p.w * 0.8f : 1.1f) * jmax (0.4f, size));
    splat (core, level, s);
}

void Stripe::render (int w, int h, float s)
{
    if (w != bufW || h != bufH || s != glowScale)
    {
        bufW = w; bufH = h;
        renderGlow (w, h, s);
        image = Image (Image::RGB, w, h, false, SoftwareImageType());
        // what is under the stripe (the editor paints it), to add the light to
        background = Image (Image::RGB, w, h, true, SoftwareImageType());
        {
            Graphics bg (background);
            bg.addTransform (AffineTransform::translation (-left, -top).scaled (s));
            if (paintBackground) paintBackground (bg);
        }
        Image::BitmapData src (background, Image::BitmapData::readOnly);
        under.resize ((size_t) (w * h * 3));
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const auto c = src.getPixelColour (i, j);
                float* u = under.data() + (size_t) ((j * w + i) * 3);
                u[0] = c.getFloatRed(); u[1] = c.getFloatGreen(); u[2] = c.getFloatBlue();
            }
    }
    light.resize (glow.size());
    const float beatGlow = style == Style::scorpius ? 0.7f * heart * envelope : 0.0f;
    for (size_t k = 0; k < glow.size(); ++k) light[k] = glow[k] * envelope + glowHeart[k] * beatGlow;
    if (style == Style::scorpius)
    {
        renderStinger (s);
        renderVenom (s);
    }
    for (auto& p : particles)
    {
        if (style == Style::scorpius && (&p - particles.data()) % 5 != 0) continue;     // (ours: a few motes in the haze)
        const float age = (float) (elapsed - p.born);
        if (age < 0.0f || age > p.life) continue;
        float a = 1.0f;
        if (p.kind != Kind::flare)
        {
            a = jmin (1.0f, age / 0.015f) * jmin (1.0f, (p.life - age) / (0.3f * p.life));
            a *= 0.8f + 0.2f * std::sin (p.flicker + p.rate * age);
        }
        if (a * envelope <= 0.005f) continue;
        if (style == Style::scorpius)
        {
            if (p.kind == Kind::flare) continue;
            Particle mote = p;
            if (mote.kind == Kind::star) mote.kind = Kind::blob;
            const auto c = Colour::fromHSV (std::fmod (hueDrift + venomHues[(&p - particles.data()) % 5] + 0.02f * p.flicker, 1.0f), 0.7f, 1.0f, 1.0f);
            mote.r = c.getFloatRed(); mote.g = c.getFloatGreen(); mote.b = c.getFloatBlue();
            splat (mote, 0.45f * p.level * a * envelope, s);
        }
        else splat (p, p.level * a * envelope, s);
    }
    Image::BitmapData bd (image, Image::BitmapData::writeOnly);
    for (int j = 0; j < h; ++j)
    {
        const float* src = light.data() + (size_t) (j * w) * 3;
        const float* u = under.data() + (size_t) (j * w) * 3;
        for (int i = 0; i < w; ++i, src += 3, u += 3)
        {
            float r = src[0], g = src[1], b = src[2];
            // light past white spills into the other channels (bright cores turn white)
            const float m = jmax (r, g, b);
            if (m > 1.0f) { const float spill = (m - 1.0f) * 0.6f; r += spill; g += spill; b += spill; }
            // added to what is under it
            r = jmin (1.0f, r + u[0]); g = jmin (1.0f, g + u[1]); b = jmin (1.0f, b + u[2]);
            auto* px = reinterpret_cast<PixelRGB*> (bd.getPixelPointer (i, j));
            px->setARGB (255, (uint8) (r * 255.0f + 0.5f), (uint8) (g * 255.0f + 0.5f), (uint8) (b * 255.0f + 0.5f));
        }
    }
}

//==============================================================================
// ours: the venom
void Stripe::addDrop (const Drop& d)
{
    if (drops.size() < maxDrops) drops.push_back (d);
}

void Stripe::fire (bool volley)
{
    auto glob = [this] (float speed, float size, float spread, float level)
    {
        Drop d {};
        d.type = Drop::Type::glob;
        d.x = stingerTipX; d.y = stingerTipY + 0.6f * gauss (rng);
        d.vx = speed; d.vy = spread * gauss (rng);
        d.size = size; d.level = level;
        d.hue = std::fmod (hueDrift + venomHues[rng.nextInt (5)] + 0.04f * gauss (rng) + 1.0f, 1.0f);
        const auto c = Colour::fromHSV (d.hue, 0.9f, 1.0f, 1.0f);
        d.r = c.getFloatRed(); d.g = c.getFloatGreen(); d.b = c.getFloatBlue();
        d.born = (float) elapsed; d.life = uniform (rng, 1.2f, 2.0f);
        d.drag = size > 2.5f ? uniform (rng, 1.6f, 2.4f) : uniform (rng, 0.6f, 1.1f); d.gravity = uniform (rng, 4.0f, 14.0f);
        addDrop (d);
    };
    if (volley)
    {
        glob (uniform (rng, 620.0f, 860.0f), uniform (rng, 2.8f, 3.6f), 14.0f, 1.5f);
        if (rng.nextFloat() < 0.6f) glob (uniform (rng, 300.0f, 520.0f), uniform (rng, 1.4f, 2.0f), 40.0f, 1.1f);
        charge = 1.0f;
        Drop f {};                                                         // the muzzle flash at the stinger
        f.type = Drop::Type::flash; f.x = stingerTipX + 4.0f; f.y = stingerTipY; f.size = 9.0f; f.level = 1.4f;
        f.r = 1.0f; f.g = 0.45f; f.b = 0.85f; f.born = (float) elapsed; f.life = 0.14f;
        addDrop (f);
    }
    else glob (uniform (rng, 240.0f, 480.0f), uniform (rng, 1.0f, 1.8f), 30.0f, 0.9f);
}

void Stripe::burst (const Drop& g)
{
    const int n = (int) (6.0f + 3.0f * g.size * g.size);
    for (int k = 0; k < n; ++k)
    {
        Drop s {};
        s.type = rng.nextFloat() < 0.15f ? Drop::Type::drip : Drop::Type::spark;
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 40.0f, 230.0f) * (0.6f + 0.25f * g.size);
        s.x = g.x; s.y = g.y; s.vx = v * std::cos (a) + 0.25f * g.vx; s.vy = 0.45f * v * std::sin (a);
        s.size = uniform (rng, 0.6f, 1.3f); s.level = uniform (rng, 0.8f, 1.4f);
        s.hue = std::fmod (g.hue + 0.04f * gauss (rng) + 1.0f, 1.0f);
        const auto c = Colour::fromHSV (s.hue, rng.nextFloat() < 0.2f ? 0.3f : 0.85f, 1.0f, 1.0f);
        s.r = c.getFloatRed(); s.g = c.getFloatGreen(); s.b = c.getFloatBlue();
        s.born = (float) elapsed; s.life = uniform (rng, 0.25f, 0.7f);
        s.drag = uniform (rng, 0.15f, 0.35f); s.gravity = s.type == Drop::Type::drip ? 160.0f : 60.0f;
        addDrop (s);
    }
    Drop f {};                                                             // the splash's flash
    f.type = Drop::Type::flash; f.x = g.x; f.y = g.y; f.size = 4.0f + 2.0f * g.size; f.level = 1.2f;
    f.r = g.r; f.g = g.g; f.b = g.b; f.born = (float) elapsed; f.life = 0.12f;
    addDrop (f);
}

void Stripe::advanceVenom (float dt)
{
    hueDrift = std::fmod (hueDrift + 0.018f * dt, 1.0f);                   // (a full turn of colours in about a minute)
    charge *= std::exp (-dt / 0.25f);
    // between beats it spits small drops, more the louder it is
    if (envelope > 0.03f)
    {
        spitDue -= dt * (0.5f + 2.5f * envelope);
        if (spitDue <= 0.0f) { fire (false); spitDue += uniform (rng, 0.4f, 1.6f); }
    }
    const size_t live = drops.size();
    for (size_t i = 0; i < live; ++i)
    {
        Drop d = drops[i];
        const float age = (float) elapsed - d.born;
        if (age > d.life) continue;
        const float px = d.x, py = d.y;
        const float k = std::exp (-dt / d.drag);
        d.vx *= k; d.vy = d.vy * k + d.gravity * dt;
        d.x += d.vx * dt; d.y += d.vy * dt;
        if (d.type == Drop::Type::glob || d.type == Drop::Type::spark)
        {
            // glancing off the tube's walls
            if (d.y < tubeTop) { d.y = 2.0f * tubeTop - d.y; d.vy = -0.6f * d.vy; }
            if (d.y > tubeBottom) { d.y = 2.0f * tubeBottom - d.y; d.vy = -0.5f * d.vy; }
        }
        if (d.type == Drop::Type::drip && d.y > tubeBottom) { d.y = tubeBottom; d.vy = 0.0f; d.vx *= 0.3f; }
        if (d.type == Drop::Type::glob)
        {
            // a comet tail: drops left along the way (one per ~1.5 px), shifting colour, now and then one that drips
            d.emitted += std::hypot (d.x - px, d.y - py) / 2.2f;
            for (; d.emitted >= 1.0f; d.emitted -= 1.0f)
            {
                Drop t {};
                const float f = rng.nextFloat();
                t.type = rng.nextFloat() < 0.04f ? Drop::Type::drip : Drop::Type::trail;
                t.x = px + (d.x - px) * f; t.y = py + (d.y - py) * f + 0.35f * d.size * gauss (rng);
                t.vx = 0.15f * d.vx + 8.0f * gauss (rng); t.vy = 5.0f * gauss (rng);
                t.size = d.size * uniform (rng, 0.35f, 0.6f); t.level = d.level * uniform (rng, 0.5f, 0.9f);
                t.hue = std::fmod (d.hue + 0.05f + 0.01f * gauss (rng) + 1.0f, 1.0f);
                const auto c = Colour::fromHSV (t.hue, 0.95f, 1.0f, 1.0f);
                t.r = c.getFloatRed(); t.g = c.getFloatGreen(); t.b = c.getFloatBlue();
                t.born = (float) elapsed; t.life = uniform (rng, 0.15f, 0.45f) * (0.6f + 0.2f * d.size);
                t.drag = 0.12f; t.gravity = t.type == Drop::Type::drip ? 140.0f : 0.0f;
                addDrop (t);
            }
            // spent: too slow, too old or at the far end, it bursts
            if (std::abs (d.vx) < 70.0f || age + dt > d.life || d.x > left + width - 6.0f)
            {
                burst (d);
                d.life = 0.0f;
            }
        }
        drops[i] = d;
    }
    // (the dead ones go)
    drops.erase (std::remove_if (drops.begin(), drops.end(), [this] (const Drop& d) { return (float) elapsed - d.born > d.life; }), drops.end());
}

void Stripe::renderVenom (float s)
{
    for (const auto& d : drops)
    {
        const float age = (float) elapsed - d.born, u = jlimit (0.0f, 1.0f, age / jmax (0.001f, d.life));
        Particle p {};
        p.x = d.x; p.y = d.y; p.r = d.r; p.g = d.g; p.b = d.b;
        switch (d.type)
        {
            case Drop::Type::glob:
            {
                // a glowing streak behind it (longer the faster it flies, turning colour), a bright coloured body
                // stretched by its speed, and a white-hot core
                const float speed = std::hypot (d.vx, d.vy), ux = d.vx / jmax (1.0f, speed), uy = d.vy / jmax (1.0f, speed);
                const float length = jmin (60.0f, 0.06f * speed) * (0.6f + 0.25f * d.size);
                for (int k = 1; k <= 12; ++k)
                {
                    const float f = (float) k / 12.0f;
                    const auto c = Colour::fromHSV (std::fmod (d.hue + 0.07f * f, 1.0f), 0.9f, 1.0f, 1.0f);
                    Particle q {};
                    q.kind = Kind::dot; q.x = d.x - ux * length * f; q.y = d.y - uy * length * f;
                    q.w = length / 12.0f + 1.0f; q.h = d.size * 0.7f * (1.0f - 0.7f * f);
                    q.r = c.getFloatRed(); q.g = c.getFloatGreen(); q.b = c.getFloatBlue();
                    splat (q, 0.55f * d.level * (1.0f - f) * envelope, s);
                }
                const float stretch = jlimit (1.0f, 3.0f, 1.0f + std::abs (d.vx) / 400.0f);
                p.kind = Kind::blob; p.w = d.size * stretch; p.h = d.size;
                splat (p, d.level * envelope, s);
                p.w *= 0.45f; p.h *= 0.45f; p.r = p.g = p.b = 1.0f;
                splat (p, 0.8f * d.level * envelope, s);
                break;
            }
            case Drop::Type::trail:
                // the tail: streaks along the flight that thin, fade and turn as they age
                p.kind = Kind::dash; p.w = d.size * (2.5f - 1.5f * u); p.h = d.size * 0.55f * (1.0f - 0.5f * u);
                splat (p, d.level * (1.0f - u) * envelope, s);
                break;
            case Drop::Type::drip:
                p.kind = Kind::dot; p.w = 0.7f * d.size; p.h = d.size * (1.0f + jmin (2.0f, std::abs (d.vy) * 0.02f));
                splat (p, d.level * (1.0f - u) * envelope, s);
                break;
            case Drop::Type::spark:
                p.kind = Kind::dash; p.w = d.size + jmin (3.0f, std::abs (d.vx) * 0.015f); p.h = 0.7f * d.size;
                splat (p, d.level * (1.0f - u * u) * envelope, s);
                break;
            case Drop::Type::flash:
                p.kind = Kind::blob; p.w = p.h = d.size * (0.6f + 0.4f * u);
                splat (p, d.level * (1.0f - u) * (1.0f - u) * envelope, s);
                break;
        }
    }
}

// ours: the stinger, the scorpion's tail end rising from the bottom left corner: three segments, the venom bulb (lit in
// the venom's colour of the moment, swelling when it fires) and the barb the venom leaves from
void Stripe::renderStinger (float s)
{
    if (envelope <= 0.005f) return;
    auto blob = [&] (float x, float y, float w, float h, Colour c, float level)
    {
        Particle p {};
        p.kind = Kind::blob; p.x = x; p.y = y; p.w = w; p.h = h;
        p.r = c.getFloatRed(); p.g = c.getFloatGreen(); p.b = c.getFloatBlue();
        splat (p, level * envelope, s);
    };
    const Colour shell = Colour::fromHSV (0.02f, 0.85f, 1.0f, 1.0f), rim = Colour::fromHSV (0.08f, 0.7f, 1.0f, 1.0f);
    // the tail: a chain of beads along an arc, thicker at each segment's joint, each segment with a glint
    for (int i = 0; i <= 18; ++i)
    {
        const float t = (float) i / 18.0f, a = t * MathConstants<float>::halfPi;
        const float x = 11.0f + 13.0f * std::sin (a), y = 307.0f - 20.0f * (1.0f - std::cos (a));
        const float joint = 0.5f + 0.5f * std::cos (MathConstants<float>::twoPi * t * 3.0f);
        const float w = (4.8f - 1.6f * t) * (0.75f + 0.25f * joint);
        blob (x, y, w, w * 0.85f, shell.interpolatedWith (rim, 0.4f * t), 0.42f + 0.18f * joint);
        if (i % 6 == 3) blob (x - 0.35f * w, y - 0.3f * w, 0.9f, 0.9f, Colour (255, 220, 190), 0.55f);
    }
    const auto venom = Colour::fromHSV (std::fmod (hueDrift + venomHues[0], 1.0f), 0.9f, 1.0f, 1.0f);
    blob (28.5f, 287.0f, 6.0f + 1.5f * charge, 4.8f + 1.2f * charge, venom, 0.55f + 0.7f * charge);
    blob (28.0f, 286.4f, 2.0f, 1.6f, Colours::white, 0.5f + 0.5f * charge);
    constexpr float barb[3][3] { { 32.6f, 288.4f, 2.2f }, { 35.2f, 289.6f, 1.5f }, { 37.4f, 290.4f, 0.9f } };
    for (auto& b : barb) blob (b[0], b[1], b[2], b[2] * 0.8f, Colour (255, 210, 235), 0.7f);
    blob (stingerTipX, stingerTipY, 1.2f + 2.8f * charge, 1.2f + 2.8f * charge, venom.interpolatedWith (Colours::white, 0.5f), 0.6f + 0.9f * charge);
}

void Stripe::paint (Graphics& g)
{
    const float s = theme::pixelScale (g, *this);
    const int w = jmax (1, roundToInt (width * s)), h = jmax (1, roundToInt (height * s));
    if (envelope <= 0.0f && background.isValid() && w == bufW && h == bufH && s == glowScale)
        g.drawImage (background, Rectangle<float> (0.0f, 0.0f, width, height));     // (dark: only what is under it)
    else
    {
        render (w, h, s);
        g.drawImage (image, Rectangle<float> (0.0f, 0.0f, width, height));
    }
}
}
