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
    { 1.00f, 0.96f, 0.94f, 0.07f },     // white
    { 1.00f, 0.88f, 0.72f, 0.04f },     // warm white
    { 1.00f, 0.28f, 0.06f, 0.065f },    // red-orange (hue 15)
    { 1.00f, 0.60f, 0.08f, 0.18f },     // amber (45)
    { 1.00f, 0.92f, 0.15f, 0.03f },     // yellow (75)
    { 0.05f, 0.78f, 1.00f, 0.10f },     // cyan (195)
    { 0.12f, 0.45f, 1.00f, 0.08f },     // azure (225)
    { 0.38f, 0.28f, 1.00f, 0.125f },    // blue-violet (255)
    { 0.72f, 0.25f, 1.00f, 0.015f },    // violet (285)
    { 1.00f, 0.18f, 0.40f, 0.056f } };  // pink-red (345)
// (the shares of the saturated hues fitted to films of music, UI round 8: the reference's 30-degree hue bins read
// 0.18 0.19 0.02 / 0.15 0.13 0.16 0.03 0.03 0.09 from red-orange to pink)
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

constexpr int numParticles = 1400, numFlares = 9;
constexpr float lanes[] { 289.1f, 291.6f, 294.25f, 302.5f };   // the rows the band's dashes keep to (the last one less)
// where the reference's particles are (local maxima per frame, measured): along the stripe in bins of 18.5 design px
// from x 0 (thinned towards the ends, where ours stood out more: the reference has ~60 per frame in each end sixth),
// and across it in bins of 2 design px from y 272.5
constexpr float alongBins[48] { 0, 0.5f, 1.6f, 4.1f, 7.7f, 10.5f, 13.7f, 18, 22.9f, 29.1f, 36, 37, 39, 42, 43, 40, 42, 42, 47, 48, 50, 45, 49, 47,
                                55, 55, 45, 52, 46, 43, 47, 44, 42, 47, 43, 38, 38, 35, 29.1f, 22.9f, 17.2f, 12.9f, 10.5f, 7, 3.5f, 1, 0, 0 };
