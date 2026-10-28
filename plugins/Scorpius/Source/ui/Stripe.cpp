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
constexpr float glandY = 291.0f;                               // ours: the venom's source (at heartX), far away
constexpr float focal = 240.0f;                                // (design px per unit across at distance 1)
constexpr size_t maxDrops = 4200;
// the venom's colour by depth (0 the lightest: acid lime, 0.5 the controls' mint, 1 deep teal)
Colour venomColour (float depth, float white = 0.0f)
{
    depth = jlimit (0.0f, 1.0f, depth);
    const auto c = Colour::fromHSV (0.24f + 0.26f * depth, 0.9f - 0.25f * std::abs (depth - 0.5f), 1.0f - 0.25f * depth * depth, 1.0f);
    return c.interpolatedWith (Colours::white, white);
}
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
            if (i == 10 || i == 38 || i == 52) fire (true);
            advanceVenom (1.0f / 60.0f);
        }
    }
    repaint();
}

void Stripe::setBands (float lows, float mid)
{
    // (0 at -40 dBFS, 1 at -10)
    auto level = [] (float a) { return jlimit (0.0f, 1.0f, (20.0f * std::log10 (jmax (a, 1.0e-6f)) + 40.0f) / 30.0f); };
    bass = level (lows); mids = level (mid);
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
        if (beat && envelope > 0.02f && elapsed - lastBurst > 0.2) { fire (true); lastBurst = elapsed; }
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
    // ours: a dim haze of venom, lime in the middle to teal at the ends; a beat flares the middle, crimson at the heart
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const float x = left + ((float) i + 0.5f) / s, u = jlimit (0.0f, 1.0f, (x - left) / width);
            float* px = glow.data() + (size_t) ((j * w + i) * 3);
            const float lum = 0.5f * (px[0] + px[1] + px[2]) / 3.0f;
            const auto c = venomColour (0.25f + 0.75f * std::abs (2.0f * u - 1.0f));
            px[0] = lum * c.getFloatRed(); px[1] = lum * c.getFloatGreen(); px[2] = lum * c.getFloatBlue();
            const float beatShare = 1.4f * lum * std::exp (-std::pow ((x - heartX) / 170.0f, 2.0f));
            float* hp = glowHeart.data() + (size_t) ((j * w + i) * 3);
            const float core = std::exp (-std::pow ((x - heartX) / 40.0f, 2.0f));
            hp[0] = beatShare * (0.4f + 0.6f * core); hp[1] = beatShare * (1.0f - 0.8f * core); hp[2] = beatShare * (0.5f - 0.4f * core);
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
            float* row = canvas + (size_t) (j * bufW + i0) * 3;
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
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
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
    canvas = light.data();
    const float beatGlow = style == Style::scorpius ? 0.7f * heart * envelope : 0.0f;
    for (size_t k = 0; k < glow.size(); ++k) light[k] = glow[k] * envelope + glowHeart[k] * beatGlow;
    const bool layers = style == Style::scorpius;
    if (layers)
    {
        front.assign (glow.size(), 0.0f);
        absorb.assign (glow.size(), 0.0f);
        gloss.assign (glow.size(), 0.0f);
        renderGland (s);
        renderVenom (s);
        canvas = light.data();
    }
    for (auto& p : particles)
    {
        if (style == Style::scorpius) break;
        const float age = (float) (elapsed - p.born);
        if (age < 0.0f || age > p.life) continue;
        float a = 1.0f;
        if (p.kind != Kind::flare)
        {
            a = jmin (1.0f, age / 0.015f) * jmin (1.0f, (p.life - age) / (0.3f * p.life));
            a *= 0.8f + 0.2f * std::sin (p.flicker + p.rate * age);
        }
        if (a * envelope <= 0.005f) continue;
        splat (p, p.level * a * envelope, s);
    }
    Image::BitmapData bd (image, Image::BitmapData::writeOnly);
    for (int j = 0; j < h; ++j)
    {
        const float* src = light.data() + (size_t) (j * w) * 3;
        const float* u = under.data() + (size_t) (j * w) * 3;
        const float* ab = layers ? absorb.data() + (size_t) (j * w) * 3 : nullptr;
        const float* fr = layers ? front.data() + (size_t) (j * w) * 3 : nullptr;
        const float* gl = layers ? gloss.data() + (size_t) (j * w) * 3 : nullptr;
        for (int i = 0; i < w; ++i, src += 3, u += 3)
        {
            float r = src[0], g = src[1], b = src[2];
            // light past white spills into the other channels (bright cores turn white)
            const float m = jmax (r, g, b);
            if (m > 1.0f) { const float spill = (m - 1.0f) * 0.6f; r += spill; g += spill; b += spill; }
            // added to what is under it
            r = jmin (1.0f, r + u[0]); g = jmin (1.0f, g + u[1]); b = jmin (1.0f, b + u[2]);
            // (ours) blood: darkens what is behind it, its own colour in front (where drops overlap they merge, so its
            // colour saturates at a deep red rather than adding up), and its gloss on top
            if (ab != nullptr)
            {
                static constexpr float deepest = 0.3f;
                auto merged = [] (float v) { return deepest * (1.0f - std::exp (-v / deepest)); };
                r = jmin (1.0f, r * (1.0f - jmin (0.92f, ab[0])) + merged (fr[0]) + gl[0]);
                g = jmin (1.0f, g * (1.0f - jmin (0.92f, ab[1])) + merged (fr[1]) + gl[1]);
                b = jmin (1.0f, b * (1.0f - jmin (0.92f, ab[2])) + merged (fr[2]) + gl[2]);
                ab += 3; fr += 3; gl += 3;
            }
            auto* px = reinterpret_cast<PixelRGB*> (bd.getPixelPointer (i, j));
            px->setARGB (255, (uint8) (r * 255.0f + 0.5f), (uint8) (g * 255.0f + 0.5f), (uint8) (b * 255.0f + 0.5f));
        }
    }
}

