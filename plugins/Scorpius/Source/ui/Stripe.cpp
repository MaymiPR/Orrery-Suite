#include "Stripe.h"
#include "StripeGlow.h"
#include "shared/ui/Theme.h"

using namespace juce;
namespace theme = orrery::theme;

namespace sc
{
namespace
{
// ORION style: the particles' tints and their shares, in the reference's balance of hues (its saturated pixels in
// 30-degree bins: 0.18 0.19 0.02 / 0.15 0.13 0.16 0.03 0.03 0.09 from red-orange to pink, UI round 8), made luminous
// (the cores are drawn white-hot, the tints colour the halos)
struct Tint { float r, g, b, share; };
constexpr Tint palette[] {
    { 1.00f, 0.95f, 0.85f, 0.05f },     // star white
    { 1.00f, 0.32f, 0.08f, 0.13f },     // ember (hue 15)
    { 1.00f, 0.66f, 0.12f, 0.19f },     // gold (45)
    { 1.00f, 0.92f, 0.25f, 0.03f },     // yellow (75)
    { 0.10f, 0.80f, 1.00f, 0.15f },     // cyan (195)
    { 0.20f, 0.50f, 1.00f, 0.13f },     // azure (225)
    { 0.45f, 0.38f, 1.00f, 0.16f },     // lavender (255)
    { 0.75f, 0.35f, 1.00f, 0.03f },     // violet (285)
    { 1.00f, 0.25f, 0.50f, 0.09f } };   // rose (345)
constexpr Tint flareTints[] {
    { 1.00f, 0.97f, 0.90f, 0.25f }, { 1.00f, 0.75f, 0.25f, 0.25f }, { 0.30f, 0.80f, 1.00f, 0.15f },
    { 1.00f, 0.35f, 0.25f, 0.15f }, { 0.65f, 0.45f, 1.00f, 0.20f } };
constexpr int numParticles = 560, numFlares = 9;
// ORION style (ours): a black hole past each end of the tube, off screen, drawing in what comes near
// (reaching 80 px into the tube: the stream pinches into a line close to the ends)
constexpr float holeX[2] { Stripe::left - 12.0f, Stripe::left + Stripe::width + 12.0f }, holeY = 291.0f, holeReach = 80.0f, holeFunnel = 90.0f;
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
        particles[i].kind = i < (size_t) numParticles ? Kind::dot : Kind::flare;     // (spawn picks the others)
        spawn (particles[i], 0.0, true);
    }
}

void Stripe::spawn (Particle& p, double now, bool anyAge)
{
    const auto& t = p.kind == Kind::flare ? pick (rng, flareTints) : pick (rng, palette);
    p.r = t.r; p.g = t.g; p.b = t.b;
    p.expand = 0.0f; p.vx = 0.0f; p.vy = 0.0f;
    p.flicker = uniform (rng, 0.0f, MathConstants<float>::twoPi);
    p.rate = uniform (rng, 3.0f, 9.0f);
    if (p.kind == Kind::flare)
    {
        // a pillar of light: swells and fades within 0.1 ... 0.25 s, sliding a little
        p.x = jlimit (40.0f, 848.0f, 444.0f + 230.0f * gauss (rng));
        p.y = 289.0f + 2.0f * gauss (rng);
        p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 0.0f, 18.0f);
        p.w = uniform (rng, 2.2f, 3.4f);
        p.h = p.w + uniform (rng, 6.0f, 18.0f);
        p.level = uniform (rng, 0.9f, 1.2f);
        p.life = uniform (rng, 0.1f, 0.25f);
        p.born = (float) (now + (anyAge ? uniform (rng, -0.25f, 0.2f) : uniform (rng, 0.0f, 0.2f)));
        return;
    }
    // along the stripe from end to end: half as the reference's density (more in the middle), half evenly
    p.x = rng.nextBool() ? fromBins (rng, alongBins, 0.0f, 18.5f) : uniform (rng, left + 2.0f, left + width - 2.0f);
    // (ours, calmer than the reference's re-scatter every frame: each glows up and dies away over a fraction of a second
    // like a neon tube, the whole field drifting slowly outwards to the black holes past the ends)
    const float side = p.x < 444.0f ? -1.0f : 1.0f;
    p.vx = side * (8.0f + 0.08f * std::abs (p.x - 444.0f)) + 4.5f * gauss (rng);
    const float u = rng.nextFloat();
    if (u < 0.46f)
    {
        // a streak of neon in the lanes through the middle (most of the band)
        p.kind = Kind::dash;
        p.y = rng.nextFloat() < 0.6f ? lanes[rng.nextInt (3)] + 0.15f * gauss (rng) : uniform (rng, 287.5f, 296.0f);
        p.w = uniform (rng, 2.5f, 6.5f); p.h = uniform (rng, 0.7f, 1.0f);
        p.level = uniform (rng, 0.55f, 0.9f);
        p.life = uniform (rng, 0.13f, 0.32f);
    }
    else if (u < 0.72f)
    {
        // a spark in the band
        p.kind = Kind::dot;
        p.y = 291.5f + 2.4f * gauss (rng);
        p.w = p.h = uniform (rng, 0.7f, 1.3f);
        p.level = uniform (rng, 0.6f, 1.0f);
        p.life = uniform (rng, 0.1f, 0.28f);
    }
    else if (u < 0.82f)
    {
        // a stray spark above or below the band (few, and dimmer the further out)
        p.kind = Kind::dot;
        p.y = jmax (277.0f, fromBins (rng, dotBins, 272.5f, 2.0f));
        p.w = p.h = uniform (rng, 0.6f, 1.0f);
        p.level = uniform (rng, 0.4f, 0.8f) * (1.0f - 0.5f * jmin (1.0f, std::abs (p.y - 291.0f) / 15.0f));
        p.life = uniform (rng, 0.14f, 0.35f);
    }
    else if (u < 0.96f)
    {
        // a streamer: out of the band, flowing outwards (the rate set as it moves: advance), to the black holes
        p.kind = Kind::dot;
        const bool above = rng.nextFloat() < 0.7f;
        p.y = above ? uniform (rng, 279.0f, 287.0f) : uniform (rng, 296.0f, 301.0f);
        p.expand = uniform (rng, 0.18f, 0.3f);
        p.vy = (above ? -1.0f : 1.0f) * uniform (rng, 5.0f, 15.0f) * (above ? 1.0f : 0.5f);
        p.w = p.h = uniform (rng, 0.7f, 1.2f);
        p.level = uniform (rng, 0.6f, 1.0f);
        p.life = uniform (rng, 0.5f, 1.1f);
    }
    else if (u < 0.985f)
    {
        // ours: a star that glints, its rays swelling and shrinking
        p.kind = Kind::star;
        p.y = 291.0f + 2.5f * gauss (rng);
        p.w = uniform (rng, 3.0f, 7.0f); p.h = p.w;
        p.level = uniform (rng, 0.8f, 1.1f);
        p.life = uniform (rng, 0.2f, 0.4f);
    }
    else
    {
        // ours: a soft orb drifting in the haze
        p.kind = Kind::blob;
        p.y = uniform (rng, 280.0f, 301.0f);
        p.vy = -uniform (rng, 0.0f, 3.0f);
        p.w = p.h = uniform (rng, 2.0f, 3.5f);
        p.level = uniform (rng, 0.15f, 0.3f);
        p.life = uniform (rng, 1.0f, 2.0f);
    }
    // (ours) near an end, a funnel into the black hole past it: the band pinches towards its height, thinner the closer
    const float k = jmax (0.0f, 1.0f - jmin (p.x - holeX[0], holeX[1] - p.x) / holeReach);
    if (k > 0.0f) p.y = holeY + (p.y - holeY) * (1.0f - 0.85f * k);
    p.born = (float) (anyAge ? now - uniform (rng, 0.0f, p.life) : now);
}

void Stripe::setInput (float levelDb, bool jumpedUp, bool anim)
{
    if (frozen) return;
    if (anim != animated) repaint();         // (ours: the scorpion shows in silence only while the animation is on)
    animated = anim;
    levelIn = animated ? levelDb : -200.0f;
    beat = beat || (animated && jumpedUp);
    // (moved on by the editor's clock, so the whole editor repaints once per frame)
    const double now = clock();
    const bool shedding = style == Style::scorpius && animated && (presence > 0.002f || ! motes.empty());
    if (envelope > 0.002f || brightness (levelIn) > 0.0f || shedding)
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
        strike = flash = presence = 0.0f;     // (ours: the scorpions at rest)
        motes.clear();
        if (style == Style::scorpius) { struck = 0.0f; buildScorpion (0.0f, bodyTime); }
        repaint();
    }
}

