#include "fx.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradientShader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <cstdlib>

namespace fx {

static SkBitmap g_bmp;
static sk_sp<SkImage> g_bg;
static int g_w = 0, g_cap_h = 0;
static float g_scale = 1.0f;

static uint8_t c8(float v) {
    return (uint8_t)std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f);
}

static SkColor to_sk(const Decal& d, float alpha_mul = 1.0f) {
    return SkColorSetARGB(c8(d.a * alpha_mul), c8(d.r), c8(d.g), c8(d.b));
}

static SkColor argb(float a, int r, int g, int b) {
    return SkColorSetARGB(c8(a), r, g, b);
}

static SkPaint linear_fill(float x0, float y0, float x1, float y1,
                           SkColor a, SkColor b) {
    SkPoint pts[2] = {{x0, y0}, {x1, y1}};
    SkColor cols[2] = {a, b};
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(SkGradientShader::MakeLinear(pts, cols, nullptr, 2,
                                           SkTileMode::kClamp));
    return p;
}

static SkPaint linear3(float x0, float y0, float x1, float y1, SkColor a,
                       SkColor b, SkColor c, float mid = 0.5f) {
    SkPoint pts[2] = {{x0, y0}, {x1, y1}};
    SkColor cols[3] = {a, b, c};
    float pos[3] = {0.0f, mid, 1.0f};
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(SkGradientShader::MakeLinear(pts, cols, pos, 3,
                                           SkTileMode::kClamp));
    return p;
}

static SkPaint radial_fill(float cx, float cy, float rad, SkColor a,
                           SkColor b) {
    SkColor cols[2] = {a, b};
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(SkGradientShader::MakeRadial({cx, cy}, rad, cols,
                                             nullptr, 2,
                                             SkTileMode::kClamp));
    return p;
}

static SkPaint solid(SkColor c) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(c);
    return p;
}

static SkPaint stroke(SkColor c, float w) {
    SkPaint p = solid(c);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(w);
    return p;
}

static SkRRect rrect_inset(const SkRRect& rr, float d) {
    SkRRect out;
    rr.inset(d, d, &out);
    return out;
}

static SkPaint blur_paint(SkColor c, float sigma) {
    SkPaint p = solid(c);
    p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
    return p;
}

// ---------------------------------------------------------- background

/// Painted once per width into a buffer taller than the window, so the
/// height animation never triggers an expensive re-blur. Blob layout is
/// anchored to a nominal height -> nothing shifts while resizing.
static void build_background(int w, int h, float scale) {
    auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
    SkCanvas* c = surf->getCanvas();
    float nh = 720.0f * scale; // nominal layout height

    c->drawPaint(linear_fill(0, 0, 0, nh * 1.2f, 0xFF0B1826, 0xFF03060B));

    struct Blob { float x, y, r; SkColor col; };
    const Blob blobs[] = {
        {w * 0.85f, nh * 0.02f, w * 0.50f, 0x4C39E0C8}, // teal TR
        {w * 0.08f, nh * 0.25f, w * 0.42f, 0x3A2A5FD0}, // blue L
        {w * 0.55f, nh * 0.45f, w * 0.48f, 0x30203A80}, // indigo C
        {w * 0.50f, nh * 0.62f, w * 0.40f, 0x2A0F8A55}, // green under beam
        {w * 0.15f, nh * 0.95f, w * 0.40f, 0x26107A55}, // green BL
        {w * 0.90f, nh * 0.85f, w * 0.45f, 0x2A1A5A90}, // violet BR
    };
    for (auto& b : blobs)
        c->drawCircle(b.x, b.y, b.r, blur_paint(b.col, b.r * 0.35f));

    // faint wave "nebula streaks" across the middle band
    for (int k = 0; k < 3; ++k) {
        SkPath p;
        float y0 = nh * (0.30f + 0.08f * k);
        for (int i = 0; i <= 64; ++i) {
            float x = w * (float)i / 64.0f;
            float y = y0 + std::sin(i * 0.18f + k * 1.7f) * 14.0f * scale +
                      std::sin(i * 0.05f + k) * 22.0f * scale;
            i ? p.lineTo(x, y) : p.moveTo(x, y);
        }
        SkPaint sp = stroke(SkColorSetARGB(22, 120, 190, 255),
                            1.2f * scale);
        sp.setMaskFilter(
            SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 1.5f * scale));
        c->drawPath(p, sp);
    }

    uint32_t seed = 1337;
    auto rnd = [&] {
        seed = seed * 1664525u + 1013904223u;
        return (float)(seed >> 8) / 16777216.0f;
    };
    int stars = (int)(w * h / (2600.0f * scale * scale));
    for (int i = 0; i < stars; ++i) {
        float x = rnd() * w, y = rnd() * h;
        float a = 0.12f + rnd() * 0.55f;
        float sz = (rnd() < 0.85f ? 0.9f : 1.6f) * scale;
        c->drawCircle(x, y, sz,
                      solid(SkColorSetARGB((int)(a * 255), 200, 220, 235)));
    }
    for (int i = 0; i < stars / 14; ++i) {
        float x = rnd() * w, y = rnd() * h;
        float s = (2.0f + rnd() * 4.0f) * scale;
        c->drawCircle(x, y, s * 0.4f,
                      blur_paint(SkColorSetARGB(140, 190, 235, 255), s));
    }

    float rad = std::sqrt((float)w * w + nh * nh) * 0.65f;
    c->drawRect(SkRect::MakeWH((float)w, (float)h),
                radial_fill(w * 0.5f, nh * 0.42f, rad, 0x00000000,
                            0x70000000));

    g_bg = surf->makeImageSnapshot();
}