constexpr float acrossBins[17] { 31, 16, 21, 23, 28, 31, 47, 103, 240, 241, 209, 161, 124, 100, 88, 45, 15 };
// the same for the dots and blobs alone (the band's dashes are added on top of them)
constexpr float dotBins[17] { 3, 4, 6, 8, 11, 15, 28, 55, 90, 90, 85, 140, 150, 120, 80, 25, 5 };   // (more below: UI round 8)

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
        // a flare: a narrow bar that swells to its height and shrinks again, sliding a little sideways
        p.x = jlimit (40.0f, 840.0f, 455.0f + 190.0f * gauss (rng));
        const auto& t = style == Style::scorpius ? pick (rng, starTints) : pick (rng, flareTints);
        p.r = t.r; p.g = t.g; p.b = t.b;
        p.y = 287.0f + 4.0f * gauss (rng);              // (mostly a little above the band)
        p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 0.0f, 35.0f);    // (they barely slide: UI round 8)
        p.vy = 0.0f;
        p.w = uniform (rng, 2.4f, 4.0f);                  // (glowing capsules, as the reference's: ~3 tall ones at
        p.h = p.w + uniform (rng, 4.0f, 20.0f);           // any time, half of them over 11 px high)
        p.level = uniform (rng, 1.05f, 1.5f);
        p.life = uniform (rng, 0.10f, 0.22f);
        p.born = (float) (now + (anyAge ? uniform (rng, -0.2f, 0.3f) : uniform (rng, 0.0f, 0.15f)));
        p.flicker = 0.0f; p.rate = 0.0f; p.expand = 0.0f;
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
        p.kind = v < 0.66f ? Kind::dot : v < 0.86f ? Kind::faint : Kind::blob;
        if (style == Style::scorpius && p.kind == Kind::blob && rng.nextFloat() < 0.6f) p.kind = Kind::star;   // (ours)
        p.y = fromBins (rng, dotBins, 272.5f, 2.0f);
        // (UI round 8) above the band the reference has fewer, bigger glowing orbs than dots: most there are orbs
        if (p.y < 286.0f) p.kind = rng.nextFloat() < 0.3f ? Kind::blob : Kind::dot;
    }
    // moving (measured by tracking the reference's particles from frame to frame, UI round 8): above the band they
    // stream outwards from the middle as if coming at you (sideways at ~0.45 times their distance from it per second,
    // rising 10 ... 40 px/s); in the band most twinkle in place and a fifth stream outwards; below it they barely move
    const float off = p.y - 291.0f;
    p.expand = 0.0f; p.vx = 8.0f * gauss (rng); p.vy = 0.0f;
    if (off < -4.5f) { p.expand = uniform (rng, 0.32f, 0.6f); p.vy = -uniform (rng, 10.0f, 40.0f); }
    else if (off < 4.5f) { if (rng.nextFloat() < 0.2f) p.expand = uniform (rng, 0.3f, 0.6f); }
    else p.vy = uniform (rng, 0.0f, 8.0f);
    switch (p.kind)
    {
        case Kind::dot:   p.w = uniform (rng, 0.5f, 1.0f); p.h = uniform (rng, 0.5f, 0.8f); p.level = uniform (rng, 0.55f, 0.9f); break;
        case Kind::dash:  p.w = uniform (rng, 1.8f, 5.0f); p.h = uniform (rng, 0.7f, 1.0f); p.level = uniform (rng, 0.5f, 0.95f); break;
        case Kind::faint: p.w = 0.6f; p.h = 0.6f; p.level = uniform (rng, 0.3f, 0.6f); break;
        case Kind::blob:  p.w = p.y < 286.0f ? uniform (rng, 1.2f, 2.6f) : uniform (rng, 1.6f, 3.5f); p.h = p.w * uniform (rng, 0.8f, 1.3f);
                          p.level = p.y < 286.0f ? uniform (rng, 0.4f, 0.8f) : uniform (rng, 0.6f, 1.1f); break;
        case Kind::star:  p.w = uniform (rng, 3.0f, 5.5f); p.h = p.w * uniform (rng, 0.7f, 1.0f); p.level = uniform (rng, 0.7f, 1.2f); break;
        case Kind::flare: break;
    }
    p.life = p.expand > 0.0f && off < -4.5f ? uniform (rng, 0.3f, 0.9f) : uniform (rng, 0.05f, 0.3f);   // (the streaming ones last)
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
        if (p.expand > 0.0f) p.vx = p.expand * (p.x - 444.0f);
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
    if (style != Style::scorpius)
    {
        // (the reference's, measured with music: UI round 8) a faint violet haze above the band, brightest in the middle
        for (int j = 0; j < h; ++j)
        {
            const float y = top + ((float) j + 0.5f) / s;
            const float rows = jlimit (0.0f, 1.0f, (287.0f - y) / 3.0f) * jlimit (0.0f, 1.0f, (y - 272.5f) / 1.5f);
            if (rows <= 0.0f) continue;
            for (int i = 0; i < w; ++i)
            {
                const float x = left + ((float) i + 0.5f) / s, along = 0.45f + 0.55f * std::exp (-std::pow ((x - 460.0f) / 300.0f, 2.0f));
                float* px = glow.data() + (size_t) ((j * w + i) * 3);
                px[0] += 0.036f * rows * along; px[1] += 0.028f * rows * along; px[2] += 0.052f * rows * along;
            }
        }
        return;
    }
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
    const bool glass = style == Style::scorpius;
    if (glass)
    {
        renderGland (s);
        renderVenom (s);
        canvas = light.data();
    }
    else
        for (auto& p : particles)
        {
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
    // (the reference's) the band reflected in the glass's lower rim: a line of light under it, following the light of
    // the lower band above it column by column, strongest towards the ends (measured: at the ends three times ours was)
    if (! glass)
    {
        const int r0 = jlimit (0, h - 1, (int) ((296.0f - top) * s)), r1 = jlimit (0, h - 1, (int) ((303.0f - top) * s));
        const int m0 = jlimit (0, h - 1, (int) ((305.7f - top) * s)), m1 = jlimit (0, h - 1, (int) ((306.6f - top) * s));
        for (int i = 0; i < w; ++i)
        {
            float acc[3] {};
            for (int j = r0; j <= r1; ++j) for (int c = 0; c < 3; ++c) acc[c] += light[(size_t) ((j * w + i) * 3 + c)];
            const float x = left + ((float) i + 0.5f) / s, gain = (0.5f + 1.5f * jmin (1.0f, std::abs (x - 444.0f) / 400.0f)) / (float) (r1 - r0 + 1);
            for (int j = m0; j <= m1; ++j)
            {
                const float shape = std::sin (MathConstants<float>::pi * ((float) (j - m0) + 0.5f) / (float) (m1 - m0 + 1));
                for (int c = 0; c < 3; ++c) light[(size_t) ((j * w + i) * 3 + c)] += gain * shape * acc[c];
            }
        }
    }
    // the scene: the light added to what is under it (light past white spills into the other channels: bright cores
    // turn white)
    scene.resize (light.size());
    for (size_t k = 0; k < light.size(); k += 3)
    {
        float r = light[k], g = light[k + 1], b = light[k + 2];
        const float m = jmax (r, g, b);
        if (m > 1.0f) { const float spill = (m - 1.0f) * 0.6f; r += spill; g += spill; b += spill; }
        scene[k] = jmin (1.0f, r + under[k]); scene[k + 1] = jmin (1.0f, g + under[k + 1]); scene[k + 2] = jmin (1.0f, b + under[k + 2]);
    }
    // ours: the glass in front of it, and what is on it
    if (glass)
    {
        if (w != glassW || h != glassH) prepareGlass (w, h, s);
        cover.assign ((size_t) (w * h), 0.0f);
        front.assign (light.size(), 0.0f);
        gloss.assign (light.size(), 0.0f);
        renderGlass (s);
    }
    Image::BitmapData bd (image, Image::BitmapData::writeOnly);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const size_t k = (size_t) (j * w + i), k3 = k * 3;
            float r = scene[k3], g = scene[k3 + 1], b = scene[k3 + 2];
            if (glass)
            {
                // the drops show what they bend in place of what is behind them; the glass's edges are darker; its
                // dust and scratches catch the light behind them; glints and reflections on top
                const float c = jmin (1.0f, cover[k]), lum = 0.3f * r + 0.55f * g + 0.15f * b;
                const float shine = dust[k] * (0.04f + 0.9f * lum) * envelope;
                r = (r * (1.0f - c) + front[k3]) * vignette[k] + gloss[k3] + shine;
                g = (g * (1.0f - c) + front[k3 + 1]) * vignette[k] + gloss[k3 + 1] + shine;
                b = (b * (1.0f - c) + front[k3 + 2]) * vignette[k] + gloss[k3 + 2] + shine;
                r = jmin (1.0f, r); g = jmin (1.0f, g); b = jmin (1.0f, b);
            }
            auto* px = reinterpret_cast<PixelRGB*> (bd.getPixelPointer (i, j));
            px->setARGB (255, (uint8) (r * 255.0f + 0.5f), (uint8) (g * 255.0f + 0.5f), (uint8) (b * 255.0f + 0.5f));
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
    d.wet = type == Drop::Type::glob || (type == Drop::Type::spray && angle < 0.0f && rng.nextFloat() < 0.4f);
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
}