//==============================================================================
// ours: the venom, shot at you
void Stripe::addDrop (const Drop& d)
{
    if (drops.size() < maxDrops) drops.push_back (d);
}

// a drop leaving the gland: outwards at a random angle (flattened, so that most fly along the stripe), towards the
// viewer; angle >= 0 sets the direction (a ring); depth: its colour (0 lime ... 1 deep teal)
void Stripe::emit (Drop::Type type, float depth, float speed, float size, float level, float angle)
{
    Drop d {};
    d.type = type;
    const float a = angle >= 0.0f ? angle : MathConstants<float>::twoPi * rng.nextFloat();
    // (globs, and the spray that lands, keep within the stripe's height, so that they hit the glass somewhere along it)
    d.wet = type == Drop::Type::glob || (type == Drop::Type::spray && angle < 0.0f && rng.nextFloat() < 0.3f);
    const float lateral = (angle >= 0.0f ? 0.42f : d.wet ? uniform (rng, 0.04f, 0.36f) : uniform (rng, 0.06f, 0.6f)) * speed;
    d.X = 0.012f * gauss (rng); d.Y = 0.003f * gauss (rng); d.Z = angle >= 0.0f ? 1.0f : uniform (rng, 0.85f, 1.15f);
    d.vX = lateral * (d.wet ? (rng.nextBool() ? 1.0f : -1.0f) : std::cos (a)); d.vY = (d.wet ? 0.014f * gauss (rng) : 0.16f * lateral * std::sin (a));
    d.vZ = -(angle >= 0.0f ? 1.0f : uniform (rng, 0.5f, 1.3f)) * speed;
    d.size = size; d.level = level; d.seed = rng.nextFloat();
    const auto c = venomColour (depth, type == Drop::Type::mist ? 0.3f : 0.0f);
    d.r = c.getFloatRed(); d.g = c.getFloatGreen(); d.b = c.getFloatBlue();
    d.born = (float) elapsed; d.life = 6.0f;
    d.sx = d.px = heartX + focal * d.X / d.Z; d.sy = d.py = glandY + focal * d.Y / d.Z;
    addDrop (d);
}

// a strike (a beat): the heart squeezes and fires a burst, a ring and globs (bigger and deeper green with the bass);
// or (no strike) a single glob spat between beats
void Stripe::fire (bool strike)
{
    const float deep = 0.25f + 0.6f * bass;
    if (! strike)
    {
        emit (Drop::Type::glob, deep + 0.15f * gauss (rng), uniform (rng, 0.8f, 1.1f), uniform (rng, 0.10f, 0.14f) * (0.8f + 0.5f * bass), 1.1f);
        return;
    }
    for (int k = 0; k < 110; ++k)
        emit (Drop::Type::spray, uniform (rng, 0.0f, 1.0f), uniform (rng, 1.0f, 1.5f), uniform (rng, 0.04f, 0.08f), uniform (rng, 0.7f, 1.2f));
    const float turn = MathConstants<float>::twoPi * rng.nextFloat();
    for (int k = 0; k < 36; ++k)
        emit (Drop::Type::spray, 0.1f, 1.0f, 0.05f, 1.1f, std::fmod (turn + MathConstants<float>::twoPi * (float) k / 36.0f, MathConstants<float>::twoPi));
    for (int k = rng.nextInt (3) + 2; --k >= 0;)
        emit (Drop::Type::glob, deep + 0.12f * gauss (rng), uniform (rng, 0.9f, 1.25f), uniform (rng, 0.12f, 0.17f) * (0.8f + 0.6f * bass), 1.3f);
    squeeze = 1.0f; charge = 1.0f;
    lastHeartbeat = elapsed;
    Drop w {};
    w.type = Drop::Type::wave; w.sx = heartX; w.sy = glandY; w.level = 0.55f; w.born = (float) elapsed; w.life = 0.7f;
    addDrop (w);
    // the biggest hits (strong bass): now and then the scorpion stings the glass
    if ((bass > 0.6f || rng.nextFloat() < 0.05f) && elapsed - lastSting > 1.6) { sting(); lastSting = elapsed; }
}

// something left on the glass where a drop struck it
Stripe::Drop Stripe::onGlass (const Drop& from, Drop::Type type, float x, float y, float size, float life, float delay)
{
    Drop e = from;
    e.type = type; e.sx = e.px = x; e.sy = e.py = y; e.size = size;
    e.born = (float) elapsed + delay; e.life = life; e.vX = e.vY = 0.0f; e.seed = rng.nextFloat();
    e.slideAt = 0.0f; e.travel = 0.0f; e.tail = 0.0f; e.dirX = e.dirY = 0.0f;
    return e;
}

// a spray drop hits the glass, as water does: a splash ring, a few droplets flung out, and the drop stays as a bead (the
// bigger ones later run down)
void Stripe::land (const Drop& g)
{
    const float size = uniform (rng, 0.9f, 2.0f) * g.size / 0.05f;
    auto r = onGlass (g, Drop::Type::ring, g.sx, g.sy, 1.5f + 2.2f * size, 0.16f, 0.0f);
    r.level = 0.6f;
    addDrop (r);
    auto b = onGlass (g, Drop::Type::bead, g.sx, g.sy, size, uniform (rng, 1.5f, 3.5f), 0.0f);
    if (size > 2.2f && rng.nextFloat() < 0.6f) b.slideAt = (float) elapsed + uniform (rng, 0.2f, 1.2f);
    addDrop (b);
    for (int k = rng.nextInt (4) + 1; --k >= 0;)
    {
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 25.0f, 80.0f) * (0.5f + 0.25f * size);
        auto f = onGlass (g, Drop::Type::bead, g.sx, g.sy, uniform (rng, 0.45f, 0.9f), uniform (rng, 0.8f, 2.0f), 0.0f);
        f.vX = v * std::cos (a); f.vY = 0.65f * v * std::sin (a);
        addDrop (f);
    }
}