// ------------------------------------------------------------ decals

static void draw_panel(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    c->drawRRect(rr, linear_fill(0, d.y, 0, d.y + d.h,
                                 SkColorSetARGB(30, 170, 200, 235),
                                 SkColorSetARGB(14, 120, 160, 210)));
    c->drawRRect(rrect_inset(rr, 0.5f),
                 stroke(SkColorSetARGB(40, 160, 200, 245), 1.0f));
    c->drawLine(d.x + d.radius, d.y + 1, d.x + d.w - d.radius, d.y + 1,
                linear3(d.x + d.radius, 0, d.x + d.w - d.radius, 0,
                        SkColorSetARGB(0, 255, 255, 255),
                        SkColorSetARGB(50, 255, 255, 255),
                        SkColorSetARGB(0, 255, 255, 255)));
}

/// Glass action bar: frosted body, blue rim that brightens on hover.
static void draw_bar(SkCanvas* c, const Decal& d) {
    float hv = std::clamp(d.hover, 0.0f, 1.0f);
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    if (hv > 0.01f)
        c->drawRRect(rr, blur_paint(argb(0.28f * hv, 70, 150, 255), 10.0f));
    c->drawRRect(rr, linear_fill(0, d.y, 0, d.y + d.h,
                                 argb(0.16f + 0.06f * hv, 150, 180, 225),
                                 argb(0.07f + 0.04f * hv, 90, 120, 170)));
    // inner top sheen
    SkRRect top = SkRRect::MakeRectXY(
        SkRect::MakeXYWH(d.x + 1, d.y + 1, d.w - 2, d.h * 0.5f),
        d.radius - 1, d.radius - 1);
    c->drawRRect(top, linear_fill(0, d.y, 0, d.y + d.h * 0.5f,
                                  argb(0.10f + 0.05f * hv, 255, 255, 255),
                                  argb(0.0f, 255, 255, 255)));
    c->drawRRect(rrect_inset(rr, 0.5f),
                 stroke(argb(0.28f + 0.40f * hv, 110, 170, 250), 1.0f));
    // hover sweep highlight travelling with the anim value
    if (hv > 0.01f && hv < 0.99f) {
        float sx = d.x + d.w * hv;
        c->save();
        c->clipRRect(rr, true);
        c->drawRect(SkRect::MakeXYWH(sx - 60, d.y, 120, d.h),
                    linear3(sx - 60, 0, sx + 60, 0, argb(0, 255, 255, 255),
                            argb(0.10f, 200, 230, 255),
                            argb(0, 255, 255, 255)));
        c->restore();
    }
}

