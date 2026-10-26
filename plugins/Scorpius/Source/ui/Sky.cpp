#include "Sky.h"
#include <cmath>

using namespace juce;

namespace sc
{
namespace
{
// Scorpius's main stars: right ascension and declination (degrees), magnitude and colour (B stars blue-white, the
// red supergiant Antares, the orange giants epsilon and zeta2, the yellow-white theta)
struct Catalogue { const char* name; float ra, dec, mag, r, g, b; };
constexpr Catalogue catalogue[] {
    { "beta",    241.36f, -19.81f, 2.6f, 0.78f, 0.86f, 1.00f },
    { "delta",   240.08f, -22.62f, 2.3f, 0.78f, 0.86f, 1.00f },
    { "pi",      239.71f, -26.11f, 2.9f, 0.78f, 0.86f, 1.00f },
    { "rho",     239.22f, -29.21f, 3.9f, 0.78f, 0.86f, 1.00f },
    { "sigma",   245.30f, -25.59f, 2.9f, 0.78f, 0.86f, 1.00f },
    { "antares", 247.35f, -26.43f, 1.0f, 1.00f, 0.32f, 0.16f },
    { "tau",     248.97f, -28.22f, 2.8f, 0.78f, 0.86f, 1.00f },
    { "epsilon", 252.54f, -34.29f, 2.3f, 1.00f, 0.80f, 0.55f },
    { "mu1",     252.97f, -38.05f, 3.0f, 0.78f, 0.86f, 1.00f },
    { "zeta2",   253.65f, -42.36f, 3.6f, 1.00f, 0.80f, 0.55f },
    { "eta",     258.04f, -43.24f, 3.3f, 0.95f, 0.95f, 1.00f },
    { "theta",   264.33f, -43.00f, 1.9f, 1.00f, 0.97f, 0.88f },
    { "iota1",   266.90f, -40.13f, 3.0f, 1.00f, 0.97f, 0.88f },
    { "kappa",   265.62f, -39.03f, 2.4f, 0.78f, 0.86f, 1.00f },
    { "lambda",  263.40f, -37.10f, 1.6f, 0.78f, 0.86f, 1.00f },
    { "upsilon", 262.69f, -37.30f, 2.7f, 0.78f, 0.86f, 1.00f } };
constexpr int antares = 5;
// the lines of the figure, and the body from the head (delta) to the stinger (lambda, upsilon)
constexpr int lines[][2] { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 }, { 8, 9 },
                           { 9, 10 }, { 10, 11 }, { 11, 12 }, { 12, 13 }, { 13, 14 }, { 14, 15 } };
constexpr int bodyStars[] { 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
constexpr float stripeWidth = 868.0f;

// the sky into the stripe (design px from its corner): east to the left, as seen
Point<float> place (float ra, float dec)
{
    return { 800.0f - (ra - 239.2f) / 27.7f * 730.0f, 3.5f + (-19.8f - dec) / 23.5f * 29.0f };
}

float noise2 (int x, int y)
{
    uint32 h = (uint32) x * 374761393u + (uint32) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

float smoothNoise (float x, float y)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = noise2 (xi, yi), b = noise2 (xi + 1, yi), c = noise2 (xi, yi + 1), d = noise2 (xi + 1, yi + 1);
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

float decay (float v, float dt, float tau) { return v * std::exp (-dt / tau); }

// exp (-x) for x >= 0 from a table (the light is drawn per pixel, many times a frame)
float expNeg (float x)
{
    static constexpr int size = 2048;
    static constexpr float range = 16.0f;
    static const auto table = []
    {
        std::array<float, size + 2> t {};
        for (int i = 0; i < size + 2; ++i) t[(size_t) i] = std::exp (-range * (float) i / (float) size);
        return t;
    }();
    if (x >= range) return 0.0f;
    const float f = jmax (0.0f, x) * ((float) size / range);
    const int i = (int) f;
    return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * (f - (float) i);
}

// the colours of sparks and glints
constexpr float bright[][3] { { 1.0f, 0.85f, 0.35f }, { 1.0f, 0.2f, 0.65f }, { 0.2f, 0.85f, 1.0f }, { 0.4f, 1.0f, 0.7f },
                              { 1.0f, 0.25f, 0.12f }, { 0.65f, 0.35f, 1.0f }, { 1.0f, 1.0f, 1.0f } };
}

Sky::Sky()
{
    for (auto& c : catalogue)
    {
        const auto p = place (c.ra, c.dec);
        stars.push_back ({ p.x, p.y, std::pow (10.0f, -0.4f * (c.mag - 1.0f)), c.r, c.g, c.b,
                           MathConstants<float>::twoPi * rng.nextFloat(), 1.5f + 3.0f * rng.nextFloat() });
    }
    heartX = stars[(size_t) antares].x;
    for (int i : bodyStars) body.push_back ({ stars[(size_t) i].x, stars[(size_t) i].y });
    bodyLength.push_back (0.0f);
    for (size_t i = 1; i < body.size(); ++i) bodyLength.push_back (bodyLength.back() + body[i].getDistanceFrom (body[i - 1]));
    heartAlong = bodyLength[2];                                    // (Antares is the body's third star)
    // dust stars, thicker towards the tail (where the Milky Way runs)
    for (int i = 0; i < 560; ++i)
    {
        const float x = stripeWidth * std::pow (rng.nextFloat(), 1.4f);
        const float kind = rng.nextFloat();                 // most white, some blue, some warm
        const float r = kind < 0.2f ? 0.7f : 1.0f, g = kind < 0.2f ? 0.8f : kind > 0.8f ? 0.75f : 0.95f, b = kind > 0.8f ? 0.55f : 1.0f;
        dust.push_back ({ x, 1.5f + 33.0f * rng.nextFloat(), 0.2f + 0.8f * std::pow (rng.nextFloat(), 3.0f), r, g, b,
                          MathConstants<float>::twoPi * rng.nextFloat(), 3.0f + 9.0f * rng.nextFloat() });
    }
}

Sky::Rgb Sky::palette (float u)
{
    // Antares's crimson, magenta, violet, blue, cyan, mint, gold
    static constexpr float at[] { 0.0f, 0.17f, 0.36f, 0.55f, 0.73f, 0.88f, 1.0f };
    static constexpr float c[][3] { { 1.0f, 0.16f, 0.1f }, { 1.0f, 0.12f, 0.55f }, { 0.62f, 0.22f, 1.0f }, { 0.22f, 0.42f, 1.0f },
                                    { 0.12f, 0.85f, 1.0f }, { 0.3f, 1.0f, 0.65f }, { 1.0f, 0.82f, 0.3f } };
    u = jlimit (0.0f, 1.0f, u);
    for (int i = 1; i < 7; ++i)
        if (u <= at[i])
        {
            const float f = (u - at[i - 1]) / (at[i] - at[i - 1]);
            return { c[i - 1][0] + (c[i][0] - c[i - 1][0]) * f, c[i - 1][1] + (c[i][1] - c[i - 1][1]) * f, c[i - 1][2] + (c[i][2] - c[i - 1][2]) * f };
        }
    return { c[6][0], c[6][1], c[6][2] };
}

float Sky::bandPosition (float x) const
{
    return x < heartX ? (heartX - x) / heartX : (x - heartX) / (stripeWidth - heartX);
}

// (between bands, and softened over the neighbours so that a loud band does not end in a hard edge)
float Sky::levelAt (float u) const
{
    auto at = [this] (float v)
    {
        const float f = jlimit (0.0f, (float) (numBands - 1), v * (float) (numBands - 1));
        const int i = jmin (numBands - 2, (int) f);
        return band[(size_t) i].level + (band[(size_t) i + 1].level - band[(size_t) i].level) * (f - (float) i);
    };
    const float step = 1.0f / (float) (numBands - 1);
    return 0.25f * at (u - step) + 0.5f * at (u) + 0.25f * at (u + step);
}

float Sky::flashAt (float u) const
{
    auto at = [this] (float v)
    {
        const float f = jlimit (0.0f, (float) (numBands - 1), v * (float) (numBands - 1));
        const int i = jmin (numBands - 2, (int) f);
        return band[(size_t) i].flash + (band[(size_t) i + 1].flash - band[(size_t) i].flash) * (f - (float) i);
    };
    const float step = 1.0f / (float) (numBands - 1);
    return 0.25f * at (u - step) + 0.5f * at (u) + 0.25f * at (u + step);
}

Point<float> Sky::along (float d) const
{
    for (size_t i = 1; i < body.size(); ++i)
        if (d <= bodyLength[i])
        {
            const float u = (d - bodyLength[i - 1]) / jmax (0.001f, bodyLength[i] - bodyLength[i - 1]);
            return body[i - 1] + (body[i] - body[i - 1]) * u;
        }
    return body.back();
}

//==============================================================================
// listening: each band's level against its own recent loudest (so quiet mixes play too, but a band far below the
// loudest stays low), its flash when it jumps over its average, and the hits of three groups of bands
void Sky::listen (float dt, const float* bands, bool beat)
{
    float loudest = -120.0f;
    for (int b = 0; b < numBands; ++b)
    {
        auto& x = band[(size_t) b];
        x.db = bands != nullptr && bands[b] > 1.0e-6f ? 20.0f * std::log10 (bands[b]) : -120.0f;
        x.ref = jmax (x.db, x.ref - 4.0f * dt, -70.0f);
        loudest = jmax (loudest, x.ref);
    }
    float sum = 0.0f;
    for (auto& x : band)
    {
        const float ref = jmax (x.ref, loudest - 24.0f);
        const float target = std::pow (jlimit (0.0f, 1.0f, 1.0f + (x.db - ref) / 30.0f), 1.7f);
        x.level = target > x.level ? target : x.level + (target - x.level) * (1.0f - std::exp (-dt / 0.18f));
        x.flash = x.db - x.avg > 5.0f && x.db > loudest - 30.0f ? 1.0f : decay (x.flash, dt, 0.15f);
        x.avg += (x.db - x.avg) * (1.0f - std::exp (-dt / 0.3f));
        sum += x.level;
    }
    loudness = sum / (float) numBands;

    auto detect = [&] (Hits& h, auto&& hit)
    {
        float e = 0.0f;
        if (bands != nullptr) for (int b = h.from; b < h.to; ++b) e += bands[b] * bands[b];
        h.db = 10.0f * std::log10 (e + 1.0e-12f);
        h.hold -= dt;
        const float over = h.db - h.avg;
        if (over > 3.0f && h.db - h.prev > 1.5f && h.hold <= 0.0f && h.db > -70.0f && h.db > loudest - 30.0f)
        {
            h.hold = 0.1f;
            hit (jlimit (0.3f, 1.0f, over / 9.0f));
        }
        h.avg += (h.db - h.avg) * (1.0f - std::exp (-dt / 0.35f));
        h.prev = h.db;
        float lv = 0.0f;
        for (int b = h.from; b < h.to; ++b) lv += band[(size_t) b].level;
        h.level = lv / (float) (h.to - h.from);
    };
    detect (kicks, [this] (float s) { kick (s); });
    detect (snares, [this] (float s) { snare (s); });
    detect (hats, [this] (float s) { hat (s); });
    if (beat && kicks.hold <= 0.0f) { kicks.hold = 0.1f; kick (1.0f); }
}

void Sky::kick (float s)
{
    heart = jmax (heart, s);
    if (rings.size() < 6) rings.push_back ({ 0.0f, 0.75f, s });
    if (pulses.size() < 6) pulses.push_back ({ 0.0f, 0.9f, s });
}

void Sky::snare (float s)
{
    const int n = s > 0.6f ? 2 : 1;
    for (int i = 0; i < n && comets.size() < 24; ++i)
        comets.push_back ({ -45.0f * (float) i, 420.0f + 260.0f * rng.nextFloat(), s });
}

void Sky::hat (float s)
{
    const int n = 3 + (int) (10.0f * s);
    for (int i = 0; i < n && glints.size() < 240; ++i)
    {
        const auto& c = bright[rng.nextInt (7)];
        glints.push_back ({ rng.nextInt ((int) dust.size()), 0.0f, 0.15f + 0.2f * rng.nextFloat(), { c[0], c[1], c[2] } });
    }
}

void Sky::advance (float dt, float brightness, bool beat, const float* bands)
{
    time += dt;
    listen (dt, bands, beat);
    heart = decay (heart, dt, 0.25f);
    sting = decay (sting, dt, 0.2f);

    // venom also trickles with the mids, glints with the highs
    cometDebt += dt * 1.5f * brightness * snares.level;
    if (cometDebt >= 1.0f) { cometDebt -= 1.0f; snare (0.5f); }
    glintDebt += dt * 40.0f * brightness * hats.level;
    while (glintDebt >= 1.0f && glints.size() < 240)
    {
        glintDebt -= 1.0f;
        const auto& c = bright[rng.nextInt (7)];
        glints.push_back ({ rng.nextInt ((int) dust.size()), 0.0f, 0.15f + 0.2f * rng.nextFloat(), { c[0], c[1], c[2] } });
    }
    glintDebt = jmin (glintDebt, 1.0f);

    for (auto& r : rings) r.age += dt;
    for (auto& p : pulses) p.age += dt;
    for (auto& g : glints) g.age += dt;
    rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& r) { return r.age > r.life; }), rings.end());
    pulses.erase (std::remove_if (pulses.begin(), pulses.end(), [] (const Pulse& p) { return p.age > p.life; }), pulses.end());
    glints.erase (std::remove_if (glints.begin(), glints.end(), [] (const Glint& g) { return g.age > g.life; }), glints.end());

    // venom runs from the head along the body; at the stinger it bursts into sparks
    const float total = bodyLength.back();
    const auto tip = body.back();
    for (auto& c : comets)
    {
        c.along += c.speed * dt;
        if (c.along >= total)
        {
            sting = jmax (sting, c.strength);
            const int n = 10 + (int) (22.0f * c.strength);
            for (int k = 0; k < n && sparks.size() < 600; ++k)
            {
                const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = 40.0f + 130.0f * rng.nextFloat();
                const auto& col = bright[rng.nextInt (7)];
                sparks.push_back ({ tip.x, tip.y, v * std::cos (a), 0.55f * v * std::sin (a), 0.0f,
                                    0.4f + 0.6f * rng.nextFloat(), { col[0], col[1], col[2] } });
            }
        }
    }
    comets.erase (std::remove_if (comets.begin(), comets.end(), [total] (const Comet& c) { return c.along >= total; }), comets.end());
    const float drag = std::exp (-2.5f * dt);
    for (auto& s : sparks)
    {
        s.age += dt;
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        s.vx *= drag;
        s.vy = s.vy * drag + 30.0f * dt;               // falling a little
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.age > s.life; }), sparks.end());
}