// a glob hits the glass: a big splash (a wide ring, a crown of droplets flung far), a pool that runs down in a few big
// beads, and, venom, it eats at the glass: bubbles and wisps rise, a faint mark stays
void Stripe::hitGlass (const Drop& g)
{
    const float size = uniform (rng, 5.0f, 8.0f) * g.size / 0.14f;
    auto r = onGlass (g, Drop::Type::ring, g.sx, g.sy, 3.0f * size, 0.22f, 0.0f);
    r.level = 0.9f;
    addDrop (r);
    auto pool = onGlass (g, Drop::Type::bead, g.sx, g.sy, size, uniform (rng, 1.8f, 3.0f), 0.0f);
    pool.slideAt = (float) elapsed + uniform (rng, 0.15f, 0.5f);
    addDrop (pool);
    for (int k = rng.nextInt (3) + 1; --k >= 0;)
    {
        auto runner = onGlass (g, Drop::Type::bead, g.sx + uniform (rng, -0.45f, 0.45f) * size, g.sy + uniform (rng, 0.1f, 0.4f) * size,
                               uniform (rng, 2.2f, 3.4f), uniform (rng, 2.0f, 3.5f), 0.0f);
        runner.slideAt = (float) elapsed + uniform (rng, 0.1f, 0.8f);
        addDrop (runner);
    }
    for (int k = 0; k < 10; ++k)
    {
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 50.0f, 170.0f) * (0.6f + 0.06f * size);
        auto f = onGlass (g, Drop::Type::bead, g.sx, g.sy, uniform (rng, 0.5f, 1.6f), uniform (rng, 1.0f, 2.6f), 0.0f);
        f.vX = v * std::cos (a); f.vY = 0.6f * v * std::sin (a);
        addDrop (f);
    }
    for (int k = 0; k < 6; ++k)
    {
        auto b = onGlass (g, Drop::Type::bubble, g.sx + uniform (rng, -0.4f, 0.4f) * size, g.sy + uniform (rng, -0.1f, 0.2f) * size,
                          uniform (rng, 0.5f, 1.1f), uniform (rng, 0.35f, 0.8f), uniform (rng, 0.1f, 0.8f));
        b.vY = -uniform (rng, 8.0f, 20.0f);
        addDrop (b);
    }
    for (int k = 0; k < 2; ++k)
    {
        auto w = onGlass (g, Drop::Type::wisp, g.sx + uniform (rng, -0.3f, 0.3f) * size, g.sy, 2.0f, uniform (rng, 1.0f, 1.6f), uniform (rng, 0.2f, 0.6f));
        w.vY = -uniform (rng, 4.0f, 9.0f); w.vX = 3.0f * gauss (rng);
        addDrop (w);
    }
    addDrop (onGlass (g, Drop::Type::etch, g.sx, g.sy, size, uniform (rng, 2.2f, 3.2f), 0.3f));
    auto f = onGlass (g, Drop::Type::flash, g.sx, g.sy, size * 1.3f, 0.1f, 0.0f);
    f.level = 0.9f;
    addDrop (f);
}

void Stripe::advanceVenom (float dt)
{
    charge *= std::exp (-dt / 0.25f);
    squeeze *= std::exp (-dt / 0.06f);
    // quiet: a slow drip; louder: a steady spray, globs spat now and then; the mids thicken the mist
    if (envelope > 0.02f)
    {
        sprayDue += dt * (8.0f + 520.0f * envelope * envelope);
        for (; sprayDue >= 1.0f; sprayDue -= 1.0f)
            emit (Drop::Type::spray, uniform (rng, 0.0f, 1.0f), uniform (rng, 0.6f, 1.2f), uniform (rng, 0.03f, 0.065f), uniform (rng, 0.5f, 1.0f));
        globDue += dt * (0.3f + 2.0f * envelope);
        for (; globDue >= 1.0f; globDue -= 1.0f) fire (false);
        mistDue += dt * (15.0f + 220.0f * mids * envelope);
        for (; mistDue >= 1.0f; mistDue -= 1.0f)
            emit (Drop::Type::mist, uniform (rng, 0.0f, 0.4f), uniform (rng, 0.9f, 1.6f), 0.02f, uniform (rng, 0.25f, 0.5f));
    }
    const float x0 = left - 30.0f, x1 = left + width + 30.0f, y0 = top - 20.0f, y1 = top + height + 20.0f;
    const size_t live = drops.size();
    for (size_t i = 0; i < live; ++i)
    {
        Drop d = drops[i];
        const float age = (float) elapsed - d.born;
        if (age > d.life || age < 0.0f) continue;
        d.px = d.sx; d.py = d.sy;
        switch (d.type)
        {
            case Drop::Type::spray: case Drop::Type::glob: case Drop::Type::mist:
            {
                d.X += d.vX * dt; d.Y += d.vY * dt; d.Z += d.vZ * dt;
                if (d.Z > 0.03f) { d.sx = heartX + focal * d.X / d.Z; d.sy = glandY + focal * d.Y / d.Z; }
                const bool onGlass = d.sx > left + 4.0f && d.sx < left + width - 4.0f && d.sy > top + 3.0f && d.sy < top + height - 4.0f;
                if (d.type == Drop::Type::glob && d.Z < 0.17f && onGlass) { hitGlass (d); d.life = 0.0f; }
                else if (d.wet && d.Z < 0.13f && onGlass) { land (d); d.life = 0.0f; }
                else if (d.Z < 0.03f || d.sx < x0 || d.sx > x1 || d.sy < y0 || d.sy > y1) d.life = 0.0f;   // (past you)
                break;
            }
            case Drop::Type::bead:
            {
                // flung droplets skid to a stop; a bead that runs goes down in fits and starts (it sticks and slips),
                // leaving tiny beads behind and shrinking, until the bottom of the glass
                // (blood is thicker: it spreads further before it stops, and runs slower)
                const float tau = d.blood ? 0.07f : 0.035f, k = std::exp (-dt / tau);
                d.sx += d.vX * (1.0f - k) * tau; d.sy += d.vY * (1.0f - k) * tau; d.vX *= k; d.vY *= k;
                if (d.slideAt > 0.0f && (float) elapsed > d.slideAt && d.sy < top + height - 2.5f)
                {
                    const float v = (d.blood ? 3.0f + 4.0f * d.size : 6.0f + 7.0f * d.size) * (rng.nextFloat() < 0.15f ? 0.0f : uniform (rng, 0.4f, 1.6f));
                    d.sy += v * dt; d.sx += 2.0f * gauss (rng) * dt; d.travel += v * dt;
                    for (; d.travel > 2.4f; d.travel -= 2.4f)
                    {
                        auto t = onGlass (d, Drop::Type::bead, d.sx + 0.3f * gauss (rng), d.sy - 0.4f * d.size, uniform (rng, 0.4f, 0.75f),
                                          uniform (rng, 0.6f, 1.6f), 0.0f);
                        t.level = 0.55f * d.level;
                        addDrop (t);
                        d.size *= 0.985f;
                    }
                    d.life = jmax (d.life, age + 0.4f);           // (it lasts while it runs)
                }
                break;
            }
            case Drop::Type::ring: case Drop::Type::wave: case Drop::Type::crack: case Drop::Type::overflow: break;
            case Drop::Type::sting: stingStage (d, age); break;
            case Drop::Type::bubble: d.sy += d.vY * dt; d.sx += 6.0f * std::sin (25.0f * age + 6.0f * d.seed) * dt; break;
            case Drop::Type::wisp: d.sy += d.vY * dt; d.sx += d.vX * dt; d.size += 5.0f * dt; break;
            case Drop::Type::etch: case Drop::Type::flash: break;
        }
        drops[i] = d;
    }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [this] (const Drop& d) { return (float) elapsed - d.born > d.life; }), drops.end());
}