void Stripe::freeze (float b)
{
    frozen = true;
    envelope = b;
    // a moment well into the animation, so ages and flares are spread as while it runs (ours: the scorpion's motes
    // streaming out from end to end, its tail at rest)
    const int frames = style == Style::scorpius ? 240 : 60;
    for (int i = 0; i < frames; ++i)
    {
        elapsed += 1.0 / 60.0;
        for (auto& p : particles) if (elapsed - p.born > p.life) spawn (p, elapsed, false);
        if (style == Style::scorpius) advanceScorpion (1.0f / 60.0f, 0.75f * pace / 60.0f);
    }
    repaint();
}

void Stripe::setBands (float lows, float mid)
{
    // (0 at -40 dBFS, 1 at -10)
    auto level = [] (float a) { return jlimit (0.0f, 1.0f, (20.0f * std::log10 (jmax (a, 1.0e-6f)) + 40.0f) / 30.0f); };
    bass = level (lows); mids = level (mid);
}

void Stripe::kick()
{
    kicked = true;
}

void Stripe::setPlaying (bool p)
{
    playing = p;
}

void Stripe::setTempo (float bpm)
{
    paceTarget = bpm > 0.0f ? jlimit (1.0f, 3.5f, bpm / 60.0f) : 2.0f;
}

void Stripe::setStyle (Style s)
{
    if (s == style) return;
    style = s;
    motes.clear();
    for (auto& p : particles) spawn (p, elapsed, true);
    glowScale = 0.0f;                                       // (and its own glow)
    repaint();
}

void Stripe::advance (double now)
{
    const double real = jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    // the particles' time runs at the project's tempo (ours: the scorpion's motes at three quarters of the ORION style's
    // pace)
    pace += (paceTarget - pace) * (float) (1.0 - std::exp (-real / 0.5));
    const double dt = real * pace;
    elapsed += dt;
    // the light eases to the level's brightness
    envelope += (brightness (levelIn) - envelope) * (float) (1.0 - std::exp (-real / easing));
    if (style == Style::scorpius) advanceScorpion ((float) real, (float) (0.75 * dt));
    beat = false;
    for (auto& p : particles)
    {
        if (elapsed - p.born > p.life) { if (p.kind != Kind::flare) p.kind = Kind::dot; spawn (p, elapsed, false); }
        // (streaming outwards from x 444: the reference's ~30 px/s near it and 0.3 ... 0.46 times the distance on top
        // (UI round 9); ours a little calmer)
        if (p.expand > 0.0f) p.vx = (p.x < 444.0f ? -1.0f : 1.0f) * (18.0f + p.expand * std::abs (p.x - 444.0f));
        // (ours) near an end the black hole past it draws it in: faster the closer, funnelled to the hole's height; off
        // the end of the tube it is gone (and it does not rise out of the tube either)
        if (style == Style::orion && p.kind != Kind::flare && p.life > 0.1f)
        {
            const float hx = p.x < 444.0f ? holeX[0] : holeX[1], dx = hx - p.x, d = std::abs (dx);
            if (d < holeReach)
            {
                const float pull = 400000.0f / (d * d + 400.0f);           // (px/s^2)
                p.vx += (dx > 0.0f ? 1.0f : -1.0f) * pull * (float) dt;
                p.vy += (holeY - p.y) * (1.0f - d / holeReach) * holeFunnel * (float) dt;
                p.vy *= (float) std::exp (-dt / 0.15);
                p.expand = 0.0f;                                            // (the hole has it now)
            }
            if (p.x < left || p.x > left + width || p.y < 277.0f || p.y > 304.0f) p.born = (float) elapsed - p.life - 1.0f;
        }
        p.x += p.vx * (float) dt;
        p.y += p.vy * (float) dt;
    }
}