//==============================================================================
void Sky::addDot (std::vector<float>& light, int w, int h, float s, float x, float y, float radius, float halo, float haloShare,
                  Rgb c, float level) const
{
    if (level <= 0.002f) return;
    const float cx = x * s, cy = y * s, rc = jmax (0.35f, radius * s), rh = halo * s;
    const float reach = rc + 3.0f * rh;
    const int i0 = jmax (0, (int) (cx - reach)), i1 = jmin (w - 1, (int) (cx + reach) + 1);
    const int j0 = jmax (0, (int) (cy - reach)), j1 = jmin (h - 1, (int) (cy + reach) + 1);
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
            const float d = std::sqrt (((float) i + 0.5f - cx) * ((float) i + 0.5f - cx) + ((float) j + 0.5f - cy) * ((float) j + 0.5f - cy));
            const float core = d <= rc ? 1.0f : expNeg ((d - rc) / 0.45f);
            const float glow = expNeg (d / jmax (0.3f, rh));
            const float v = level * ((1.0f - haloShare) * core + haloShare * glow);
            float* px = light.data() + (size_t) ((j * w + i) * 3);
            px[0] += v * c.r; px[1] += v * c.g; px[2] += v * c.b;
        }
}

// a point of light about a design px wide, spread over the pixels around it (no stair-steps along a trail)
void Sky::addPoint (std::vector<float>& light, int w, int h, float s, float x, float y, Rgb c, float level) const
{
    if (level <= 0.002f) return;
    const float cx = x * s - 0.5f, cy = y * s - 0.5f, k = 1.0f / (2.0f * 0.45f * 0.45f * s * s);
    const int i0 = jmax (0, (int) std::floor (cx - 1.5f * s)), j0 = jmax (0, (int) std::floor (cy - 1.5f * s));
    const int i1 = jmin (w - 1, (int) std::ceil (cx + 1.5f * s)), j1 = jmin (h - 1, (int) std::ceil (cy + 1.5f * s));
    if (i0 > i1 || j0 > j1) return;
    float wx[16];                                   // (separable)
    const int nx = jmin (16, i1 - i0 + 1);
    for (int i = 0; i < nx; ++i) wx[i] = expNeg (((float) (i0 + i) - cx) * ((float) (i0 + i) - cx) * k);
    for (int j = j0; j <= j1; ++j)
    {
        const float v = 0.8f * level * expNeg (((float) j - cy) * ((float) j - cy) * k);
        float* px = light.data() + (size_t) ((j * w + i0) * 3);
        for (int i = 0; i < nx; ++i, px += 3)
        {
            px[0] += v * wx[i] * c.r; px[1] += v * wx[i] * c.g; px[2] += v * wx[i] * c.b;
        }
    }
}

