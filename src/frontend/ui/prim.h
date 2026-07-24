/** Procedural UI primitives: anti-aliased rounded rectangles, circles and
 * icon glyphs, all rasterized once at boot into tiny T8 masks and drawn
 * tinted. No image assets required — themes may override later.
 */
#pragma once

#include "platform/psp/gu_renderer.h"
#include "rs_common.h"

namespace rs::ui::prim {

bool init();

/* Anti-aliased rounded rectangle (9-sliced from the mask). `radius` is
 * clamped to min(w,h)/2 and capped at the baked corner radius (14px). */
void roundedRect(gfx::Renderer& r, float x, float y, float w, float h,
                 float radius, u32 color);
/* 1px-ish rounded outline (drawn as two nested fills is wasteful; this
 * draws the ring mask 9-sliced). */
void roundedOutline(gfx::Renderer& r, float x, float y, float w, float h,
                    float radius, u32 color);

void circle(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void ring(gfx::Renderer& r, float cx, float cy, float radius, u32 color);

/* Icon glyphs (vector, no textures) --------------------------------- */
void iconClock(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void iconStar(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void iconGear(gfx::Renderer& r, float cx, float cy, float radius, u32 color);

/* PSP face-button glyphs for hint bars. */
enum class Button { Cross, Circle, Triangle, Square };
void buttonGlyph(gfx::Renderer& r, Button b, float cx, float cy, float radius,
                 u32 color);

/* Battery pill with fill level (0..1) or unknown (-1). */
void battery(gfx::Renderer& r, float x, float y, float level, bool charging,
             u32 color, u32 accent);

}  // namespace rs::ui::prim
