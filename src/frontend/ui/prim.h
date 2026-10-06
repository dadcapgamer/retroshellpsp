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
/* Tight, hardware-cheap elevation used under selected controls. */
void dropShadow(gfx::Renderer& r, float x, float y, float w, float h,
                float radius, u32 color);

/* Selected-row treatment shared by every list panel (settings, core
 * picker, in-game menu): tight shadow, soft tint and accent focus ring. */
void focusRow(gfx::Renderer& r, float x, float y, float w, float h,
              u32 fill, u32 bar, u32 shadow);

void circle(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void ring(gfx::Renderer& r, float cx, float cy, float radius, u32 color);

/* Icon glyphs (vector, no textures) --------------------------------- */
void iconClock(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void iconStar(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
void iconGear(gfx::Renderer& r, float cx, float cy, float radius, u32 color);
/* Console icons (Figma "console icons v2"): System enum order 0..8,
 * settings=9. `size` snaps to 24, 48 or 64 (the artwork has built-in padding). */
void iconSystem(gfx::Renderer& r, int systemIdx, float x, float y, float size,
                u32 base, u32 detail);

/* Flat 1-bit-style shapes for the firmware look: no AA masks, integer
 * aligned, so edges stay crisp on the LCD. */
void outlineRect(gfx::Renderer& r, float x, float y, float w, float h,
                 float thickness, u32 color);
enum class Dir { Up, Down, Left, Right };
/* Open chevron (<, >, ^, v), `size` is the half-height of the head. */
void chevron(gfx::Renderer& r, Dir d, float cx, float cy, float size,
             float thickness, u32 color);
/* Arrow with a stem, used by the d-pad legend glyphs. */
void arrow(gfx::Renderer& r, Dir d, float cx, float cy, float size, u32 color);
/* Sparse dot lattice for the artwork fallback; tiled from one baked tile so
 * a full placeholder costs a handful of sprites, not hundreds of rects. */
void dotField(gfx::Renderer& r, float x, float y, float w, float h, u32 color);

/* PSP face-button glyphs for hint bars. DpadUp/DpadDown draw as arrows;
 * Face buttons and START are the Figma pixel glyphs. */
enum class Button { Cross, Circle, Triangle, Square, DpadUp, DpadDown, Start,
                    L1, R1, Select, DpadLeftRight };
void buttonGlyph(gfx::Renderer& r, Button b, float cx, float cy, float radius,
                 u32 color);
/* Drawn width of a glyph in pixels (the Figma artwork is baked at legend
 * size, so widths differ: START is a wordmark). */
float buttonGlyphWidth(Button b);

/* Battery pill with fill level (0..1) or unknown (-1). */
void battery(gfx::Renderer& r, float x, float y, float level, bool charging,
             u32 color, u32 accent);

}  // namespace rs::ui::prim