// something left on the glass where a drop struck it
Stripe::Drop Stripe::onGlass (const Drop& from, Drop::Type type, float x, float y, float size, float life, float delay)
{
    Drop e = from;
    e.type = type; e.sx = e.px = x; e.sy = e.py = y; e.size = size;
    e.born = (float) elapsed + delay; e.life = life; e.vX = e.vY = 0.0f; e.seed = rng.nextFloat();
    e.slideAt = 0.0f; e.travel = 0.0f; e.spread = 0.0f;
    return e;
}

// a spray drop hits the glass, as water does: it flattens into a film and pulls back into a bead, a faint splash ring
// spreads, now and then a droplet is flung off
void Stripe::land (const Drop& g)
{
    const float size = uniform (rng, 0.6f, 1.6f) * g.size / 0.05f;
    auto r = onGlass (g, Drop::Type::ring, g.sx, g.sy, 1.5f + 2.0f * size, 0.15f, 0.0f);
    r.level = 0.35f;
    addDrop (r);
    auto b = onGlass (g, Drop::Type::bead, g.sx, g.sy, size, uniform (rng, 4.0f, 9.0f), 0.0f);
    b.spread = 1.0f;
    addDrop (b);
    for (int k = rng.nextInt (3); --k >= 0;)
    {
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 25.0f, 80.0f) * (0.5f + 0.25f * size);
        auto f = onGlass (g, Drop::Type::bead, g.sx, g.sy, uniform (rng, 0.4f, 0.8f), uniform (rng, 1.5f, 3.5f), 0.0f);
        f.vX = v * std::cos (a); f.vY = 0.65f * v * std::sin (a);
        addDrop (f);
    }
}