static void draw_iconbtn(SkCanvas* c, const Decal& d) {
    float hv = std::clamp(d.hover, 0.0f, 1.0f);
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    if (hv > 0.01f)
        c->drawRRect(rr, blur_paint(argb(0.45f * hv, c8(d.r), c8(d.g),
                                         c8(d.b)),
                                    6.0f));
    c->drawRRect(rr, linear_fill(0, d.y, 0, d.y + d.h,
                                 argb(0.22f + 0.25f * hv, 120, 165, 230),
                                 argb(0.10f + 0.20f * hv, 60, 100, 170)));
    c->drawRRect(rrect_inset(rr, 0.5f),
                 stroke(argb(0.30f + 0.45f * hv, 140, 190, 255), 1.0f));
}

static void draw_darkbox(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    c->drawRRect(rr, solid(SkColorSetARGB(150, 4, 9, 14)));
    c->drawRRect(rrect_inset(rr, 0.5f),
                 stroke(SkColorSetARGB(30, 160, 200, 230), 1.0f));
}

/// Glass pill; `intensity` = active (neon), `hover` = hover anim.
static void draw_pill(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.h * 0.5f, d.h * 0.5f);
    float on = std::clamp(d.intensity, 0.0f, 1.0f);
    float hv = std::clamp(d.hover, 0.0f, 1.0f) * (1.0f - on);
    // aura
    float aura = on + hv * 0.35f;
    if (aura > 0.01f)
        c->drawRRect(rr, blur_paint(argb(0.65f * aura * d.a, c8(d.r),
                                         c8(d.g), c8(d.b)),
                                    10.0f));
    // body: grey glass -> blue glass as `on` grows
    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    SkColor top = argb(mix(0.20f + 0.08f * hv, 0.55f, on),
                       (int)mix(150, 70, on), (int)mix(165, 150, on),
                       (int)mix(185, 235, on));
    SkColor bot = argb(mix(0.10f + 0.06f * hv, 0.35f, on),
                       (int)mix(80, 25, on), (int)mix(90, 80, on),
                       (int)mix(110, 170, on));
    c->drawRRect(rr, linear_fill(0, d.y, 0, d.y + d.h, top, bot));
    // glossy upper half
    SkRRect gl = SkRRect::MakeRectXY(
        SkRect::MakeXYWH(d.x + 2, d.y + 1.5f, d.w - 4, d.h * 0.48f),
        d.h * 0.3f, d.h * 0.3f);
    c->drawRRect(gl, linear_fill(0, d.y, 0, d.y + d.h * 0.5f,
                                 argb(0.18f + 0.12f * on, 255, 255, 255),
                                 argb(0.0f, 255, 255, 255)));
    // rim
    SkColor rim = on > 0.01f
                      ? argb(mix(0.35f, 1.0f, on), (int)mix(170, 120, on),
                             (int)mix(190, 200, on), 255)
                      : argb(0.30f + 0.30f * hv, 170, 190, 215);
    c->drawRRect(rrect_inset(rr, 0.75f), stroke(rim, on > 0.5f ? 1.5f : 1.0f));
}

/// Sine "energy wisp" across the beam, phase-animated by `t`.
static void wisp(SkCanvas* c, float x0, float x1, float cy, float amp,
                 float freq, float phase, SkColor col, float width,
                 float blur) {
    SkPath p;
    const int N = 72;
    for (int i = 0; i <= N; ++i) {
        float u = (float)i / N;
        float x = x0 + (x1 - x0) * u;
        // envelope: thin at the edges, fat near the center gem
        float env = std::sin(u * 3.14159f);
        float y = cy + std::sin(u * freq + phase) * amp * env +
                  std::sin(u * freq * 2.3f - phase * 1.4f) * amp * 0.35f *
                      env;
        i ? p.lineTo(x, y) : p.moveTo(x, y);
    }
    SkPaint sp = stroke(col, width);
    if (blur > 0)
        sp.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur));
    sp.setShader(SkGradientShader::MakeLinear(
        std::array<SkPoint, 2>{SkPoint{x0, cy}, SkPoint{x1, cy}}.data(),
        std::array<SkColor, 3>{SkColorSetA(col, 0), col,
                               SkColorSetA(col, 0)}
            .data(),
        nullptr, 3, SkTileMode::kClamp));
    c->drawPath(p, sp);
}

