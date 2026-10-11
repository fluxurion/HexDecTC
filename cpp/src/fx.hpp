#pragma once
// fx — Skia decor layer. The UI pass queues `Decal`s each frame in
// logical (point) coordinates; render() paints the cached nebula
// background plus the decals into a BGRA bitmap that is uploaded to an
// SDL texture behind the ImGui widgets.

#include <cstdint>
#include <vector>

namespace fx {

enum class Kind {
    Panel,    // frosted glass rounded rect
    DarkBox,  // dark inset panel (console)
    Pill,     // tab pill; intensity = active, hover = hover anim
    Beam,     // horizontal glowing bar + animated energy wisps
    Gem,      // diamond crystal at rect center
    GlowRect, // pure glow behind a rect
    Row,      // subtle row highlight
    Line,     // hairline separator
    Bar,      // glass action bar with blue rim; hover = hover anim
    IconBtn,  // small rounded-square glass button; hover = hover anim
    Frame,    // glowing window outline
};

struct Decal {
    Kind kind;
    float x, y, w, h;
    float r, g, b, a;
    float radius = 10.0f;
    float intensity = 1.0f; // active/glow amount
    float hover = 0.0f;     // 0..1 animated hover
};

void init();
/// Repaint the frame: (re)builds the background only when the width
/// changes or the height outgrows the buffer, then draws `decals`.
/// `scale` converts logical points to pixels, `t` drives animations.
bool render(int w, int h, float scale, float t,
            const std::vector<Decal>& decals);
const void* pixels();
int stride();
/// Allocated buffer height in pixels (>= the last rendered height).
int buffer_h();

} // namespace fx