// a glob hits the glass: a big flattened splash (a wide ring, droplets flung far) that pulls back into a heavy bead,
// which soon runs
void Stripe::hitGlass (const Drop& g)
{
    const float size = uniform (rng, 3.6f, 5.2f) * g.size / 0.14f;
    auto r = onGlass (g, Drop::Type::ring, g.sx, g.sy, 2.6f * size, 0.22f, 0.0f);
    r.level = 0.6f;
    addDrop (r);
    auto pool = onGlass (g, Drop::Type::bead, g.sx, g.sy, size, uniform (rng, 4.0f, 7.0f), 0.0f);
    pool.spread = 1.0f;
    addDrop (pool);
    for (int k = 0; k < 10; ++k)
    {
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = uniform (rng, 50.0f, 170.0f) * (0.6f + 0.06f * size);
        auto f = onGlass (g, Drop::Type::bead, g.sx, g.sy, uniform (rng, 0.5f, 1.5f), uniform (rng, 2.5f, 5.0f), 0.0f);
        f.vX = v * std::cos (a); f.vY = 0.6f * v * std::sin (a); f.spread = 0.5f;
        addDrop (f);
    }
    auto f = onGlass (g, Drop::Type::flash, g.sx, g.sy, size * 1.3f, 0.09f, 0.0f);
    f.level = 0.6f;
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
    constexpr float heavy = 3.0f;                     // (a bead this wide or wider runs down)
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
                // it pulls back from its splash into a bead; flung droplets skid to a stop
                d.spread *= std::exp (-dt / 0.09f);
                const float k = std::exp (-dt / 0.035f);
                d.sx += d.vX * (1.0f - k) * 0.035f; d.sy += d.vY * (1.0f - k) * 0.035f; d.vX *= k; d.vY *= k;
                // heavy enough, it runs down in fits and starts (sticking and slipping), faster the bigger it is,
                // leaving a wet track and now and then a speck behind, and losing a little as it goes
                if (d.size >= heavy && d.slideAt <= 0.0f) d.slideAt = (float) elapsed + uniform (rng, 0.05f, 0.4f);
                if (d.slideAt > 0.0f && (float) elapsed > d.slideAt && d.sy < top + height - 0.5f * d.size - 1.0f)
                {
                    const float v = jmin (45.0f, 6.0f + 10.0f * (d.size - 1.6f)) * (rng.nextFloat() < 0.12f ? 0.0f : uniform (rng, 0.5f, 1.5f));
                    d.sy += v * dt; d.sx += 1.5f * gauss (rng) * dt; d.travel += v * dt;
                    for (; d.travel > 1.2f; d.travel -= 1.2f)
                    {
                        addDrop (onGlass (d, Drop::Type::track, d.sx, d.sy - 0.4f * d.size, 0.55f * d.size, uniform (rng, 1.2f, 2.2f), 0.0f));
                        if (rng.nextFloat() < 0.1f)
                            addDrop (onGlass (d, Drop::Type::bead, d.sx + 0.25f * d.size * gauss (rng), d.sy - 0.6f * d.size,
                                              uniform (rng, 0.35f, 0.7f), uniform (rng, 1.0f, 2.5f), 0.0f));
                        d.size *= 0.994f;
                    }
                    d.life = jmax (d.life, age + 1.5f);       // (it lasts while it runs)
                }
                break;
            }
            case Drop::Type::track: case Drop::Type::ring: case Drop::Type::flash: case Drop::Type::wave: break;
        }
        drops[i] = d;
    }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [this] (const Drop& d) { return (float) elapsed - d.born > d.life; }), drops.end());
    coalesce();
}