// a soft line of light (a streak), brighter at its head (x1, y1)
void Stripe::splatLine (float x0, float y0, float x1, float y1, float width, float level, float r, float g, float b, float s)
{
    // (long streaks keep only their last 70 px)
    const float len = std::hypot (x1 - x0, y1 - y0);
    if (len > 70.0f) { const float k = 70.0f / len; x0 = x1 + (x0 - x1) * k; y0 = y1 + (y0 - y1) * k; }
    const float ax = (x0 - left) * s, ay = (y0 - top) * s, bx = (x1 - left) * s, by = (y1 - top) * s;
    const float hw = jmax (0.3f, width * 0.5f * s), edge = jmax (0.4f, 0.3f * s), reach = hw + 3.0f * edge;
    const int i0 = jmax (0, (int) std::floor (jmin (ax, bx) - reach)), i1 = jmin (bufW - 1, (int) std::ceil (jmax (ax, bx) + reach));
    const int j0 = jmax (0, (int) std::floor (jmin (ay, by) - reach)), j1 = jmin (bufH - 1, (int) std::ceil (jmax (ay, by) + reach));
    if (i0 > i1 || j0 > j1) return;
    const float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    for (int j = j0; j <= j1; ++j)
    {
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
        const float py = (float) j + 0.5f;
        for (int i = i0; i <= i1; ++i, row += 3)
        {
            const float px = (float) i + 0.5f;
            const float t = l2 > 1.0e-6f ? jlimit (0.0f, 1.0f, ((px - ax) * dx + (py - ay) * dy) / l2) : 1.0f;
            const float qx = ax + t * dx - px, qy = ay + t * dy - py;
            const float d = std::sqrt (qx * qx + qy * qy) - hw;
            const float v = level * (0.15f + 0.85f * t) * (d <= 0.0f ? 1.0f : std::exp (-d / edge));
            row[0] += v * r; row[1] += v * g; row[2] += v * b;
        }
    }
}

// a bead of water on the glass: mostly clear (a faint tint of the venom), light refracted along its lower right edge,
// and a bright highlight up and to the left; tiny ones are a speck with a glint
void Stripe::bead (float x, float y, float size, float level, const Drop& d, float s)
{
    Particle p {};
    p.kind = Kind::dot;
    const float r = 0.5f * d.r + 0.5f, g = 0.5f * d.g + 0.5f, b = 0.5f * d.b + 0.5f;
    if (d.tail > 0.3f)        // (spatter: a tail pointing back the way it came)
        splatLine (x - d.dirX * d.tail, y - d.dirY * d.tail, x, y, jmax (0.35f, 0.4f * size), 0.3f * level, r, g, b, s);
    if (size < 1.2f)
    {
        p.x = x; p.y = y; p.w = p.h = size; p.r = d.r; p.g = d.g; p.b = d.b;
        splat (p, 0.2f * level, s);
        p.x -= 0.25f * size; p.y -= 0.25f * size; p.w = p.h = 0.4f; p.r = p.g = p.b = 1.0f;
        splat (p, 0.3f * level, s);
        return;
    }
    // the body: barely there (water is clear), a touch of the venom's colour
    p.kind = Kind::blob; p.x = x; p.y = y; p.w = size; p.h = 0.9f * size; p.r = d.r; p.g = d.g; p.b = d.b;
    splat (p, 0.09f * level, s);
    // the light it bends: a thin bright crescent along its lower edge, brightest at the bottom
    p.kind = Kind::dot; p.w = p.h = jmax (0.4f, 0.14f * size); p.r = r; p.g = g; p.b = b;
    const int n = size > 4.0f ? 9 : 5;
    for (int k = 0; k < n; ++k)
    {
        const float a = 0.1f + 2.6f * (float) k / (float) (n - 1);     // (from the right round to the left)
        p.x = x + 0.44f * size * std::cos (a); p.y = y + 0.4f * size * std::sin (a);
        splat (p, (0.12f + 0.3f * std::sin (a)) * level, s);
    }
    // the highlight: a small, sharp white spot up and to the left, and a fainter one opposite
    p.x = x - 0.2f * size; p.y = y - 0.2f * size; p.w = p.h = jmax (0.45f, 0.17f * size); p.r = p.g = p.b = 1.0f;
    splat (p, 0.85f * level, s);
    p.x = x + 0.18f * size; p.y = y + 0.24f * size; p.w = p.h = jmax (0.4f, 0.1f * size);
    splat (p, 0.3f * level, s);
}