static void draw_beam(SkCanvas* c, const Decal& d, float t) {
    float cy = d.y + d.h * 0.5f;
    float on = std::clamp(d.intensity, 0.0f, 1.5f);
    SkColor col = to_sk(d);
    int r = SkColorGetR(col), g = SkColorGetG(col), b = SkColorGetB(col);

    // wide soft halo band
    c->drawRect(SkRect::MakeXYWH(d.x, cy - d.h * 0.35f, d.w, d.h * 0.7f),
                blur_paint(argb(0.30f * on, r, g, b), 24.0f));

    // energy wisps
    float speed = 1.2f + 0.8f * std::min(on, 1.0f);
    struct W { float amp, freq, ph, w, a, blur; };
    const W ws[] = {
        {d.h * 0.32f, 9.0f, 0.0f, 6.0f, 0.45f, 6.0f},
        {d.h * 0.24f, 12.0f, 2.1f, 1.8f, 0.95f, 0.6f},
        {d.h * 0.38f, 7.0f, 4.2f, 1.3f, 0.80f, 0.5f},
        {d.h * 0.18f, 15.0f, 1.0f, 1.2f, 0.85f, 0.3f},
        {d.h * 0.28f, 10.5f, 5.3f, 3.5f, 0.45f, 3.0f},
        {d.h * 0.42f, 5.5f, 3.3f, 1.0f, 0.65f, 0.4f},
        {d.h * 0.12f, 18.0f, 0.4f, 1.0f, 0.75f, 0.3f},
    };
    for (size_t i = 0; i < std::size(ws); ++i) {
        auto& w = ws[i];
        float ph = w.ph + t * speed * (i % 2 ? -1.0f : 1.0f);
        wisp(c, d.x, d.x + d.w, cy, w.amp, w.freq, ph,
             argb(w.a * std::min(on, 1.0f), std::min(255, r + 40),
                  std::min(255, g + 20), std::min(255, b + 40)),
             w.w, w.blur);
    }

    // horizontal fade beam
    c->drawRect(SkRect::MakeXYWH(d.x, cy - 9, d.w, 18),
                linear3(d.x, cy, d.x + d.w, cy, SkColorSetA(col, 0),
                        argb(0.55f * std::min(on, 1.0f), r, g, b),
                        SkColorSetA(col, 0)));
    // hot core line
    c->drawRect(SkRect::MakeXYWH(d.x, cy - 1.5f, d.w, 3),
                linear3(d.x, cy, d.x + d.w, cy, argb(0, 230, 255, 240),
                        argb(0.9f * std::min(on, 1.0f), 230, 255, 240),
                        argb(0, 230, 255, 240)));

    // drifting sparks
    uint32_t seed = 4242;
    auto rnd = [&] {
        seed = seed * 1664525u + 1013904223u;
        return (float)(seed >> 8) / 16777216.0f;
    };
    for (int i = 0; i < 46; ++i) {
        float base = rnd(), spd = 0.02f + rnd() * 0.05f;
        float dir = rnd() < 0.5f ? -1.0f : 1.0f;
        float u = std::fmod(base + dir * t * spd + 2.0f, 1.0f);
        float x = d.x + d.w * u;
        float env = std::sin(u * 3.14159f);
        float y = cy + (rnd() - 0.5f) * d.h * 0.9f * env +
                  std::sin(t * 1.5f + i) * 3.0f;
        float tw = 0.5f + 0.5f * std::sin(t * (2.0f + rnd() * 3.0f) + i);
        float sz = 0.6f + rnd() * 1.2f;
        c->drawCircle(x, y, sz,
                      solid(argb((0.25f + 0.6f * tw) * env *
                                     std::min(on, 1.0f),
                                 200, 255, 215)));
    }
}