void Stripe::renderGlow (int w, int h, float s)
{
    glow.assign ((size_t) (w * h * 3), 0.0f);
    glowScale = s;
    if (style == Style::scorpius) return;           // (ours: no fixed glow; the scorpion has its own haze)
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
    // (the reference's, measured with music: UI round 8) a faint violet haze above the band, brightest in the middle;
    // and (UI round 9) less steady glow in the band itself, whose light is more its particles' than ours was
    for (int j = 0; j < h; ++j)
    {
        const float y = top + ((float) j + 0.5f) / s;
        const float band = 1.0f - 0.2f * jlimit (0.0f, 1.0f, std::min (y - 285.0f, 298.0f - y) / 1.5f);
        for (int i = 0; i < w * 3; ++i) glow[(size_t) (j * w * 3 + i)] *= band;
        const float rows = jlimit (0.0f, 1.0f, (287.0f - y) / 3.0f) * jlimit (0.0f, 1.0f, (y - 272.5f) / 1.5f);
        if (rows <= 0.0f) continue;
        for (int i = 0; i < w; ++i)
        {
            const float x = left + ((float) i + 0.5f) / s, along = 0.45f + 0.55f * std::exp (-std::pow ((x - 460.0f) / 300.0f, 2.0f));
            float* px = glow.data() + (size_t) ((j * w + i) * 3);
            px[0] += 0.018f * rows * along; px[1] += 0.014f * rows * along; px[2] += 0.026f * rows * along;
        }
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

// ORION style: one particle's light. Starlike: a white-hot core in a halo of its colour; streaks a line of light with
// a whiter middle; streamers a short trail; pillars a glowing capsule in a tall soft halo; ours: glinting stars with long
// thin rays, and soft orbs
void Stripe::heavenly (const Particle& p, float level, float u, float s)
{
    auto whiter = [&] (Particle q, float k) { q.r += (1.0f - q.r) * k; q.g += (1.0f - q.g) * k; q.b += (1.0f - q.b) * k; return q; };
    // (ours) like neon: a hot core a little whiter than its colour, in a glow of the pure colour and a wide faint bloom;
    // near the black holes thinner and drawn out along its way
    const float k = jmax (0.0f, 1.0f - jmin (p.x - holeX[0], holeX[1] - p.x) / holeReach), thin = 1.0f - 0.55f * k;
    switch (p.kind)
    {
        case Kind::dot:
        {
            const float trail = p.expand > 0.0f || k > 0.0f ? 0.04f + 0.08f * k : 0.0f;
            if (trail > 0.0f)
                splatLine (p.x - p.vx * trail, p.y - p.vy * trail, p.x, p.y, p.w * thin, 0.5f * level, p.r, p.g, p.b, s);
            Particle core = whiter (p, 0.2f);
            core.w *= thin; core.h *= thin;
            splat (core, level, s);
            splatGlow (p.x, p.y, 1.4f + 1.2f * p.w, (1.2f + 1.0f * p.w) * thin, 0.42f * level, p.r, p.g, p.b, s);
            splatGlow (p.x, p.y, 4.0f, 3.0f * thin, 0.1f * level, p.r, p.g, p.b, s);
            break;
        }
        case Kind::dash:
        {
            Particle tube = p;
            tube.w *= 1.0f + 1.5f * k; tube.h *= thin;
            splat (tube, 0.8f * level, s);
            Particle core = whiter (tube, 0.35f);
            core.w *= 0.75f; core.h *= 0.45f;
            splat (core, 0.45f * level, s);
            splatGlow (p.x, p.y, 0.6f * tube.w + 1.4f, 1.5f * thin, 0.4f * level, p.r, p.g, p.b, s);
            splatGlow (p.x, p.y, 0.6f * tube.w + 4.0f, 3.2f * thin, 0.1f * level, p.r, p.g, p.b, s);
            break;
        }
        case Kind::star:
        {
            const float swell = std::sin (MathConstants<float>::pi * jlimit (0.0f, 1.0f, u)), ray = p.w * swell;
            const Particle w = whiter (p, 0.5f);
            splatLine (p.x - ray, p.y, p.x, p.y, 0.35f, 0.7f * level, w.r, w.g, w.b, s);
            splatLine (p.x + ray, p.y, p.x, p.y, 0.35f, 0.7f * level, w.r, w.g, w.b, s);
            splatLine (p.x, p.y - 0.45f * ray, p.x, p.y, 0.3f, 0.5f * level, w.r, w.g, w.b, s);
            splatLine (p.x, p.y + 0.45f * ray, p.x, p.y, 0.3f, 0.5f * level, w.r, w.g, w.b, s);
            Particle core = whiter (p, 0.8f);
            core.kind = Kind::blob; core.w = core.h = 0.9f + 0.6f * swell;
            splat (core, level, s);
            splatGlow (p.x, p.y, 2.5f * swell + 1.0f, 2.0f * swell + 1.0f, 0.3f * level, p.r, p.g, p.b, s);
            break;
        }
        case Kind::blob:
            splatGlow (p.x, p.y, p.w, p.h, level, p.r, p.g, p.b, s);
            break;
        case Kind::flare:
        {
            const float swell = std::sin (MathConstants<float>::pi * jlimit (0.0f, 1.0f, u));
            Particle tube = whiter (p, 0.2f);
            tube.h = p.w + (p.h - p.w) * swell;
            splat (tube, 0.8f * level * swell, s);
            splatGlow (p.x, p.y, 1.6f * p.w, 0.6f * tube.h + 1.0f, 0.4f * level * swell, p.r, p.g, p.b, s);
            break;
        }
        case Kind::faint:
            break;
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
    for (size_t k = 0; k < glow.size(); ++k) light[k] = glow[k] * envelope;
    if (style == Style::scorpius)
        renderScorpion (s);
    else
        for (auto& p : particles)
        {
            const float age = (float) (elapsed - p.born);
            if (age < 0.0f || age > p.life) continue;
            const float u = age / p.life;
            float a = 1.0f;
            if (p.kind != Kind::flare) a = std::pow (std::sin (MathConstants<float>::pi * u), 0.6f);   // (glowing up, dying away)
            a *= 0.88f + 0.12f * std::sin (p.flicker + p.rate * age);
            const float level = p.level * a * envelope;
            if (level <= 0.005f) continue;
            heavenly (p, level, u, s);
        }
    // the particles' light stays inside the tube (none on its rims) and dies away into the black holes past its ends:
    // what is over the glow fades out above y 275 and below 304, and over the last ~40 px at each end
    for (int j = 0; j < h; ++j)
    {
        const float y = top + ((float) j + 0.5f) / s, my = jlimit (0.0f, 1.0f, (y - 273.5f) / 2.0f) * jlimit (0.0f, 1.0f, (305.5f - y) / 2.0f);
        for (int i = 0; i < w; ++i)
        {
            const float x = ((float) i + 0.5f) / s, mx = jlimit (0.0f, 1.0f, jmin (x, width - x) / 40.0f), m = my * mx * mx;
            if (m >= 1.0f) continue;
            const size_t k = (size_t) ((j * w + i) * 3);
            for (int c = 0; c < 3; ++c) light[k + c] = glow[k + c] * envelope + (light[k + c] - glow[k + c] * envelope) * m;
        }
    }
    // (the reference's) the band reflected in the glass's lower rim: a line of light under it, following the light of
    // the lower band above it column by column, strongest towards the ends (measured: at the ends three times ours was);
    // ours: the scorpion's light too
    {
        const int r0 = jlimit (0, h - 1, (int) ((296.0f - top) * s)), r1 = jlimit (0, h - 1, (int) ((303.0f - top) * s));
        const int m0 = jlimit (0, h - 1, (int) ((305.7f - top) * s)), m1 = jlimit (0, h - 1, (int) ((306.6f - top) * s));
        for (int i = 0; i < w; ++i)
        {
            float acc[3] {};
            for (int j = r0; j <= r1; ++j) for (int c = 0; c < 3; ++c) acc[c] += light[(size_t) ((j * w + i) * 3 + c)];
            const float x = left + ((float) i + 0.5f) / s, gain = 0.7f * (0.5f + 1.5f * jmin (1.0f, std::abs (x - 444.0f) / 400.0f)) / (float) (r1 - r0 + 1);
            for (int j = m0; j <= m1; ++j)
            {
                const float shape = std::sin (MathConstants<float>::pi * ((float) (j - m0) + 0.5f) / (float) (m1 - m0 + 1));
                for (int c = 0; c < 3; ++c) light[(size_t) ((j * w + i) * 3 + c)] += gain * shape * acc[c];
            }
        }
        if (style == Style::orion) addReflections (w, h, s);
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
    Image::BitmapData bd (image, Image::BitmapData::writeOnly);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const size_t k3 = (size_t) (j * w + i) * 3;
            auto* px = reinterpret_cast<PixelRGB*> (bd.getPixelPointer (i, j));
            px->setARGB (255, (uint8) (scene[k3] * 255.0f + 0.5f), (uint8) (scene[k3 + 1] * 255.0f + 0.5f), (uint8) (scene[k3 + 2] * 255.0f + 0.5f));
        }
}

// (the reference's) the glass's fixed reflections while it is lit (UI round 9: films with music, the 10th percentile
// of the lit frames less the idle frame, set against ours): two dotted lines of light, at y 282.25 and 304, made of
// segments whose bright ends fall at the same places on both (closer together towards the right, as a reflection
// seen at an angle), brightest around the middle and gone at the ends; at both ends of the tube bright bumps on the
// lower line and three short violet lines across the end caps; in grey levels at full brightness
void Stripe::addReflections (int w, int h, float s)
{
    static constexpr float peaks[] { 234.0f, 314.0f, 394.0f, 465.5f, 529.5f, 593.5f, 652.5f, 698.0f, 744.0f, 790.0f };
    static constexpr float topHeight[] { 29, 24, 23, 20, 22, 37, 38, 42, 48, 59 };
    static constexpr float lowHeight[] { 35, 34, 30, 27, 24, 20, 19, 24, 19, 73 };
    static constexpr float endBumps[][2] { { 11.0f, 86 }, { 43.5f, 66 }, { 50.5f, 72 }, { 845.0f, 76 }, { 876.5f, 70 } };
    const float e = envelope / 255.0f;
    auto addAt = [&] (float y, float halfHeight, float x, float level, float r, float g, float b)
    {
        const int i = (int) ((x - left) * s);
        if (i < 0 || i >= w) return;
        const int j0 = jmax (0, (int) ((y - halfHeight - top) * s)), j1 = jmin (h - 1, (int) ((y + halfHeight - top) * s));
        for (int j = j0; j <= j1; ++j)
        {
            float* px = light.data() + (size_t) ((j * w + i) * 3);
            px[0] += level * r; px[1] += level * g; px[2] += level * b;
        }
    };
    for (int i = 0; i < w; ++i)
    {
        const float x = left + ((float) i + 0.5f) / s;
        // the broad level of each line: rising from the left to the middle, falling to nothing past x 800
        const float topBase = 0.0f;                   // (the broad level there is the glow's)
        const float lowBase = x > 630.0f && x < 792.0f ? 22.0f : 0.0f;    // (a dotted run before the last spike)
        // the segments: each ramps up to its bright end (a sawtooth); on the lower line only their ends show, sharply
        float ramp = 0.0f, spike = 0.0f;
        for (int k = 0; k < 10; ++k)
        {
            const float start = k == 0 ? peaks[0] - 60.0f : peaks[k - 1] + 4.0f;
            if (x >= start && x <= peaks[k] + 1.5f) ramp = topHeight[k] * std::pow (jlimit (0.0f, 1.0f, (x - start) / (peaks[k] - start)), 2.0f);
            spike += lowHeight[k] * std::exp (-std::pow ((x - peaks[k]) / 2.2f, 2.0f));
        }
        for (auto& b : endBumps) spike += b[1] * std::exp (-std::pow ((x - b[0]) / 3.0f, 2.0f));
        // dotted (a dot every 3.5 px)
        const float dots = 0.55f + 0.45f * std::cos (MathConstants<float>::twoPi * x / 3.5f);
        const float topLevel = (topBase + ramp) * (ramp > 0.5f ? dots : 1.0f) * e;
        const float lowLevel = (lowBase * (lowBase > 0.0f ? dots : 1.0f) + spike) * e;
        addAt (282.25f, 0.5f, x, topLevel, 1.0f, 0.85f, 1.0f);
        addAt (304.0f, 0.5f, x, lowLevel, 1.0f, 0.93f, 0.98f);
    }
    // (the end caps' three short violet lines: the black holes are there now)
    if (false)
    for (float y : { 289.0f, 291.0f, 293.0f })
        for (float d = 0.0f; d < 8.0f; d += 0.5f / s)
        {
            const float level = (y == 291.0f ? 50.0f : 33.0f) * std::exp (-d / 2.5f) * e;
            addAt (y, 0.25f, 10.5f + d, level, 0.71f, 0.63f, 1.0f);
            addAt (y, 0.25f, 877.5f - d, level, 0.71f, 0.63f, 1.0f);
        }
}

//==============================================================================
// ours (SCORPIUS style): the scorpion in light
namespace
{
// two scorpions back to back, their tails arched towards each other either side of the middle: each drawn in its own
// units (about 145 across, 35 high, side on, facing right, the back of its arched tail at x 24), scaled into the tube's
// height; the first to the right of the middle, the second mirrored to the left
constexpr float scorpionScale = 0.84f, scorpionX = 444.0f, scorpionY = 290.0f, scorpionBack = 24.0f, scorpionGap = 3.0f;
Point<float> toDesign (Point<float> p, int which)
{
    const float x = scorpionGap + (p.x - scorpionBack) * scorpionScale;
    return { which == 0 ? scorpionX + x : scorpionX - x, scorpionY + (p.y - 18.0f) * scorpionScale };
}

struct Rgb { float r, g, b; };
constexpr Rgb coreLight { 0.78f, 1.00f, 0.88f }, mintLight { 0.55f, 1.00f, 0.84f }, greenLight { 0.20f, 0.88f, 0.47f },
              tealLight { 0.08f, 0.67f, 0.59f }, deepLight { 0.04f, 0.43f, 0.27f }, aquaLight { 0.15f, 0.95f, 0.85f },
              tipLight { 0.70f, 1.00f, 1.00f };

// a smooth curve through the points (Catmull-Rom), n points per span
std::vector<Point<float>> curve (const std::vector<Point<float>>& in, int n)
{
    std::vector<Point<float>> out;
    for (size_t i = 0; i + 1 < in.size(); ++i)
    {
        const auto p0 = in[i == 0 ? 0 : i - 1], p1 = in[i], p2 = in[i + 1], p3 = in[jmin (in.size() - 1, i + 2)];
        for (int k = 0; k < n; ++k)
        {
            const float t = (float) k / (float) n, t2 = t * t, t3 = t2 * t;
            auto at = [&] (float a0, float a1, float a2, float a3)
            { return 0.5f * (2.0f * a1 + (-a0 + a2) * t + (2.0f * a0 - 5.0f * a1 + 4.0f * a2 - a3) * t2 + (-a0 + 3.0f * a1 - 3.0f * a2 + a3) * t3); };
            out.push_back ({ at (p0.x, p1.x, p2.x, p3.x), at (p0.y, p1.y, p2.y, p3.y) });
        }
    }
    out.push_back (in.back());
    return out;
}

// part of an ellipse (radii rx, ry, turned by rot) from angle a0 to a1
std::vector<Point<float>> ellipseArc (Point<float> c, float rx, float ry, float a0, float a1, float rot, int n)
{
    std::vector<Point<float>> out;
    for (int k = 0; k <= n; ++k)
    {
        const float t = a0 + (a1 - a0) * (float) k / (float) n, x = std::cos (t) * rx, y = std::sin (t) * ry;
        out.push_back ({ c.x + x * std::cos (rot) - y * std::sin (rot), c.y + x * std::sin (rot) + y * std::cos (rot) });
    }
    return out;
}

// a gaussian blur of an RGB buffer: three box blurs across and three down (zero past the edges)
void blurRgb (const std::vector<float>& src, std::vector<float>& dst, std::vector<float>& tmp, int w, int h, float sigma)
{
    dst = src;
    const int r = (int) std::round ((std::sqrt (4.0f * sigma * sigma + 1.0f) - 1.0f) * 0.5f);
    if (r < 1) return;
    tmp.resize (src.size());
    const float inv = 1.0f / (float) (2 * r + 1);
    auto pass = [&] (const float* in, float* out, int count, int stride, int lines, int lineStride)
    {
        for (int l = 0; l < lines; ++l)
            for (int c = 0; c < 3; ++c)
            {
                const float* a = in + (size_t) (l * lineStride + c);
                float* o = out + (size_t) (l * lineStride + c);
                float sum = 0.0f;
                for (int i = 0; i <= jmin (r, count - 1); ++i) sum += a[(size_t) (i * stride)];
                for (int i = 0; i < count; ++i)
                {
                    o[(size_t) (i * stride)] = sum * inv;
                    if (i + r + 1 < count) sum += a[(size_t) ((i + r + 1) * stride)];
                    if (i - r >= 0) sum -= a[(size_t) ((i - r) * stride)];
                }
            }
    };
    for (int k = 0; k < 3; ++k)
    {
        pass (dst.data(), tmp.data(), w, 3, h, w * 3);      // across
        pass (tmp.data(), dst.data(), h, w * 3, w, 3);      // down
    }
}
}

// the scorpions' strokes for a pose: strike 0 (the tails arched) ... 1 (struck forwards over the heads). Each in its
// own units: side on, facing right, about 145 across and 35 high; tint 0 its green ... 1 the
// cyan-mint of its tips
void Stripe::buildScorpion (float st, float t)
{
    strokes.clear(); glints.clear(); fills.clear();
    auto add = [this] (std::vector<Point<float>> pts, float w, float b, Part part, float tint)
    { strokes.push_back ({ std::move (pts), w, b, part, tint, {} }); };
    auto line = [] (Point<float> a, Point<float> b) { return std::vector<Point<float>> { a, b }; };
    auto dot = [this] (Point<float> p, float r, float level, float tint) { glints.push_back ({ p, r, level, tint }); };
    auto fill = [this] (Point<float> p, float rx, float ry, float level) { fills.push_back ({ p, rx, ry, level }); };
    auto lerp = [] (float a, float b, float k) { return a + (b - a) * k; };
    auto dir = [] (float a) { return Point<float> { std::cos (a), std::sin (a) }; };

    for (int which = 0; which < 2; ++which)
    {
        const size_t s0 = strokes.size(), d0 = glints.size(), f0 = fills.size();
        const float t2 = t + 0.9f * (float) which;          // (the two out of step)
        // the body (mesosoma): seven plates along its back, each rising to a ridge, a soft belly, a dashed line along its side
        std::vector<Point<float>> back { { 45.0f, 21.2f } };
        for (int i = 0; i < 7; ++i)
        {
            const float e = 46.0f + 6.0f * (float) i;
            back.insert (back.end(), { { e + 0.3f, 20.7f }, { e + 1.6f, 19.4f }, { e + 5.7f, 19.0f } });
        }
        back.push_back ({ 88.0f, 19.3f });
        add (back, 1.0f, 1.0f, Part::body, 0.0f);
        add (curve ({ { 45, 26.8f }, { 52, 28.2f }, { 66, 28.8f }, { 80, 28.6f }, { 88, 28.4f } }, 4), 1.0f, 1.0f, Part::body, 0.0f);
        add (curve ({ { 45, 21.2f }, { 43.6f, 24 }, { 45, 26.8f } }, 3), 1.0f, 1.0f, Part::body, 0.0f);
        for (int i = 1; i < 7; ++i) { const float x = 46.0f + 6.0f * (float) i; add (line ({ x, 19.4f }, { x - 0.4f, 22.6f }), 0.45f, 0.6f, Part::detail, 0.0f); }
        for (float x = 47.0f; x < 86.0f; x += 3.2f) add (line ({ x, 24.6f }, { x + 1.7f, 24.7f }), 0.35f, 0.45f, Part::detail, 0.2f);
        for (float x = 50.0f; x < 87.0f; x += 7.0f) fill ({ x, 24.0f }, 4.5f, 3.6f, 1.0f);
        // the head (carapace): a groove, the eyes on their mound and at the front, the small jaws
        add (curve ({ { 88, 19.3f }, { 92, 18.5f }, { 99, 18.9f }, { 104, 21 }, { 106.3f, 23.6f }, { 104.8f, 26.4f }, { 98, 27.9f }, { 88, 28.4f } }, 4), 1.0f, 1.0f, Part::body, 0.1f);
        add ({ { 91, 19.6f }, { 93, 23 }, { 100, 23.5f } }, 0.4f, 0.5f, Part::detail, 0.1f);
        add (ellipseArc ({ 95.6f, 18.7f }, 1.7f, 1.0f, MathConstants<float>::pi, MathConstants<float>::twoPi, 0.0f, 8), 0.6f, 0.9f, Part::detail, 0.4f);
        for (auto p : { Point<float> { 95.0f, 18.4f }, { 96.3f, 18.4f }, { 103.2f, 20.9f }, { 103.9f, 21.6f } }) dot (p, 0.45f, 0.9f, 1.0f);
        add (curve ({ { 105.8f, 23 }, { 108.6f, 22.6f }, { 109.6f, 23.8f }, { 108.3f, 24.6f } }, 3), 0.6f, 0.9f, Part::detail, 0.5f);
        fill ({ 97.0f, 23.5f }, 6.0f, 3.8f, 1.0f);

        // the tail (metasoma): its spine, arched over the back at rest, the sting thrown forwards over the head when struck;
        // it sways a little
        static const Point<float> rest[] { { 46, 21.5f }, { 36, 19.2f }, { 28.8f, 14 }, { 27.6f, 8.2f }, { 33, 4.3f }, { 44, 3 }, { 56.5f, 4.2f }, { 65.5f, 8 } };
        static const Point<float> thrown[] { { 46, 21.5f }, { 38, 17 }, { 35.5f, 10.2f }, { 40.5f, 5.2f }, { 52, 3.3f }, { 66, 4 }, { 80, 7.6f }, { 90, 12.5f } };
        std::vector<Point<float>> bones;
        for (int i = 0; i < 8; ++i)
        {
            const float k = (float) i / 7.0f;
            bones.push_back ({ lerp (rest[i].x, thrown[i].x, st) + 0.36f * k * std::sin (t2 * 1.3f),
                               lerp (rest[i].y, thrown[i].y, st) + 0.4f * k * std::sin (t2 * 1.3f + 0.5f) });
        }
        const auto sp = curve (bones, 10);
        const int n = (int) sp.size();
        spines[which] = sp;
        // five segments, each a capsule wider than the last, with a keel along its back and two grains
        static constexpr float cuts[] { 0.0f, 0.17f, 0.34f, 0.52f, 0.71f, 0.93f };
        for (int c = 0; c < 5; ++c)
        {
            const int a = (int) (cuts[c] * (float) (n - 1)), b = (int) (cuts[c + 1] * (float) (n - 1));
            const float wa = 2.5f + 0.32f * (float) c, tint = 0.1f + 0.1f * (float) c;
            std::vector<Point<float>> one, other, keel;
            for (int i = a; i <= b; ++i)
            {
                const auto p = sp[(size_t) i], q = sp[(size_t) jmin (n - 1, i + 1)], r = sp[(size_t) jmax (0, i - 1)];
                const float dx = q.x - r.x, dy = q.y - r.y, l = jmax (1.0e-4f, std::hypot (dx, dy));
                const float f = (float) (i - a) / (float) jmax (1, b - a), w = wa * (0.84f + 0.16f * std::sin (f * MathConstants<float>::pi));
                one.push_back ({ p.x - dy / l * w, p.y + dx / l * w });
                other.push_back ({ p.x + dy / l * w, p.y - dx / l * w });
                if (i > a + 1 && i < b - 1) keel.push_back ({ p.x + dy / l * w * 0.45f, p.y - dx / l * w * 0.45f });
            }
            const auto joint = std::vector<Point<float>> { one.back(), other.back() };
            const auto mid = sp[(size_t) ((a + b) / 2)];
            add (std::move (one), 0.95f, 1.0f, Part::tail, tint);
            add (std::move (other), 0.95f, 1.0f, Part::tail, tint);
            add (joint, 0.65f, 0.85f, Part::detail, tint);
            if (keel.size() > 1) add (std::move (keel), 0.35f, 0.5f, Part::detail, tint);
            dot (sp[(size_t) (a + (b - a) / 3)], 0.4f, 0.6f, tint + 0.2f);
            dot (sp[(size_t) (a + 2 * (b - a) / 3)], 0.4f, 0.6f, tint + 0.2f);
            fill (mid, wa * 1.1f, wa * 1.1f, 0.9f);
        }
        // the sting: a swollen bulb and a long, curved blade
        const auto end = sp[(size_t) (0.93f * (float) (n - 1))], before = sp[(size_t) (n - 4)];
        const float ang = std::atan2 (sp.back().y - before.y, sp.back().x - before.x);
        const auto bulb = end + dir (ang) * 3.4f;
        add (ellipseArc (bulb, 4.5f, 3.1f, 0.0f, MathConstants<float>::twoPi, ang, 24), 1.0f, 1.2f, Part::telson, 0.7f);
        add (ellipseArc (bulb, 2.4f, 1.4f, -2.5f, 0.5f, ang, 8), 0.4f, 0.7f, Part::detail, 0.8f);
        fill (bulb, 4.0f, 3.0f, 1.2f);
        const auto root = bulb + dir (ang) * 4.3f;
        const auto barb = root + dir (ang + 1.25f) * 8.0f;
        stingTips[which] = barb;
        add (curve ({ root, root + dir (ang + 0.35f) * 4.6f, barb }, 6), 0.95f, 1.5f, Part::sting, 1.0f);
        add (curve ({ root + dir (ang + 1.57f) * 1.1f, root + dir (ang + 0.55f) * 4.2f, barb }, 6), 0.55f, 1.2f, Part::sting, 1.0f);

        // the claws (the far one behind, dimmer), resting on the ground: arm, wrist, a heavy pincer with ridges, a fixed
        // finger and a moving one a little open, both toothed
        for (int far = 1; far >= 0; --far)
        {
            const Point<float> o { far != 0 ? -3.2f : 0.0f, far != 0 ? -2.6f : 0.0f };
            const float b = far != 0 ? 0.4f : 1.0f;
            add (curve ({ Point<float> { 100.5f, 24.6f } + o, Point<float> { 104.5f, 26.4f } + o, Point<float> { 108.5f, 28.4f } + o }, 3), 1.0f, b, Part::arm, 0.3f);
            add (curve ({ Point<float> { 108.5f, 28.4f } + o, Point<float> { 112, 29.4f } + o, Point<float> { 115.5f, 30.6f } + o }, 3), 1.05f, b, Part::arm, 0.4f);
            dot (Point<float> { 108.5f, 28.4f } + o, 0.6f, 0.8f * b, 0.6f);
            const auto palm = Point<float> { 121.8f, 31.0f } + o;
            add (ellipseArc (palm, 6.4f, 3.3f, 0.0f, MathConstants<float>::twoPi, -0.05f, 28), 1.05f, b, Part::claw, 0.55f);
            add (curve ({ Point<float> { 117, 30 } + o, Point<float> { 122, 29.1f } + o, Point<float> { 127, 29.4f } + o }, 3), 0.4f, 0.6f * b, Part::detail, 0.6f);
            add (curve ({ Point<float> { 117.5f, 32.4f } + o, Point<float> { 123, 33.2f } + o, Point<float> { 127.5f, 32.8f } + o }, 3), 0.4f, 0.6f * b, Part::detail, 0.6f);
            fill (palm, 5.8f, 3.0f, b);
            const Point<float> fixedTip = Point<float> { 141, 31.4f } + o;
            add (curve ({ Point<float> { 127.6f, 28.7f } + o, Point<float> { 133, 28.7f } + o, Point<float> { 138, 29.8f } + o, fixedTip }, 4), 0.95f, 1.15f * b, Part::finger, 0.9f);
            add (curve ({ Point<float> { 128, 30.5f } + o, Point<float> { 133, 30.6f } + o, Point<float> { 138, 30.9f } + o, fixedTip }, 3), 0.6f, b, Part::finger, 0.9f);
            for (int k = 0; k < 4; ++k) { const float x = 130.0f + 2.6f * (float) k; add (line (Point<float> { x, 30.6f } + o, Point<float> { x + 0.6f, 31.4f } + o), 0.35f, 0.6f * b, Part::detail, 0.9f); }
            // (the moving finger along the ground, a little open)
            const auto pivot = Point<float> { 128, 32.6f } + o;
            const float a = 0.13f, len = 13.0f;
            const auto tip = pivot + dir (a) * len, inner = pivot + dir (a - 0.12f) * (0.55f * len) + Point<float> { 0.0f, -0.4f };
            add (curve ({ pivot + Point<float> { 0.0f, 0.8f }, pivot + dir (a + 0.08f) * (0.55f * len) + Point<float> { 0.0f, 0.9f }, tip }, 5), 0.95f, 1.15f * b, Part::finger, 0.9f);
            add (curve ({ pivot + Point<float> { 0.0f, -0.6f }, inner, tip }, 4), 0.6f, b, Part::finger, 0.9f);
            for (int k = 1; k < 4; ++k)
            {
                const auto p = pivot + dir (a - 0.1f) * (len * (0.25f + 0.2f * (float) k)) + Point<float> { 0.0f, -0.3f };
                add (line (p, p + dir (a - 1.6f) * 0.9f), 0.35f, 0.6f * b, Part::detail, 0.9f);
            }
            if (far == 0) { pincers[2 * which] = fixedTip; pincers[2 * which + 1] = tip; }
        }

        // four legs a side, jointed, the front ones reaching forwards and the back ones backwards, stepping a little
        for (int i = 0; i < 4; ++i)
            for (int far = 1; far >= 0; --far)
            {
                const float x = 85.0f - 7.0f * (float) i + (far != 0 ? 1.5f : 0.0f), y = far != 0 ? -0.9f : 0.0f;
                const float d = 1.0f - 0.667f * (float) i, step = 0.8f * std::sin (t2 * 7.0f + 1.7f * (float) i + (far != 0 ? 1.5f : 0.0f));
                const Point<float> hip { x, 28.4f + y }, knee { x + 2.2f * d + 0.6f, 31.2f + y }, ankle { x + 4.6f * d + 0.5f * step, 33.6f + y }, foot { x + 6.6f * d + step, 35.0f + y };
                const float b = far != 0 ? 0.35f : 0.85f;
                add ({ hip, knee, ankle, foot }, 0.7f, b, Part::leg, 0.1f);
                add (line (foot, foot + Point<float> { d >= 0.0f ? 1.0f : -1.0f, -0.4f }), 0.4f, b, Part::detail, 0.3f);
                if (far == 0) dot (knee, 0.45f, 0.7f, 0.3f);
            }

        // into the tube, this one's side of the middle
        for (size_t k = s0; k < strokes.size(); ++k) for (auto& p : strokes[k].pts) p = toDesign (p, which);
        for (size_t k = d0; k < glints.size(); ++k) glints[k].p = toDesign (glints[k].p, which);
        for (size_t k = f0; k < fills.size(); ++k) fills[k].p = toDesign (fills[k].p, which);
        for (auto& p : spines[which]) p = toDesign (p, which);
        stingTips[which] = toDesign (stingTips[which], which);
        for (int k = 0; k < 2; ++k) pincers[2 * which + k] = toDesign (pincers[2 * which + k], which);
    }

    // the distance along each stroke, for what runs along it
    for (auto& k : strokes)
    {
        k.along.assign (k.pts.size(), 0.0f);
        for (size_t i = 1; i < k.pts.size(); ++i) k.along[i] = k.along[i - 1] + k.pts[i].getDistanceFrom (k.pts[i - 1]);
    }
}

Point<float> Stripe::pointAlong (const Stroke& k, float d) const
{
    d = jlimit (0.0f, k.along.back(), d);
    size_t i = 1;
    while (i + 1 < k.pts.size() && k.along[i] < d) ++i;
    const float span = jmax (1.0e-6f, k.along[i] - k.along[i - 1]), f = (d - k.along[i - 1]) / span;
    return k.pts[i - 1] + (k.pts[i] - k.pts[i - 1]) * f;
}

// a frame of the scorpion's life: its strike (on a kick: the low band jumping up), its claws, the sparks running along
// it and the motes it sheds (real: seconds; paced: the motes' time, at the tempo)
void Stripe::advanceScorpion (float real, float paced)
{
    bodyTime += real * 0.5f * pace;
    presence = envelope >= presence || ! playing ? envelope : presence * std::exp (-real / 1.5f);
    if (presence < 0.03f && envelope <= 0.002f) presence = 0.0f;           // (and out, once faint)
    // a kick (dsp/Kick.h) strikes, from wherever the tails are in their swing
    if (kicked && envelope > 0.02f)
    {
        strikeFrom = struck;
        strike = 1.0f;
    }
    kicked = false;
    // any beat (the light jumping up) brightens it
    if (beat && envelope > 0.02f) flash = jmax (flash, 0.6f * envelope);
    strike = jmax (0.0f, strike - real * 3.2f);
    flash *= std::exp (-real * 5.0f);
    // the tails snap forwards in the first fifth of a strike and settle back over the rest
    const float phase = 1.0f - strike;
    struck = strike <= 0.0f ? 0.0f : phase < 0.18f ? strikeFrom + (1.0f - strikeFrom) * std::pow (phase / 0.18f, 0.6f)
                                                   : std::pow (1.0f - (phase - 0.18f) / 0.82f, 2.0f);
    buildScorpion (struck, bodyTime);
    mapScorpions();
    if (runners.empty())
        for (int i = 0; i < 180; ++i)
            runners.push_back ({ (size_t) rng.nextInt (1 << 16), rng.nextFloat(), (0.15f + 0.35f * rng.nextFloat()) * (rng.nextBool() ? 1.0f : -1.0f), rng.nextFloat() });
    for (auto& r : runners)
    {
        r.at += r.speed * real * (1.0f + 2.0f * flash + 3.0f * struck);
        r.at -= std::floor (r.at);
    }
    // shedding: ~800 motes a second at full brightness, more on a beat, fewer as it fades; none in silence; from the
    // outlines (not the details inside), each born just clear of the scorpion. Each mote keeps the brightness it was born
    // with (it does not dim and brighten with every hit)
    moteDue += real * 800.0f * presence * (0.8f + 0.6f * flash);
    for (; moteDue >= 1.0f; moteDue -= 1.0f)
    {
        const Stroke* pick = nullptr;
        for (int tries = 0; tries < 16 && pick == nullptr; ++tries)
            if (const auto& k = strokes[(size_t) rng.nextInt ((int) strokes.size())]; k.part != Part::detail) pick = &k;
        if (pick == nullptr) continue;
        const auto& k = *pick;
        auto p = pointAlong (k, rng.nextFloat() * k.along.back());
        for (int step = 0; step < 8 && onScorpion (p.x, p.y) > 0.05f; ++step) p += awayFromScorpion (p.x, p.y) * 0.8f;
        const float a = MathConstants<float>::twoPi * rng.nextFloat(), j = uniform (rng, 3.0f, 11.0f), c = rng.nextFloat();
        const Rgb col = c < 0.42f ? greenLight : c < 0.66f ? tealLight : c < 0.86f ? mintLight : c < 0.94f ? aquaLight : coreLight;
        Mote m { p.x, p.y, j * std::cos (a), 0.8f * j * std::sin (a), 0.0f, uniform (rng, 0.15f, 0.6f),
                 uniform (rng, 0.3f, 0.46f) * (1.0f + 0.5f * flash), uniform (rng, 1.2f, 3.0f), uniform (rng, 0.0f, 6.3f), col.r, col.g, col.b,
                 jmax (presence, envelope), p.x + uniform (rng, -30.0f, 30.0f) < scorpionX ? -1.0f : 1.0f };
        if (motes.size() < 16000) motes.push_back (m);
    }
    // the motes: drifting off it, then streaming outwards (the ORION style's streamers' law); near an end the black
    // hole past it draws them in, faster the closer, funnelled to its height (the ORION style's pull); off the end gone
    for (auto& m : motes)
    {
        m.age += paced;
        if (m.age < m.rise)
        {
            m.vx *= std::exp (-paced * 3.0f);
            m.vy *= std::exp (-paced * 3.0f);
        }
        else
        {
            if (m.expand > 0.0f)
            {
                m.vx = m.side * (18.0f + m.expand * std::abs (m.x - scorpionX));
                m.vy = (m.vy + 4.0f * std::sin (m.twinkle + bodyTime * 1.4f) * paced) * std::exp (-paced * 2.0f);
            }
            const float hx = m.side < 0.0f ? holeX[0] : holeX[1], dx = hx - m.x, d = std::abs (dx);
            if (d < holeReach)
            {
                const float pull = 400000.0f / (d * d + 400.0f);
                m.vx += (dx > 0.0f ? 1.0f : -1.0f) * pull * paced;
                m.vy += (holeY - m.y) * (1.0f - d / holeReach) * holeFunnel * paced;
                m.vy *= std::exp (-paced / 0.15f);
                m.expand = 0.0f;
            }
        }
        // (round the scorpions, not across them: pushed off their lines and bodies, harder the closer)
        if (const float on = onScorpion (m.x, m.y); on > 0.0f)
        {
            const auto away = awayFromScorpion (m.x, m.y);
            m.vx += away.x * 900.0f * on * paced;
            m.vy += away.y * 900.0f * on * paced;
            m.x += away.x * 6.0f * on * paced;
            m.y += away.y * 6.0f * on * paced;
        }
        m.x += m.vx * paced;
        m.y += m.vy * paced;
    }
    motes.erase (std::remove_if (motes.begin(), motes.end(), [] (const Mote& m)
                                 { return m.x < left || m.x > left + width || m.y < top - 2.0f || m.y > top + height + 2.0f; }),
                 motes.end());
}

// where the scorpions are, on a grid of design pixels: 1 on their lines and inside their bodies (the glow that fills
// them), falling to 0 ~2.5 px out
void Stripe::mapScorpions()
{
    scorpionMap.assign ((size_t) (mapW * mapH), 0.0f);
    auto mark = [this] (float x, float y, float radius)
    {
        const int i0 = jmax (0, (int) (x - radius - mapLeft)), i1 = jmin (mapW - 1, (int) (x + radius - mapLeft) + 1);
        const int j0 = jmax (0, (int) (y - radius - mapTop)), j1 = jmin (mapH - 1, (int) (y + radius - mapTop) + 1);
        for (int j = j0; j <= j1; ++j)
            for (int i = i0; i <= i1; ++i)
            {
                const float dx = (float) i + 0.5f + mapLeft - x, dy = (float) j + 0.5f + mapTop - y;
                const float v = 1.0f - std::sqrt (dx * dx + dy * dy) / radius;
                float& c = scorpionMap[(size_t) (j * mapW + i)];
                c = jmax (c, jmin (1.0f, 1.6f * v));
            }
    };
    for (auto& k : strokes)
        for (size_t i = 0; i < k.pts.size(); ++i)
        {
            mark (k.pts[i].x, k.pts[i].y, 2.5f);
            if (i > 0 && k.pts[i].getDistanceFrom (k.pts[i - 1]) > 1.5f) mark ((k.pts[i].x + k.pts[i - 1].x) * 0.5f, (k.pts[i].y + k.pts[i - 1].y) * 0.5f, 2.5f);
        }
    for (auto& f : fills) mark (f.p.x, f.p.y, 0.8f * jmin (f.rx, f.ry) * scorpionScale + 1.5f);
}

float Stripe::onScorpion (float x, float y) const
{
    const int i = (int) (x - mapLeft), j = (int) (y - mapTop);
    if (scorpionMap.empty() || i < 0 || i >= mapW || j < 0 || j >= mapH) return 0.0f;
    return scorpionMap[(size_t) (j * mapW + i)];
}

// the way off the scorpion from (x, y): down its slope (unit length; outwards from the middle where it is flat)
Point<float> Stripe::awayFromScorpion (float x, float y) const
{
    const float gx = onScorpion (x - 1.0f, y) - onScorpion (x + 1.0f, y), gy = onScorpion (x, y - 1.0f) - onScorpion (x, y + 1.0f);
    const float l = std::hypot (gx, gy);
    if (l < 1.0e-4f) return { x < scorpionX ? -1.0f : 1.0f, y < scorpionY ? -0.3f : 0.3f };
    return { gx / l, gy / l };
}

// one stroke as neon: the distance from each pixel to it, lit in three layers (a wide halo, a glow, a thin core; green
// on the body, cyan-mint towards the tips), the outline trembling a little like live current
void Stripe::strokeNeon (const Stroke& k, float level, float s)
{
    if (k.pts.size() < 2 || level <= 0.002f) return;
    const float w = k.width * scorpionScale, wobble = k.part == Part::detail ? 0.08f * scorpionScale : 0.16f * scorpionScale;
    std::vector<Point<float>> px (k.pts.size());
    float x0 = 1.0e9f, x1 = -1.0e9f, y0 = 1.0e9f, y1 = -1.0e9f;
    for (size_t i = 0; i < k.pts.size(); ++i)
    {
        const float jig = wobble * std::sin (bodyTime * 46.0f + 1.9f * (float) i + k.along.back());
        px[i] = { (k.pts[i].x + jig - left) * s, (k.pts[i].y - jig - top) * s };
        x0 = jmin (x0, px[i].x); x1 = jmax (x1, px[i].x); y0 = jmin (y0, px[i].y); y1 = jmax (y1, px[i].y);
    }
    const float halo = 1.3f * w * s, glowW = 0.55f * w * s, core = jmax (0.25f, 0.19f * w * s), edge = jmax (0.4f, 0.3f * s);
    const float reach = halo + 3.0f * edge;
    const int i0 = jmax (0, (int) (x0 - reach)), i1 = jmin (bufW - 1, (int) (x1 + reach) + 1);
    const int j0 = jmax (0, (int) (y0 - reach)), j1 = jmin (bufH - 1, (int) (y1 + reach) + 1);
    auto mix = [] (const Rgb& a, const Rgb& b, float f) { return Rgb { a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f }; };
    const Rgb haloC = mix (tealLight, aquaLight, 0.6f * k.tint), glowC = mix (greenLight, aquaLight, k.tint), coreC = mix (coreLight, tipLight, k.tint);
    auto lit = [edge] (float d, float half) { return d <= half ? 1.0f : std::exp (-(d - half) / edge); };
    // (the squared distance to the nearest of its segments, each segment visiting only the pixels near it)
    const int bw = i1 - i0 + 1, bh = j1 - j0 + 1;
    if (bw <= 0 || bh <= 0) return;
    nearest.assign ((size_t) (bw * bh), reach * reach);
    for (size_t n = 1; n < px.size(); ++n)
    {
        const float ax = px[n - 1].x, ay = px[n - 1].y, dx = px[n].x - ax, dy = px[n].y - ay, l2 = dx * dx + dy * dy;
        const int si0 = jmax (i0, (int) (jmin (ax, px[n].x) - reach)), si1 = jmin (i1, (int) (jmax (ax, px[n].x) + reach) + 1);
        const int sj0 = jmax (j0, (int) (jmin (ay, px[n].y) - reach)), sj1 = jmin (j1, (int) (jmax (ay, px[n].y) + reach) + 1);
        for (int j = sj0; j <= sj1; ++j)
        {
            float* row = nearest.data() + (size_t) ((j - j0) * bw + (si0 - i0));
            const float py = (float) j + 0.5f;
            for (int i = si0; i <= si1; ++i, ++row)
            {
                const float qx = (float) i + 0.5f;
                const float t = l2 > 1.0e-6f ? jlimit (0.0f, 1.0f, ((qx - ax) * dx + (py - ay) * dy) / l2) : 0.0f;
                const float ex = ax + t * dx - qx, ey = ay + t * dy - py;
                *row = jmin (*row, ex * ex + ey * ey);
            }
        }
    }
    for (int j = j0; j <= j1; ++j)
    {
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
        const float* closest = nearest.data() + (size_t) ((j - j0) * bw);
        for (int i = 0; i < bw; ++i, row += 3)
        {
            const float d2 = closest[i];
            if (d2 >= reach * reach) continue;
            const float d = std::sqrt (d2);
            const float a = 0.12f * level * lit (d, halo), b = 0.4f * level * lit (d, glowW), c = jmin (1.0f, 0.7f * level) * lit (d, core);
            row[0] += a * haloC.r + b * glowC.r + c * coreC.r;
            row[1] += a * haloC.g + b * glowC.g + c * coreC.g;
            row[2] += a * haloC.b + b * glowC.b + c * coreC.b;
        }
    }
}

// the scorpion's light: a haze around it, its motes, a faint glow filling its body, its outline, sparks and glints, all
// in a layer that blooms (three widths of blur added under it, as neon in a photograph), then added to the light
void Stripe::renderScorpion (float s)
{
    if (strokes.empty()) buildScorpion (0.0f, bodyTime);
    const float e = presence;
    // (faintly lit in silence; full with the music, fading over ~1.5 s after it; a beat brightens them, a strike more)
    const float lv = 0.22f + 0.45f * e + 0.3f * flash + 0.25f * struck * e;
    splatGlow (scorpionX, scorpionY, 90.0f, 14.0f, (0.06f + 0.12f * e) * (0.9f + 0.1f * std::sin (bodyTime * 1.6f)), deepLight.r, deepLight.g, deepLight.b, s);
    // the motes: thinner and drawn out near the black holes (the ORION style's); each as bright as the music was when
    // it was born (stopped, no brighter than the light, so they fade with it), fading where they cross a scorpion
    for (auto& m : motes)
    {
        const float k = jmax (0.0f, 1.0f - jmin (m.x - holeX[0], holeX[1] - m.x) / holeReach), thin = 1.0f - 0.55f * k;
        const float a = jmin (1.0f, m.age * 4.0f) * 0.62f * (0.75f + 0.25f * std::sin (m.twinkle + bodyTime * 10.0f))
                        * (playing ? m.level : jmin (m.level, envelope)) * (1.0f - 0.9f * onScorpion (m.x, m.y));
        if (a < 0.01f) continue;
        // one glow each, drawn out behind it along its way (its trail); a long trail as a line
        const float trail = m.age >= m.rise ? 0.04f + 0.08f * k : 0.0f, tx = m.vx * trail, ty = m.vy * trail, len = std::hypot (tx, ty);
        if (len > 4.0f) splatLine (m.x - tx, m.y - ty, m.x, m.y, 0.7f * m.size * thin, 0.8f * a, m.r, m.g, m.b, s);
        else splatGlow (m.x - 0.4f * tx, m.y - 0.4f * ty, 0.45f * m.size + 0.4f * len, 0.45f * m.size * thin, a, m.r, m.g, m.b, s);
    }
    // the scorpion itself in a layer of its own, to bloom
    layer.assign (light.size(), 0.0f);
    canvas = layer.data();
    for (auto& f : fills)
        splatGlow (f.p.x, f.p.y, 0.6f * f.rx * scorpionScale, 0.6f * f.ry * scorpionScale, 0.07f * f.level * lv, tealLight.r, tealLight.g, tealLight.b, s);
    for (auto& k : strokes)
    {
        const bool tip = k.part == Part::sting || k.part == Part::telson;
        strokeNeon (k, k.bright * lv * (tip ? 1.0f + 1.0f * flash + 1.2f * struck : 1.0f), s);
    }
    for (auto& d : glints)
    {
        const float rr = 0.45f * d.r * scorpionScale + 0.3f;
        const Rgb c { coreLight.r + (tipLight.r - coreLight.r) * d.tint, coreLight.g + (tipLight.g - coreLight.g) * d.tint, coreLight.b + (tipLight.b - coreLight.b) * d.tint };
        splatGlow (d.p.x, d.p.y, rr, rr, d.level * lv, c.r, c.g, c.b, s);
    }
    // the claws' tips glow; the stings' barbs flare on a strike
    for (auto& p : pincers)
    {
        const float rr = 0.35f * scorpionScale * 4.5f;
        splatGlow (p.x, p.y, rr, rr, 0.4f * lv, aquaLight.r, aquaLight.g, aquaLight.b, s);
    }
    for (int which = 0; which < 2; ++which)
    {
        const float rr = 0.35f * (3.0f + 9.0f * jmax (flash, struck));
        splatGlow (stingTips[which].x, stingTips[which].y, rr, rr, 0.55f + 0.4f * jmax (flash, struck), tipLight.r, tipLight.g, tipLight.b, s);
        // on a strike, a pulse of light runs up the tail from the body to the sting
        const auto& sp = spines[which];
        if (struck > 0.02f && ! sp.empty())
        {
            const float u = jlimit (0.0f, 1.0f, (1.0f - strike) / 0.35f);
            const auto p = sp[(size_t) (u * (float) (sp.size() - 1))];
            splatGlow (p.x, p.y, 2.2f, 2.2f, 0.8f * (1.0f - u) * lv, aquaLight.r, aquaLight.g, aquaLight.b, s);
        }
    }
    // sparks running along the outline (not its details)
    std::vector<size_t> outline;
    for (size_t i = 0; i < strokes.size(); ++i) if (strokes[i].part != Part::detail) outline.push_back (i);
    for (auto& r : runners)
    {
        const auto& k = strokes[outline[r.stroke % outline.size()]];
        const auto p = pointAlong (k, r.at * k.along.back());
        const float rr = 0.3f * scorpionScale * (0.9f + 1.1f * r.bright);
        const Rgb& c = r.bright > 0.75f ? coreLight : k.tint > 0.5f ? aquaLight : mintLight;
        splatGlow (p.x, p.y, rr, rr, (0.35f + 0.5f * r.bright) * lv * k.bright, c.r, c.g, c.b, s);
    }
    canvas = light.data();
    // the bloom, over the scorpion and the reach of its widest blur only: the layer no brighter than white (as neon
    // photographed), blurred at two widths and added under it
    float lo = 1.0e9f, hi = -1.0e9f;
    for (auto& k : strokes) for (auto& p : k.pts) { lo = jmin (lo, p.x); hi = jmax (hi, p.x); }
    const int x0 = jmax (0, (int) ((lo - 30.0f - left) * s)), x1 = jmin (bufW, (int) ((hi + 30.0f - left) * s) + 1), rw = x1 - x0;
    if (rw <= 0) return;
    region.resize ((size_t) (rw * bufH * 3));
    for (int j = 0; j < bufH; ++j)
        for (int i = 0; i < rw * 3; ++i) region[(size_t) (j * rw * 3 + i)] = jmin (1.0f, layer[(size_t) ((j * bufW + x0) * 3 + i)]);
    auto addBack = [&] (const std::vector<float>& from, float share)
    {
        for (int j = 0; j < bufH; ++j)
        {
            float* o = light.data() + (size_t) ((j * bufW + x0) * 3);
            const float* f = from.data() + (size_t) (j * rw * 3);
            for (int i = 0; i < rw * 3; ++i) o[i] += share * f[i];
        }
    };
    const int hw = (rw + 1) / 2, hh = (bufH + 1) / 2;
    halfRes.assign ((size_t) (hw * hh * 3), 0.0f);
    for (int j = 0; j < bufH; ++j)
        for (int i = 0; i < rw; ++i)
            for (int c = 0; c < 3; ++c) halfRes[(size_t) (((j / 2) * hw + i / 2) * 3 + c)] += 0.25f * region[(size_t) ((j * rw + i) * 3 + c)];
    // (the wide and the middle blur at half the resolution, added up there and brought back up once; the narrowest is
    // left to the lines' own soft edges)
    blurRgb (halfRes, blurred, blurTmp, hw, hh, 7.3f * s * 0.5f);
    blurRgb (halfRes, midBlur, blurTmp, hw, hh, 2.3f * s * 0.5f);
    for (size_t k = 0; k < blurred.size(); ++k) blurred[k] = 0.5f * blurred[k] + 0.4f * midBlur[k];
    for (int j = 0; j < bufH; ++j)
    {
        const float fy = jlimit (0.0f, (float) (hh - 1), ((float) j + 0.5f) * 0.5f - 0.5f);
        const int ya = (int) fy, yb = jmin (hh - 1, ya + 1);
        const float wy = fy - (float) ya;
        float* o = light.data() + (size_t) ((j * bufW + x0) * 3);
        for (int i = 0; i < rw; ++i, o += 3)
        {
            const float fx = jlimit (0.0f, (float) (hw - 1), ((float) i + 0.5f) * 0.5f - 0.5f);
            const int xa = (int) fx, xb = jmin (hw - 1, xa + 1);
            const float wx = fx - (float) xa;
            const float* p00 = blurred.data() + (size_t) ((ya * hw + xa) * 3), *p10 = blurred.data() + (size_t) ((ya * hw + xb) * 3);
            const float* p01 = blurred.data() + (size_t) ((yb * hw + xa) * 3), *p11 = blurred.data() + (size_t) ((yb * hw + xb) * 3);
            for (int c = 0; c < 3; ++c)
                o[c] += (p00[c] * (1.0f - wx) + p10[c] * wx) * (1.0f - wy) + (p01[c] * (1.0f - wx) + p11[c] * wx) * wy;
        }
    }
    addBack (region, 1.0f);
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
    // (separable: the weights across computed once)
    float gx[1024];
    const int n = jmin (1024, i1 - i0 + 1);
    for (int i = 0; i < n; ++i) { const float dx = ((float) (i0 + i) + 0.5f - cx) / ax; gx[i] = std::exp (-dx * dx); }
    for (int j = j0; j <= j1; ++j)
    {
        const float dy = ((float) j + 0.5f - cy) / ay, gy = level * std::exp (-dy * dy);
        float* row = canvas + (size_t) (j * bufW + i0) * 3;
        for (int i = 0; i < n; ++i, row += 3)
        {
            const float v = gy * gx[i];
            row[0] += v * r; row[1] += v * g; row[2] += v * b;
        }
    }
}

void Stripe::paint (Graphics& g)
{
    const float s = theme::pixelScale (g, *this);
    const int w = jmax (1, roundToInt (width * s)), h = jmax (1, roundToInt (height * s));
    // (dark: only what is under it; ours: the scorpion stays faintly lit while the animation is on)
    const bool dark = envelope <= 0.0f && ! (style == Style::scorpius && animated);      // (their glow fading included)
    if (dark && background.isValid() && w == bufW && h == bufH && s == glowScale)
        g.drawImage (background, Rectangle<float> (0.0f, 0.0f, width, height));
    else
    {
        render (w, h, s);
        g.drawImage (image, Rectangle<float> (0.0f, 0.0f, width, height));
    }
}
}