// a liquid drop: a round body stretched into a teardrop along its motion (dx, dy: where it came from), and a glossy
// highlight up and to the left of its middle
void Stripe::droplet (float x, float y, float dx, float dy, float size, float level, const Drop& d, float s)
{
    Particle p {};
    p.x = x; p.y = y; p.w = p.h = size;
    p.r = d.r; p.g = d.g; p.b = d.b;
    const float len = std::hypot (dx, dy), tail = jmin (3.5f * size, 0.9f * len);
    if (size < 1.5f)
    {
        // (far ones: a short streak is all that shows)
        if (tail > 0.3f) splatLine (x + dx / len * tail, y + dy / len * tail, x, y, size, level, d.r, d.g, d.b, s);
        else { p.kind = Kind::dot; splat (p, level, s); }
        return;
    }
    if (tail > 0.3f * size)
    {
        const float ux = dx / len, uy = dy / len;
        splatLine (x + ux * tail, y + uy * tail, x, y, 0.5f * size, 0.6f * level, d.r, d.g, d.b, s);
    }
    p.kind = Kind::blob;
    splat (p, level, s);
    p.x = x - 0.22f * size; p.y = y - 0.22f * size; p.w = p.h = jmax (0.5f, 0.3f * size);
    p.r = p.g = p.b = 1.0f;
    splat (p, 0.9f * level, s);
}

void Stripe::renderVenom (float s)
{
    const float e = envelope;
    for (const auto& d : drops)
    {
        const float age = (float) elapsed - d.born;
        if (age < 0.0f) continue;
        const float u = jlimit (0.0f, 1.0f, age / jmax (0.001f, d.life));
        Particle p {};
        p.x = d.sx; p.y = d.sy; p.r = d.r; p.g = d.g; p.b = d.b; p.kind = Kind::blob;
        switch (d.type)
        {
            case Drop::Type::spray:
            case Drop::Type::glob:
            {
                // nearer: bigger, brighter, more stretched
                const float near = jlimit (0.0f, 1.0f, 1.1f - d.Z);
                const float size = jmin (d.type == Drop::Type::glob ? 15.0f : 6.0f, d.size * 0.6f / jmax (0.04f, d.Z));
                const float level = d.level * (0.06f + 1.0f * near * near) * jmin (1.0f, age / 0.08f) * e;
                if (level > 0.004f) droplet (d.sx, d.sy, d.px - d.sx, d.py - d.sy, jmax (0.6f, size), level, d, s);
                break;
            }
            case Drop::Type::mist:
            {
                const float level = d.level * jlimit (0.0f, 1.0f, 1.1f - d.Z) * e;
                if (level > 0.004f) splatLine (d.px, d.py, d.sx, d.sy, 0.5f, level, d.r, d.g, d.b, s);
                break;
            }
            case Drop::Type::bead:
            {
                // (it dries: fades over the last third of its life)
                const float level = d.level * jmin (1.0f, 3.0f * (1.0f - u)) * jmin (1.0f, age / 0.02f) * e;
                if (level > 0.004f) { if (d.blood) renderBlood (d, s); else bead (d.sx, d.sy, d.size, level, d, s); }
                break;
            }
            case Drop::Type::sting:
                renderStinger (d, s);
                break;
            case Drop::Type::crack:
            {
                // hairline cracks from the puncture, catching the light
                Random r ((int64) (d.seed * 1.0e6f) + 3);
                canvas = gloss.data();
                for (int k = 0; k < 6; ++k)
                {
                    const float a = MathConstants<float>::twoPi * r.nextFloat(), len = uniform (r, 2.0f, 6.5f);
                    splatLine (d.sx, d.sy, d.sx + len * std::cos (a), d.sy + 0.8f * len * std::sin (a), 0.35f, 0.55f * (1.0f - u) * e, 1.0f, 1.0f, 1.0f, s);
                }
                canvas = light.data();
                break;
            }
            case Drop::Type::overflow:
            {
                // the venom welling up out of the wound: a green bead swelling and trembling until it bursts
                const float grow = jmin (1.0f, age / 0.3f), size = 1.0f + 4.2f * grow * (1.0f + 0.08f * std::sin (34.0f * age));
                p.kind = Kind::blob; p.w = size; p.h = 0.9f * size;
                splat (p, 0.35f * d.level * e, s);
                bead (d.sx, d.sy, size, d.level * e, d, s);
                break;
            }
            case Drop::Type::wave:
            {
                // the heart's pressure wave: two soft fronts running out from it, red turning to venom green
                const float r = 260.0f * (1.0f - std::exp (-age / 0.3f)), k = jmin (1.0f, r / 220.0f);
                const auto c = Colour (190, 10, 25).interpolatedWith (venomColour (0.3f), k);
                for (float side : { -1.0f, 1.0f })
                    splatGlow (heartX + side * r, glandY, 9.0f + 0.1f * r, 7.0f, d.level * (1.0f - u) * (1.0f - u) * e,
                               c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), s);
                break;
            }
            case Drop::Type::ring:
            {
                // the splash ring: a thin ring of light spreading fast and fading
                const float radius = d.size * (1.0f - std::exp (-age / 0.035f)), level = d.level * (1.0f - u) * (1.0f - u) * e;
                p.kind = Kind::dot; p.w = p.h = 0.7f;
                const int n = jlimit (6, 24, (int) (radius * 2.0f));
                for (int k = 0; k < n; ++k)
                {
                    const float a = MathConstants<float>::twoPi * ((float) k + d.seed) / (float) n;
                    p.x = d.sx + radius * std::cos (a); p.y = d.sy + 0.7f * radius * std::sin (a);
                    splat (p, level, s);
                }
                break;
            }
            case Drop::Type::bubble:
            {
                // a little ring that pops
                const float level = 0.8f * d.level * std::sin (MathConstants<float>::pi * u) * e;
                p.w = p.h = d.size * (1.0f + 0.6f * u);
                splat (p, 0.5f * level, s);
                p.w = p.h = 0.4f * d.size; p.r = p.g = p.b = 1.0f;
                p.x -= 0.2f * d.size; p.y -= 0.2f * d.size;
                splat (p, 0.6f * level, s);
                break;
            }
            case Drop::Type::wisp:
                p.w = d.size; p.h = d.size * 1.6f; p.r = 0.6f + 0.4f * d.r; p.g = 0.6f + 0.4f * d.g; p.b = 0.6f + 0.4f * d.b;
                splat (p, 0.14f * std::sin (MathConstants<float>::pi * u) * e, s);
                break;
            case Drop::Type::etch:
                p.w = d.size; p.h = 0.6f * d.size;
                splat (p, 0.13f * (1.0f - u) * e, s);
                break;
            case Drop::Type::flash:
                p.w = d.size * (0.5f + 0.8f * u); p.h = p.w * 0.45f;
                splat (p, d.level * (1.0f - u) * (1.0f - u) * e, s);
                break;
        }
    }
}