// a line of points 0.5 design px apart (about as bright as its level), its level going from atA to atB
void Sky::addStreak (std::vector<float>& light, int w, int h, float s, Point<float> a, Point<float> b, Rgb c, float atA, float atB) const
{
    const int steps = jmax (1, (int) (a.getDistanceFrom (b) / 0.5f));
    for (int k = 0; k <= steps; ++k)
    {
        const float u = (float) k / (float) steps;
        const auto p = a + (b - a) * u;
        addPoint (light, w, h, s, p.x, p.y, c, 0.55f * (atA + (atB - atA) * u));
    }
}

void Sky::addRing (std::vector<float>& light, int w, int h, float s, Point<float> c, float rx, float ry, float width, Rgb col, float level) const
{
    if (level <= 0.003f) return;
    const float cx = c.x * s, cy = c.y * s, ax = rx * s, ay = ry * s, wd = width * s;
    const int i0 = jmax (0, (int) (cx - ax - 3 * wd)), i1 = jmin (w - 1, (int) (cx + ax + 3 * wd) + 1);
    const int j0 = jmax (0, (int) (cy - ay - 3 * wd)), j1 = jmin (h - 1, (int) (cy + ay + 3 * wd) + 1);
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
            const float dx = ((float) i + 0.5f - cx) / ax, dy = ((float) j + 0.5f - cy) / ay;
            const float q = std::sqrt (dx * dx + dy * dy);
            const float d = (q - 1.0f) * ax / wd;
            if (std::abs (d) > 3.0f) continue;
            const float v = level * expNeg (d * d);
            float* px = light.data() + (size_t) ((j * w + i) * 3);
            px[0] += v * col.r; px[1] += v * col.g; px[2] += v * col.b;
        }
}