static void draw_gem(SkCanvas* c, const Decal& d, float t) {
    float cx = d.x + d.w * 0.5f, cy = d.y + d.h * 0.5f;
    float on = std::clamp(d.intensity, 0.0f, 1.5f);
    float pulse = 0.5f + 0.5f * std::sin(t * 2.2f);
    float s = std::min(d.w, d.h) * 0.46f * (0.95f + 0.07f * on);

    // halo (breathing)
    c->drawCircle(cx, cy, s * (1.7f + 0.15f * pulse),
                  blur_paint(argb((0.45f + 0.12f * pulse) * std::min(on, 1.2f),
                                  60, 230, 130),
                             22.0f));

    float sx = s * 0.86f;
    SkPath dia;
    dia.moveTo(cx, cy - s);
    dia.lineTo(cx + sx, cy);
    dia.lineTo(cx, cy + s);
    dia.lineTo(cx - sx, cy);
    dia.close();
    SkPaint fill = linear3(cx, cy - s, cx, cy + s, 0xF0D2FBE4, 0xE036C878,
                           0xF00A5C30, 0.5f);
    c->drawPath(dia, fill);

    // facets: four triangles with alternating light
    SkPoint top{cx, cy - s}, rt{cx + sx, cy}, bot{cx, cy + s}, lt{cx - sx, cy};
    SkPoint ctr{cx, cy - s * 0.08f};
    auto tri = [&](SkPoint a, SkPoint b, SkColor col) {
        SkPath p;
        p.moveTo(a);
        p.lineTo(b);
        p.lineTo(ctr);
        p.close();
        c->drawPath(p, solid(col));
    };
    tri(top, rt, SkColorSetARGB(40, 255, 255, 255));
    tri(rt, bot, SkColorSetARGB(50, 0, 40, 20));
    tri(bot, lt, SkColorSetARGB(25, 0, 30, 10));
    tri(lt, top, SkColorSetARGB(70, 255, 255, 255));
    SkPaint facet = stroke(SkColorSetARGB(90, 230, 255, 240), 1.0f);
    for (SkPoint q : {top, rt, bot, lt})
        c->drawLine(q, ctr, facet);

    c->drawPath(dia, stroke(SkColorSetARGB(220, 215, 255, 230), 1.6f));

    // shimmer line sweeping over the crystal
    float sw = std::fmod(t * 0.6f, 2.2f) - 0.6f; // -0.6 .. 1.6
    if (sw > -0.3f && sw < 1.3f) {
        c->save();
        c->clipPath(dia, true);
        float sx = cx - s + 2 * s * sw;
        SkPath band;
        band.moveTo(sx - s * 0.25f, cy - s);
        band.lineTo(sx + s * 0.05f, cy - s);
        band.lineTo(sx - s * 0.45f, cy + s);
        band.lineTo(sx - s * 0.75f, cy + s);
        band.close();
        c->drawPath(band, solid(SkColorSetARGB(70, 255, 255, 255)));
        c->restore();
    }

    c->drawCircle(cx - s * 0.30f, cy - s * 0.38f, s * 0.08f,
                  blur_paint(argb(0.6f + 0.4f * pulse, 255, 255, 255),
                             s * 0.10f));
}

static void draw_glow(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    float on = std::clamp(d.intensity, 0.0f, 1.0f);
    c->drawRRect(rr, blur_paint(to_sk(d, on * 0.9f), 14.0f));
}

static void draw_row(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    c->drawRRect(rr, solid(to_sk(d)));
}

static void draw_line(SkCanvas* c, const Decal& d) {
    c->drawLine(d.x, d.y, d.x + d.w, d.y,
                linear3(d.x, d.y, d.x + d.w, d.y,
                        SkColorSetARGB(0, 200, 225, 240),
                        SkColorSetARGB(60, 200, 225, 240),
                        SkColorSetARGB(0, 200, 225, 240)));
}

/// Window outline: thin blue rim, brighter along the top.
static void draw_frame(SkCanvas* c, const Decal& d) {
    SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(d.x, d.y, d.w, d.h),
                                     d.radius, d.radius);
    SkPaint p = stroke(0, 1.0f);
    p.setShader(SkGradientShader::MakeLinear(
        std::array<SkPoint, 2>{SkPoint{0, d.y}, SkPoint{0, d.y + d.h}}
            .data(),
        std::array<SkColor, 2>{SkColorSetARGB(150, 90, 160, 240),
                               SkColorSetARGB(60, 60, 110, 190)}
            .data(),
        nullptr, 2, SkTileMode::kClamp));
    c->drawRRect(rrect_inset(rr, 0.5f), p);
}