// beads that touch merge (their volumes add up, the bigger one keeps its place more), so a running bead sweeps up those
// in its way and grows; found through a grid of 6 design px cells
void Stripe::coalesce()
{
    constexpr float cell = 6.0f;
    const int nx = (int) (width / cell) + 2, ny = (int) (height / cell) + 2;
    gridHead.assign ((size_t) (nx * ny), -1);
    gridNext.assign (drops.size(), -1);
    auto cellOf = [&] (const Drop& d) { return jlimit (0, ny - 1, (int) ((d.sy - top) / cell)) * nx + jlimit (0, nx - 1, (int) ((d.sx - left) / cell)); };
    for (int i = 0; i < (int) drops.size(); ++i)
    {
        const auto& d = drops[(size_t) i];
        if (d.type != Drop::Type::bead || (float) elapsed < d.born) continue;
        const int c = cellOf (d);
        gridNext[(size_t) i] = gridHead[(size_t) c]; gridHead[(size_t) c] = i;
    }
    for (int i = 0; i < (int) drops.size(); ++i)
    {
        auto& a = drops[(size_t) i];
        if (a.type != Drop::Type::bead || a.life <= 0.0f || (float) elapsed < a.born) continue;
        const int cx = jlimit (0, nx - 1, (int) ((a.sx - left) / cell)), cy = jlimit (0, ny - 1, (int) ((a.sy - top) / cell));
        for (int gy = jmax (0, cy - 1); gy <= jmin (ny - 1, cy + 1); ++gy)
            for (int gx = jmax (0, cx - 1); gx <= jmin (nx - 1, cx + 1); ++gx)
                for (int j = gridHead[(size_t) (gy * nx + gx)]; j >= 0; j = gridNext[(size_t) j])
                {
                    if (j == i) continue;
                    auto& b = drops[(size_t) j];
                    if (b.life <= 0.0f || a.life <= 0.0f) continue;
                    if (std::hypot (a.sx - b.sx, a.sy - b.sy) >= 0.42f * (a.size + b.size)) continue;
                    auto& big = a.size >= b.size ? a : b;
                    auto& small = a.size >= b.size ? b : a;
                    const float va = big.size * big.size * big.size, vb = small.size * small.size * small.size, w = vb / (va + vb);
                    big.sx += (small.sx - big.sx) * w; big.sy += (small.sy - big.sy) * w;
                    big.size = std::cbrt (va + vb);
                    const float end = jmax (big.born + big.life, small.born + small.life);
                    big.life = end - big.born;
                    big.spread = jmax (big.spread, 0.3f * small.spread);
                    small.life = -1.0f;                         // (gone: the bigger has it)
                }
    }
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

// a disc of light, flat with a slightly brighter rim and a soft edge: an out-of-focus drop
void Stripe::splatDisc (float x, float y, float radius, float level, float r, float g, float b, float s)
{
    const float cx = (x - left) * s, cy = (y - top) * s, R = jmax (0.6f, radius * s);
    const int i0 = jmax (0, (int) (cx - R - 2.0f)), i1 = jmin (bufW - 1, (int) (cx + R + 2.0f));
    const int j0 = jmax (0, (int) (cy - R - 2.0f)), j1 = jmin (bufH - 1, (int) (cy + R + 2.0f));
    for (int j = j0; j <= j1; ++j)
    {
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
        for (int i = i0; i <= i1; ++i, row += 3)
        {
            const float dx = (float) i + 0.5f - cx, dy = (float) j + 0.5f - cy, rho = std::sqrt (dx * dx + dy * dy) / R;
            if (rho > 1.0f + 2.0f / R) continue;
            const float r4 = rho * rho * rho * rho;
            const float v = level * (rho <= 1.0f ? 0.8f + 0.35f * r4 : 1.15f * std::exp (-(rho - 1.0f) * R / 0.7f));
            row[0] += v * r; row[1] += v * g; row[2] += v * b;
        }
    }
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
        if (age < 0.0f || age > d.life) continue;
        const float u = jlimit (0.0f, 1.0f, age / jmax (0.001f, d.life));
        Particle p {};
        p.x = d.sx; p.y = d.sy; p.r = d.r; p.g = d.g; p.b = d.b; p.kind = Kind::blob;
        switch (d.type)
        {
            case Drop::Type::spray:
            case Drop::Type::glob:
            {
                // depth of field (in focus at 0.45 of the way): the far ones soft, the ones in focus sharp drops
                // streaking, the nearest big discs, as dim as their light is spread
                const float near = jlimit (0.0f, 1.0f, 1.1f - d.Z), z = jmax (0.04f, d.Z);
                const float size = jmin (d.type == Drop::Type::glob ? 15.0f : 6.0f, d.size * 0.6f / z);
                const float level = d.level * (0.06f + 1.0f * near * near) * jmin (1.0f, age / 0.08f) * e;
                if (level <= 0.004f) break;
                const float blur = jmin (9.0f, 0.9f * std::abs (1.0f / z - 1.0f / 0.45f));
                if (blur < 1.3f) droplet (d.sx, d.sy, d.px - d.sx, d.py - d.sy, jmax (0.6f, size), level, d, s);
                else
                {
                    const float spreadOut = (size * size) / ((size + 2.0f * blur) * (size + 2.0f * blur));
                    if (d.Z < 0.45f) splatDisc (d.sx, d.sy, 0.5f * size + blur, 1.6f * level * jmax (0.04f, spreadOut), d.r, d.g, d.b, s);
                    else splatGlow (d.sx, d.sy, 0.5f * size + 0.5f * blur, 0.5f * size + 0.5f * blur, level * jmax (0.25f, spreadOut), d.r, d.g, d.b, s);
                }
                break;
            }
            case Drop::Type::mist:
            {
                const float level = d.level * jlimit (0.0f, 1.0f, 1.1f - d.Z) * e;
                if (level > 0.004f) splatLine (d.px, d.py, d.sx, d.sy, 0.5f, level, d.r, d.g, d.b, s);
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
                p.kind = Kind::dot; p.w = p.h = 0.6f;
                const int n = jlimit (6, 24, (int) (radius * 2.0f));
                for (int k = 0; k < n; ++k)
                {
                    const float a = MathConstants<float>::twoPi * ((float) k + d.seed) / (float) n;
                    p.x = d.sx + radius * std::cos (a); p.y = d.sy + 0.7f * radius * std::sin (a);
                    splat (p, level, s);
                }
                break;
            }
            case Drop::Type::flash:
                p.w = d.size * (0.5f + 0.8f * u); p.h = p.w * 0.45f;
                splat (p, d.level * (1.0f - u) * (1.0f - u) * e, s);
                break;
            case Drop::Type::bead: case Drop::Type::track: break;        // (on the glass: renderGlass)
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

//==============================================================================
// ours: the glass. Its edges are darker (the tube's rim shades it), and it has dust and fine scratches (a fixed
// pattern, drawn once per size) that show only where light falls on them
void Stripe::prepareGlass (int w, int h, float s)
{
    glassW = w; glassH = h;
    vignette.assign ((size_t) (w * h), 1.0f);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const float x = ((float) i + 0.5f) / s, y = ((float) j + 0.5f) / s;
            const float ey = jmax (0.0f, 1.0f - jmin (y, height - y) / 5.0f), ex = jmax (0.0f, 1.0f - jmin (x, width - x) / 40.0f);
            vignette[(size_t) (j * w + i)] = 1.0f - 0.35f * ey * ey - 0.4f * ex * ex;
        }
    dust.assign ((size_t) (w * h), 0.0f);
    Random r (77);
    for (int k = (w * h) / 700; --k >= 0;)
        dust[(size_t) (r.nextInt (h) * w + r.nextInt (w))] += uniform (r, 0.15f, 0.6f);
    for (int k = 0; k < 28; ++k)
    {
        // a scratch: a faint, slightly curved line, mostly along the stripe
        float x = uniform (r, 0.0f, (float) w), y = uniform (r, 0.0f, (float) h);
        float a = uniform (r, -0.5f, 0.5f) + (r.nextFloat() < 0.3f ? MathConstants<float>::halfPi : 0.0f);
        const float len = uniform (r, 4.0f, 22.0f) * s, level = uniform (r, 0.06f, 0.16f), bend = 0.02f * gauss (r);
        for (float t = 0.0f; t < len; t += 0.5f)
        {
            a += bend * 0.5f;
            x += 0.5f * std::cos (a); y += 0.5f * std::sin (a);
            const int ix = (int) x, iy = (int) y;
            if (ix >= 0 && ix < w && iy >= 0 && iy < h) dust[(size_t) (iy * w + ix)] = jmax (dust[(size_t) (iy * w + ix)], level);
        }
    }
}

// what is on the glass: the wet tracks, the beads (lenses), and over them the glass's own reflection: a faint band of
// light drifting slowly across it, and a thin line of light along its top edge
void Stripe::renderGlass (float s)
{
    for (const auto& d : drops)
        if (d.type == Drop::Type::track && (float) elapsed >= d.born && (float) elapsed - d.born <= d.life) wetTrack (d, s);
    for (const auto& d : drops)
        if (d.type == Drop::Type::bead && (float) elapsed >= d.born && (float) elapsed - d.born <= d.life) lens (d, s);
    const float band = std::fmod ((float) elapsed * 0.035f, 1.6f) * width * 1.0f - 0.3f * width;   // (where it is now)
    for (int j = 0; j < bufH; ++j)
    {
        const float y = ((float) j + 0.5f) / s;
        const float edge = y < 1.5f ? 0.05f * (1.5f - y) / 1.5f : 0.0f;
        const float c = band + 0.9f * y;                                   // (slanted)
        const int i0 = jmax (0, (int) ((c - 90.0f) * s)), i1 = jmin (bufW - 1, (int) ((c + 90.0f) * s));
        float* row = gloss.data() + (size_t) (j * bufW) * 3;
        if (edge > 0.0f) for (int i = 0; i < bufW; ++i) { row[i * 3] += edge; row[i * 3 + 1] += edge; row[i * 3 + 2] += edge; }
        for (int i = i0; i <= i1; ++i)
        {
            const float x = ((float) i + 0.5f) / s, t = (x - c) / 34.0f, v = 0.035f * std::exp (-t * t);
            row[i * 3] += 0.85f * v; row[i * 3 + 1] += 0.95f * v; row[i * 3 + 2] += v;
        }
    }
}

// a bead of venom on the inside of the glass is a lens: it shows the scene behind it upside down and small, darker
// towards its rim (light bent away), with a caustic at its foot (light gathered through it); it is lit from behind (a
// bright rim on the side the heart's light comes from), and seen through the glass (a little dimmer, the glass's
// reflections over it); just after it hits it is a flat, uneven film; drying, it shrinks
void Stripe::lens (const Drop& d, float s)
{
    const float age = (float) elapsed - d.born, u = age / jmax (0.001f, d.life);
    const float evaporate = jlimit (0.0f, 1.0f, (1.0f - u) / 0.35f);
    if (evaporate <= 0.0f || envelope <= 0.002f) return;
    const float size = d.size * std::sqrt (evaporate);
    const float R = 0.5f * size * s * (1.0f + 0.8f * d.spread), Ry = R * (0.92f - 0.3f * d.spread);
    if (R < 0.4f) return;
    const float cx = (d.sx - left) * s, cy = (d.sy - top) * s;
    const int i0 = jmax (0, (int) (cx - 1.3f * R - 1.0f)), i1 = jmin (bufW - 1, (int) (cx + 1.3f * R + 1.0f));
    const int j0 = jmax (0, (int) (cy - 1.3f * Ry - 1.0f)), j1 = jmin (bufH - 1, (int) (cy + 1.3f * Ry + 1.0f));
    const float* behind = scene.data() + (size_t) ((jlimit (0, bufH - 1, (int) cy) * bufW + jlimit (0, bufW - 1, (int) cx)) * 3);
    const float glow = 0.3f * behind[0] + 0.55f * behind[1] + 0.15f * behind[2];
    const float toLight = std::atan2 ((glandY - d.sy) * 3.0f, heartX - d.sx);       // (the heart's side, mostly along)
    const float shown = jmin (1.0f, envelope * 1.5f) * jmin (1.0f, 6.0f * evaporate);
    constexpr float minify = 2.6f;
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
            const float dx = (float) i + 0.5f - cx, dy = (float) j + 0.5f - cy;
            float rho = std::sqrt ((dx / R) * (dx / R) + (dy / Ry) * (dy / Ry));
            if (d.spread > 0.02f)
            {
                const float th = std::atan2 (dy, dx), seed = MathConstants<float>::twoPi * d.seed;
                rho /= 1.0f + d.spread * (0.16f * std::sin (5.0f * th + seed) + 0.08f * std::sin (9.0f * th + 2.0f * seed));
            }
            if (rho >= 1.1f) continue;
            const float a = jlimit (0.0f, 1.0f, (1.05f - rho) / 0.1f) * shown;
            const int sx = jlimit (0, bufW - 1, (int) (cx - dx * minify)), sy = jlimit (0, bufH - 1, (int) (cy - dy * minify));
            const float* c = scene.data() + (size_t) ((sy * bufW + sx) * 3);
            const float rim = jlimit (0.0f, 1.0f, (rho - 0.55f) / 0.45f), dark = 1.0f - 0.72f * rim * rim;
            const float foot = dy > 0.0f ? dy / Ry : 0.0f, caustic = foot * std::exp (-((rho - 0.8f) / 0.11f) * ((rho - 0.8f) / 0.11f)) * (0.15f + 0.9f * glow);
            const float facing = jmax (0.0f, std::cos (std::atan2 (dy / Ry, dx / R) - toLight));
            const float backlit = facing * std::exp (-((rho - 0.88f) / 0.09f) * ((rho - 0.88f) / 0.09f)) * (0.12f + 0.8f * glow);
            const float out[3] { 0.9f * (c[0] * dark + 0.05f * d.r + caustic * (0.4f + 0.6f * d.r) + backlit * (0.5f + 0.5f * d.r)),
                                 0.9f * (c[1] * dark + 0.05f * d.g + caustic * (0.4f + 0.6f * d.g) + backlit * (0.5f + 0.5f * d.g)),
                                 0.9f * (c[2] * dark + 0.05f * d.b + caustic * (0.4f + 0.6f * d.b) + backlit * (0.5f + 0.5f * d.b)) };
            const size_t k = (size_t) (j * bufW + i);
            float* f = front.data() + k * 3;
            for (int ch = 0; ch < 3; ++ch) f[ch] = f[ch] * (1.0f - a) + a * out[ch];
            cover[k] += a * (1.0f - cover[k]);
        }
}

// the wet track a running bead leaves: a thin film, which bends what is behind it a little (sideways), with a faint line
// of light along one edge, drying from its ends
void Stripe::wetTrack (const Drop& d, float s)
{
    const float u = ((float) elapsed - d.born) / jmax (0.001f, d.life), a = 0.35f * (1.0f - u) * jmin (1.0f, envelope * 1.5f);
    if (a <= 0.003f) return;
    const float cx = (d.sx - left) * s, halfW = jmax (0.6f, 0.5f * d.size * s * (1.0f - 0.5f * u));
    const int i0 = jmax (0, (int) (cx - halfW)), i1 = jmin (bufW - 1, (int) (cx + halfW));
    const int j0 = jmax (0, (int) ((d.sy - 0.7f - top) * s)), j1 = jmin (bufH - 1, (int) ((d.sy + 0.7f - top) * s));
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
            const int sx = jlimit (0, bufW - 1, (int) (cx - 1.6f * ((float) i + 0.5f - cx)));
            const float* c = scene.data() + (size_t) ((j * bufW + sx) * 3);
            const size_t k = (size_t) (j * bufW + i);
            float* f = front.data() + k * 3;
            for (int ch = 0; ch < 3; ++ch) f[ch] = f[ch] * (1.0f - a) + a * jmin (1.0f, 1.1f * c[ch]);
            cover[k] += a * (1.0f - cover[k]);
        }
    // (its edge catches the light from behind, a little)
    for (int j = j0; j <= j1; ++j)
        if (i0 >= 0 && i0 < bufW) { float* px = front.data() + (size_t) ((j * bufW + i0) * 3); px[0] += 0.08f * a; px[1] += 0.1f * a; px[2] += 0.1f * a; }
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