// a soft glow (gaussian), added to the canvas
void Stripe::splatGlow (float x, float y, float rx, float ry, float level, float r, float g, float b, float s)
{
    const float cx = (x - left) * s, cy = (y - top) * s, ax = jmax (0.5f, rx * s), ay = jmax (0.5f, ry * s);
    const int i0 = jmax (0, (int) (cx - 2.5f * ax)), i1 = jmin (bufW - 1, (int) (cx + 2.5f * ax));
    const int j0 = jmax (0, (int) (cy - 2.5f * ay)), j1 = jmin (bufH - 1, (int) (cy + 2.5f * ay));
    for (int j = j0; j <= j1; ++j)
    {
        const float dy = ((float) j + 0.5f - cy) / ay, gy = std::exp (-dy * dy);
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
        for (int i = i0; i <= i1; ++i, row += 3)
        {
            const float dx = ((float) i + 0.5f - cx) / ax, v = level * gy * std::exp (-dx * dx);
            row[0] += v * r; row[1] += v * g; row[2] += v * b;
        }
    }
}

// ours: the heart, far away in the middle: soft layers of glow from a warm core through crimson and maroon into the
// green haze, beating lub-dub (the beat resyncs to each strike); on a strike it squeezes, then swells
void Stripe::renderGland (float s)
{
    if (envelope <= 0.005f) return;
    const float t = std::fmod ((float) (elapsed - lastHeartbeat), 0.85f);
    auto bump = [] (float x, float w) { return std::exp (-(x / w) * (x / w)); };
    const float beatNow = bump (t, 0.045f) + 0.6f * bump (t - 0.17f, 0.045f);
    const float swell = (1.0f + 0.22f * beatNow) * (1.0f - 0.35f * squeeze + 0.5f * jmax (0.0f, charge - squeeze));
    const float e = envelope;
    splatGlow (heartX, glandY, 60.0f * swell, 16.0f, (0.35f + 0.15f * beatNow + 0.2f * charge) * e, 0.30f, 0.0f, 0.03f, s);
    splatGlow (heartX, glandY, 22.0f * swell, 8.0f * swell, (0.5f + 0.3f * beatNow + 0.4f * charge) * e, 0.85f, 0.03f, 0.07f, s);
    splatGlow (heartX, glandY, 8.0f * swell, 4.5f * swell, (0.6f + 0.4f * beatNow + 0.5f * charge) * e, 1.0f, 0.18f, 0.12f, s);
    splatGlow (heartX, glandY, 3.0f * swell, 2.2f * swell, (0.3f + 0.25f * beatNow + 0.3f * charge) * e, 1.0f, 0.45f, 0.3f, s);
}

// ours: blood, in flight and on the glass. It is drawn twice: into the absorbing layer (how much of what is behind it
// it takes away: green and blue most, so the venom behind shows through tinted red, more where it is thick) and in
// front (the red light it lets through, its rim of refracted light and its wet highlight). Drying (the last half of
// its life) it loses the shine and darkens, then fades.
void Stripe::renderBlood (const Drop& d, float s)
{
    const float age = (float) elapsed - d.born;
    Particle p {};
    auto absorbing = [&] (float amount) { canvas = absorb.data(); p.r = 0.14f * amount; p.g = 0.78f * amount; p.b = 0.72f * amount; };
    auto lit = [&] (float r, float g, float b) { canvas = front.data(); p.r = r; p.g = g; p.b = b; };
    // on the glass: a bead of blood
    const float u = jlimit (0.0f, 1.0f, age / jmax (0.001f, d.life));
    const float dry = jlimit (0.0f, 1.0f, 2.0f * u - 1.0f), fade = jmin (1.0f, 4.0f * (1.0f - u)) * jmin (1.0f, age / 0.02f) * envelope * d.level;
    const float size = d.size, thick = 0.45f + 0.55f * jmin (1.0f, size / 3.5f);
    // while it flies off the hit, it streaks (the spatter's elongated drops)
    const float moved = std::hypot (d.sx - d.px, d.sy - d.py);
    if (moved > 0.6f)
    {
        canvas = absorb.data(); splatLine (d.px, d.py, d.sx, d.sy, 0.7f * size, 0.7f * thick * fade, 0.14f, 0.78f, 0.72f, s);
        canvas = front.data();  splatLine (d.px, d.py, d.sx, d.sy, 0.7f * size, 0.6f * fade, 0.18f, 0.0f, 0.012f, s);
    }
    else if (d.tail > 0.3f)
    {
        // spatter at rest: its tail points back at the wound (the drop's head is the round end)
        const float x0 = d.sx - d.dirX * d.tail, y0 = d.sy - d.dirY * d.tail, w = jmax (0.35f, 0.45f * size);
        canvas = absorb.data(); splatLine (x0, y0, d.sx, d.sy, w, 0.75f * thick * fade, 0.14f, 0.78f, 0.72f, s);
        canvas = front.data();  splatLine (x0, y0, d.sx, d.sy, w, 0.7f * thick * fade * (1.0f - 0.6f * dry), 0.24f, 0.0f, 0.014f, s);
    }
    p.kind = size < 1.2f ? Kind::dot : Kind::blob; p.x = d.sx; p.y = d.sy; p.w = size; p.h = 0.92f * size;
    absorbing ((0.75f + 0.35f * dry) * thick); splat (p, fade, s);
    lit (0.24f * (1.0f - 0.6f * dry), 0.0f, 0.014f); splat (p, thick * fade, s);
    if (size >= 1.8f)
    {
        // the light it bends: a red crescent along its lower edge
        p.kind = Kind::dot; p.w = p.h = jmax (0.4f, 0.15f * size);
        lit (0.6f, 0.05f, 0.06f);
        for (int k = 0; k < 6; ++k)
        {
            const float a = 0.1f + 2.6f * (float) k / 5.0f;
            p.x = d.sx + 0.42f * size * std::cos (a); p.y = d.sy + 0.38f * size * std::sin (a);
            splat (p, (0.1f + 0.25f * std::sin (a)) * (1.0f - dry) * fade, s);
        }
    }
    // the wet highlight
    p.kind = Kind::dot; p.x = d.sx - 0.2f * size; p.y = d.sy - 0.2f * size; p.w = p.h = jmax (0.4f, 0.17f * size);
    lit (1.0f, 0.8f, 0.8f); canvas = gloss.data(); splat (p, (size < 1.2f ? 0.08f : size < 1.8f ? 0.3f : 0.6f) * (1.0f - 0.85f * dry) * fade, s);
    canvas = light.data();
}