// the aurora: per column, a curtain hanging from a waving line, as tall and bright as its band, in vertical rays that
// shimmer; brightest along its lower edge
void Sky::renderAurora (std::vector<float>& light, int w, int h, float s, float brightness)
{
    for (int i = 0; i < w; ++i)
    {
        const float x = ((float) i + 0.5f) / s;
        const float u = bandPosition (x);
        const float lev = levelAt (u), fl = flashAt (u);
        const float amount = brightness * (0.5f * std::pow (lev, 1.2f) + 0.3f * fl * lev);
        if (amount < 0.004f) continue;
        const float rays = 0.35f + 0.65f * std::pow (smoothNoise (x / 1.7f + time * 5.0f, time * 0.8f), 1.5f)
                                 * (0.6f + 0.4f * smoothNoise (x / 11.0f - time * 1.3f, 3.7f));
        const float yc = 19.0f + 4.5f * std::sin (x / 55.0f - 1.1f * time) + 2.5f * std::sin (x / 19.0f + 1.7f * time);
        const float above = 3.0f + 9.0f * lev, below = 1.6f + 1.5f * lev;
        const Rgb c = palette (u);
        const float v0 = amount * rays;
        const int j0 = jmax (0, (int) ((yc - 2.5f * above) * s)), j1 = jmin (h - 1, (int) ((yc + 2.5f * below) * s) + 1);
        for (int j = j0; j <= j1; ++j)
        {
            const float y = ((float) j + 0.5f) / s;
            const float d = y < yc ? (yc - y) / above : (y - yc) / below;
            const float edge = expNeg ((y - yc) * (y - yc) * (1.0f / 1.21f));
            const float v = v0 * (expNeg (d * d) + 0.5f * edge);
            float* px = light.data() + (size_t) ((j * w + i) * 3);
            // the lower edge a little whiter
            px[0] += v * (c.r + 0.35f * edge * (1.0f - c.r));
            px[1] += v * (c.g + 0.35f * edge * (1.0f - c.g));
            px[2] += v * (c.b + 0.35f * edge * (1.0f - c.b));
        }
    }
}