// -------------------------------------------------------------- API

void init() {}

bool render(int w, int h, float scale, float t,
            const std::vector<Decal>& decals) {
    if (w <= 0 || h <= 0)
        return false;
    if (w != g_w || h > g_cap_h || scale != g_scale || !g_bg) {
        // headroom so panel/console expansion never reallocates
        int cap = std::max(h, (int)(1100 * scale));
        g_bmp.allocPixels(SkImageInfo::MakeN32Premul(w, cap));
        build_background(w, cap, scale);
        g_w = w;
        g_cap_h = cap;
        g_scale = scale;
    }
    SkCanvas canvas(g_bmp);
    canvas.clipRect(SkRect::MakeWH((float)w, (float)h));
    canvas.drawImage(g_bg, 0, 0);

    // Animated starfield over the baked nebula: two parallax layers of
    // drifting stars with per-star twinkle. Deterministic layout,
    // ~100 circles/frame -- cheap against the cached backdrop.
    struct Star {
        float x, y;   // normalized 0..1
        float r, a;   // radius factor, base alpha
        float spd;    // drift speed (logical px/s)
        float tw, ph; // twinkle freq + phase
    };
    static std::vector<Star> g_stars;
    if (g_stars.empty()) {
        uint32_t seed = 424242;
        auto rnd = [&] {
            seed = seed * 1664525u + 1013904223u;
            return (float)(seed >> 8) / 16777216.0f;
        };
        for (int i = 0; i < 80; ++i)
            g_stars.push_back(
                {rnd(), rnd(), 0.5f + rnd() * 1.0f, 0.15f + rnd() * 0.5f,
                 1.0f + rnd() * (i % 3 == 0 ? 13.0f : 4.5f),
                 0.8f + rnd() * 2.2f, rnd() * 6.283f});
    }
    for (auto& s : g_stars) {
        float x = std::fmod(s.x * w + t * s.spd * scale,
                            (float)w + 20.0f) -
                  10.0f;
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(t * s.tw + s.ph));
        canvas.drawCircle(x, s.y * h, s.r * scale,
                          solid(SkColorSetARGB((int)(s.a * tw * 255), 205,
                                               225, 240)));
    }
    for (int i = 0; i < 7; ++i) { // a few larger glowing stars
        const Star& s = g_stars[i * 11 % (int)g_stars.size()];
        float x = std::fmod(s.x * w + t * s.spd * scale,
                            (float)w + 20.0f) -
                  10.0f;
        float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(t * s.tw + s.ph));
        float sz = (3.0f + 3.0f * s.r) * scale;
        canvas.drawCircle(x, s.y * h, sz * 0.4f,
                          blur_paint(SkColorSetARGB((int)(90 * tw), 170,
                                                    225, 255),
                                     sz));
    }

    canvas.save();
    canvas.scale(scale, scale);
    for (const auto& d : decals) {
        switch (d.kind) {
        case Kind::Panel: draw_panel(&canvas, d); break;
        case Kind::DarkBox: draw_darkbox(&canvas, d); break;
        case Kind::Pill: draw_pill(&canvas, d); break;
        case Kind::Beam: draw_beam(&canvas, d, t); break;
        case Kind::Gem: draw_gem(&canvas, d, t); break;
        case Kind::GlowRect: draw_glow(&canvas, d); break;
        case Kind::Row: draw_row(&canvas, d); break;
        case Kind::Line: draw_line(&canvas, d); break;
        case Kind::Bar: draw_bar(&canvas, d); break;
        case Kind::IconBtn: draw_iconbtn(&canvas, d); break;
        case Kind::Frame: draw_frame(&canvas, d); break;
        }
    }
    canvas.restore();
    return true;
}

const void* pixels() { return g_bmp.getPixels(); }
int stride() { return (int)g_bmp.rowBytes(); }
int buffer_h() { return g_cap_h; }

} // namespace fx