// the sting (the biggest hits): the barb stabs the glass at a place along the stripe, away from the heart
void Stripe::sting()
{
    Drop d {};
    d.type = Drop::Type::sting;
    const float side = rng.nextBool() ? 1.0f : -1.0f;
    d.sx = jlimit (left + 30.0f, left + width - 30.0f, heartX + side * uniform (rng, 110.0f, 380.0f));
    d.sy = glandY + uniform (rng, -6.0f, 6.0f);
    d.born = (float) elapsed; d.life = 1.25f; d.seed = rng.nextFloat(); d.level = 1.0f;
    const auto c = venomColour (uniform (rng, 0.1f, 0.4f));
    d.r = c.getFloatRed(); d.g = c.getFloatGreen(); d.b = c.getFloatBlue();
    addDrop (d);
}

// what happens as the sting goes on: the barb reaches the glass (0.09 s: a glint, a puncture, cracks), tears free
// (0.17 s: the first spurt, the heart's second beat), the venom wells up (0.4 s) and bursts (0.8 s), and the wound
// spurts again on the heart's next lub and dub (0.85, 1.02 s)
void Stripe::stingStage (Drop& d, float age)
{
    static constexpr float at[] { 0.09f, 0.17f, 0.40f, 0.80f, 0.85f, 1.02f };
    const int stage = (int) d.travel;
    if (stage >= 6 || age < at[stage]) return;
    d.travel += 1.0f;
    switch (stage)
    {
        case 0:
        {
            auto glint = onGlass (d, Drop::Type::flash, d.sx, d.sy, 5.0f, 0.08f, 0.0f);
            glint.r = glint.g = glint.b = 1.0f; glint.level = 1.2f;
            addDrop (glint);
            addDrop (onGlass (d, Drop::Type::crack, d.sx, d.sy, 1.0f, 0.6f, 0.0f));
            auto hole = onGlass (d, Drop::Type::bead, d.sx, d.sy, 1.6f, 4.5f, 0.0f);
            hole.blood = true; hole.level = 1.3f;
            addDrop (hole);
            break;
        }
        case 1: spurt (d, 40, 1.0f); break;
        case 2:
        {
            auto o = onGlass (d, Drop::Type::overflow, d.sx, d.sy, 1.0f, at[3] - at[2], 0.0f);
            o.level = 1.0f;
            addDrop (o);
            break;
        }
        case 3: venomBurst (d); break;
        case 4: spurt (d, 18, 0.55f); break;
        case 5: spurt (d, 10, 0.35f); break;
        default: break;
    }
}

// a spurt of blood from the wound: mostly a fan the way the barb was pulled (towards the heart), some all round, and a
// mist; each drop flies, slows, and stays as spatter with a tail back towards the wound; the big ones run
void Stripe::spurt (const Drop& w, int count, float strength)
{
    const float pull = std::atan2 (glandY - w.sy, heartX - w.sx);
    auto drop = [&] (float size, float angle, float speed, float life, float tailPerSpeed)
    {
        auto b = onGlass (w, Drop::Type::bead, w.sx, w.sy, size, life, 0.0f);
        b.blood = true; b.level = 1.0f;
        const float c = std::cos (angle), sn = 0.7f * std::sin (angle), n = std::hypot (c, sn);
        b.vX = speed * c; b.vY = speed * sn;
        b.dirX = c / n; b.dirY = sn / n; b.tail = jmin (8.0f, speed * tailPerSpeed);
        if (size > 1.8f) b.slideAt = (float) elapsed + uniform (rng, 0.3f, 1.0f);
        addDrop (b);
    };
    for (int k = 0; k < count; ++k)
    {
        const bool fan = rng.nextFloat() < 0.85f;
        const float angle = fan ? pull + 0.3f * gauss (rng) : MathConstants<float>::twoPi * rng.nextFloat();
        const float size = rng.nextFloat() < 0.12f ? uniform (rng, 1.8f, 2.8f) : uniform (rng, 0.4f, 1.4f);
        drop (size, angle, uniform (rng, 120.0f, 420.0f) * strength * (fan ? 1.0f : 0.5f), uniform (rng, 3.0f, 5.0f), 0.018f);
    }
    for (int k = 0; k < count + count / 4; ++k)
        drop (uniform (rng, 0.25f, 0.5f), pull + 0.5f * gauss (rng), uniform (rng, 80.0f, 300.0f) * strength, uniform (rng, 2.0f, 3.0f), 0.01f);
}