void Sky::render (std::vector<float>& light, int w, int h, float s, float brightness)
{
    // the Milky Way: a warm band of dust across the tail, rendered once per size; it breathes with the lows
    if (w != hazeW || h != hazeH || s != hazeS)
    {
        hazeW = w; hazeH = h; hazeS = s;
        haze.assign ((size_t) (w * h * 3), 0.0f);
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const float x = ((float) i + 0.5f) / s, y = ((float) j + 0.5f) / s;
                const float centre = 26.0f - 16.0f * (x / 420.0f);                 // rising to the right
                const float milky = std::exp (-std::pow ((y - centre) / 10.0f, 2.0f)) * jlimit (0.0f, 1.0f, (480.0f - x) / 300.0f);
                const float n = 0.55f + 0.45f * smoothNoise (x / 9.0f, y / 4.0f) + 0.25f * smoothNoise (x / 3.0f, y / 1.5f);
                const float v = 0.085f * milky * n;
                float* px = haze.data() + (size_t) ((j * w + i) * 3);
                px[0] = v; px[1] = v * 0.78f; px[2] = v * 0.6f;
            }
    }
    const float hazeLevel = brightness * (0.7f + 0.6f * kicks.level + 0.4f * heart);
    for (size_t k = 0; k < haze.size(); ++k) light[k] += haze[k] * hazeLevel;

    renderAurora (light, w, h, s, brightness);

    for (auto& d : dust)
        addDot (light, w, h, s, d.x, d.y, 0.25f, 0.6f, 0.25f, { d.r, d.g, d.b },
                brightness * d.size * (0.55f + 0.45f * std::sin (d.phase + d.rate * time)) * (0.8f + 0.5f * loudness));
    for (auto& g : glints)
    {
        const auto& d = dust[(size_t) g.star];
        const float a = brightness * 1.6f * std::pow (1.0f - g.age / g.life, 1.5f);
        addDot (light, w, h, s, d.x, d.y, 0.45f, 1.2f, 0.4f, g.c, a);
        addStreak (light, w, h, s, { d.x, d.y }, { d.x - 3.0f, d.y }, g.c, a * 0.4f, 0.0f);
        addStreak (light, w, h, s, { d.x, d.y }, { d.x + 3.0f, d.y }, g.c, a * 0.4f, 0.0f);
    }

    // the figure's lines, and the kicks' pulses running out from the heart along the body
    const float lineLevel = brightness * (0.09f + 0.1f * loudness + 0.12f * heart);
    for (auto& l : lines)
        addStreak (light, w, h, s, { stars[(size_t) l[0]].x, stars[(size_t) l[0]].y }, { stars[(size_t) l[1]].x, stars[(size_t) l[1]].y },
                   { 0.75f, 0.8f, 1.0f }, lineLevel, lineLevel);
    for (auto& p : pulses)
    {
        const float front = 520.0f * p.age, fade = p.strength * (1.0f - p.age / p.life) * brightness;
        for (float d = 0.0f; d <= bodyLength.back(); d += 0.6f)
        {
            const float off = (std::abs (d - heartAlong) - front) / 9.0f;
            if (std::abs (off) > 2.5f) continue;
            const auto q = along (d);
            addPoint (light, w, h, s, q.x, q.y, palette (std::abs (d - heartAlong) / 500.0f), 0.55f * fade * std::exp (-off * off));
        }
    }

    // the stars, each with its band: brighter as it plays, a coloured flash when it hits
    for (size_t i = 0; i < stars.size(); ++i)
    {
        const auto& st = stars[i];
        const bool heartStar = (int) i == antares;
        const float u = bandPosition (st.x), lev = levelAt (u), fl = heartStar ? heart : flashAt (u);
        const auto pc = palette (u);
        const Rgb c { st.r + (pc.r - st.r) * 0.4f * fl, st.g + (pc.g - st.g) * 0.4f * fl, st.b + (pc.b - st.b) * 0.4f * fl };
        const float twinkle = 0.75f + 0.25f * std::sin (st.phase + st.rate * time);
        const float level = brightness * twinkle * (0.9f + 1.1f * st.size) * (0.7f + 0.5f * lev + 0.9f * fl);
        const float radius = 0.5f + 0.8f * st.size + 0.5f * fl + (heartStar ? 0.8f * heart : 0.0f);
        addDot (light, w, h, s, st.x, st.y, radius, heartStar ? 3.0f + 6.0f * heart : 1.6f + 1.2f * fl, heartStar ? 0.5f : 0.35f, c, level);
        if (st.size > 0.3f || fl > 0.2f)        // the brighter ones, and any flashing: four spikes
        {
            const float len = (2.0f + 5.0f * st.size) * (1.0f + 1.5f * fl);
            for (auto dir : { Point<float> (len, 0.0f), Point<float> (-len, 0.0f), Point<float> (0.0f, len * 0.6f), Point<float> (0.0f, -len * 0.6f) })
                addStreak (light, w, h, s, { st.x, st.y }, { st.x + dir.x, st.y + dir.y }, c, level * 0.3f, 0.0f);
        }
    }

    // the heart's rings: out over the sky, crimson turning magenta
    const Point<float> heartAt { stars[(size_t) antares].x, stars[(size_t) antares].y };
    for (auto& r : rings)
    {
        const float u = r.age / r.life;
        const float rx = 5.0f + 210.0f * r.age * (1.0f - 0.3f * u);
        const Rgb c { 1.0f, 0.16f + 0.1f * u, 0.12f + 0.5f * u };
        addRing (light, w, h, s, heartAt, rx, rx * 0.3f, 1.0f + 2.0f * u, c, brightness * 0.5f * r.strength * std::pow (1.0f - u, 2.0f));
    }

    // venom: a white-hot head, its trail amber, red, then magenta
    for (auto& cm : comets)
    {
        if (cm.along < 0.0f) continue;
        const auto head = along (cm.along);
        addDot (light, w, h, s, head.x, head.y, 0.6f, 1.8f, 0.4f, { 1.0f, 0.92f, 0.7f }, brightness * 1.8f * (0.6f + 0.4f * cm.strength));
        for (float k = 0.5f; k < 80.0f; k += 0.5f)
        {
            if (cm.along - k < 0.0f) break;
            const auto p = along (cm.along - k);
            const float u = k / 80.0f;
            const Rgb c = u < 0.3f ? Rgb { 1.0f, 0.75f - 0.6f * u, 0.3f - 0.5f * u }
                                   : Rgb { 1.0f - 0.25f * (u - 0.3f), jmax (0.12f, 0.57f - 0.9f * (u - 0.3f)), 0.15f + 0.6f * (u - 0.3f) };
            addPoint (light, w, h, s, p.x, p.y, c, brightness * 0.45f * cm.strength * std::pow (1.0f - u, 1.6f));
        }
    }
    // the stinger flares when venom reaches it, and its sparks fly
    if (sting > 0.01f)
        addDot (light, w, h, s, body.back().x, body.back().y, 0.8f + 0.8f * sting, 4.0f, 0.5f, { 0.5f, 1.0f, 0.75f }, brightness * 1.4f * sting);
    for (auto& sp : sparks)
    {
        const float fade = brightness * 1.2f * (1.0f - sp.age / sp.life);
        addStreak (light, w, h, s, { sp.x - sp.vx * 0.035f, sp.y - sp.vy * 0.035f }, { sp.x, sp.y }, sp.c, 0.2f * fade, fade);
    }
}
}