// the venom overflowing the wound bursts: a green splatter all round (drops with tails), a splash ring, a couple of
// runners, and it eats at the glass (bubbles, wisps, a mark)
void Stripe::venomBurst (const Drop& w)
{
    for (int k = 0; k < 22; ++k)
    {
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 50.0f, 230.0f);
        const float size = rng.nextFloat() < 0.15f ? uniform (rng, 1.8f, 2.6f) : uniform (rng, 0.5f, 1.6f);
        auto b = onGlass (w, Drop::Type::bead, w.sx, w.sy, size, uniform (rng, 1.5f, 3.0f), 0.0f);
        const auto c = venomColour (uniform (rng, 0.0f, 0.6f));
        b.r = c.getFloatRed(); b.g = c.getFloatGreen(); b.b = c.getFloatBlue(); b.blood = false; b.level = 1.2f;
        const float cx = std::cos (a), sy = 0.7f * std::sin (a), n = std::hypot (cx, sy);
        b.vX = v * cx; b.vY = v * sy; b.dirX = cx / n; b.dirY = sy / n; b.tail = jmin (6.0f, v * 0.015f);
        if (size > 1.8f) b.slideAt = (float) elapsed + uniform (rng, 0.2f, 0.8f);
        addDrop (b);
    }
    auto ring = onGlass (w, Drop::Type::ring, w.sx, w.sy, 12.0f, 0.2f, 0.0f);
    ring.level = 0.8f; ring.blood = false;
    addDrop (ring);
    auto f = onGlass (w, Drop::Type::flash, w.sx, w.sy, 10.0f, 0.12f, 0.0f);
    f.level = 0.9f; f.blood = false;
    addDrop (f);
    for (int k = 0; k < 5; ++k)
    {
        auto b = onGlass (w, Drop::Type::bubble, w.sx + uniform (rng, -3.0f, 3.0f), w.sy + uniform (rng, -1.0f, 2.0f), uniform (rng, 0.5f, 1.1f),
                          uniform (rng, 0.35f, 0.8f), uniform (rng, 0.05f, 0.6f));
        b.vY = -uniform (rng, 8.0f, 20.0f);
        addDrop (b);
    }
    for (int k = 0; k < 2; ++k)
    {
        auto wi = onGlass (w, Drop::Type::wisp, w.sx + uniform (rng, -2.0f, 2.0f), w.sy, 2.0f, uniform (rng, 1.0f, 1.6f), uniform (rng, 0.1f, 0.5f));
        wi.vY = -uniform (rng, 4.0f, 9.0f); wi.vX = 3.0f * gauss (rng);
        addDrop (wi);
    }
    addDrop (onGlass (w, Drop::Type::etch, w.sx, w.sy, 7.0f, uniform (rng, 2.2f, 3.2f), 0.2f));
}

// the barb: a dark, curved spike of chitin (it covers what is behind it, with a glint along its edge), thrusting out of
// the depth from the heart to the glass, held there a moment, then torn back out, its tip bloody
void Stripe::renderStinger (const Drop& d, float s)
{
    const float a = (float) elapsed - d.born;
    float tipX, tipY, scale; bool bloody = false;
    if (a < 0.09f) { const float f = (a / 0.09f) * (a / 0.09f); tipX = heartX + (d.sx - heartX) * f; tipY = glandY + (d.sy - glandY) * f; scale = 0.2f + 0.8f * f; }
    else if (a < 0.17f) { tipX = d.sx + 0.5f * std::sin (150.0f * a); tipY = d.sy; scale = 1.0f; }
    else if (a < 0.32f) { const float f = ((a - 0.17f) / 0.15f) * ((a - 0.17f) / 0.15f); tipX = d.sx + (heartX - d.sx) * f; tipY = d.sy + (glandY - d.sy) * f; scale = 1.0f - 0.85f * f; bloody = true; }
    else return;
    float bx = heartX - tipX, by = glandY - tipY;
    const float len = std::hypot (bx, by);
    if (len < 1.0f) { bx = d.sx < heartX ? 1.0f : -1.0f; by = 0.0f; } else { bx /= len; by /= len; }
    float nx = -by, ny = bx;
    if (ny > 0.0f) { nx = -nx; ny = -ny; }                         // (it curves upwards, as a scorpion's tail)
    const float e = envelope;
    Particle p {};
    p.kind = Kind::blob;
    for (int k = 26; k >= 0; --k)
    {
        const float along = (float) k * 0.75f * scale, bend = jmin (6.0f, 0.03f * along * along) * scale;
        const float w = jmin (4.6f, 0.45f + 0.17f * (float) k) * scale;
        p.x = tipX + bx * along + nx * bend; p.y = tipY + by * along + ny * bend; p.w = p.h = w;
        canvas = absorb.data(); p.r = p.g = p.b = 0.9f; splat (p, e, s);
        canvas = front.data();
        if (bloody && k < 5) { p.r = 0.3f; p.g = 0.0f; p.b = 0.02f; } else { p.r = 0.1f; p.g = 0.045f; p.b = 0.02f; }
        splat (p, e, s);
        // the glint along its upper edge
        Particle q = p;
        q.kind = Kind::dot; q.x += nx * 0.35f * w; q.y += ny * 0.35f * w; q.w = q.h = jmax (0.4f, 0.22f * w);
        q.r = 1.0f; q.g = 0.8f; q.b = 0.55f;
        canvas = gloss.data(); splat (q, 0.3f * e, s);
        // and the venom's green light on its lower edge (lit from behind by the haze)
        q.x = p.x - nx * 0.42f * w; q.y = p.y - ny * 0.42f * w; q.r = 0.3f; q.g = 0.9f; q.b = 0.45f;
        splat (q, 0.18f * e, s);
    }
    Particle tip {};
    tip.kind = Kind::dot; tip.x = tipX; tip.y = tipY; tip.w = tip.h = 0.6f; tip.r = tip.g = tip.b = 1.0f;
    canvas = gloss.data(); splat (tip, 0.8f * e, s);
    canvas = light.data();
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
