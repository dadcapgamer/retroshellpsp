/** The shell's shared visual language. Every screen composes these so the
 * UI reads as one firmware rather than several pages:
 *
 *   layout   one 480x272 grid: 16px margins, header 0-27, footer 246-271
 *   focus    ONE language, three shapes of it, all in the accent:
 *              focusFill      rows, menus, settings (accent block)
 *              focusUnderline horizontal rails and tabs
 *              focusFrame     cards and artwork
 *   surfaces flat, pixel-rounded (no AA masks, no shadows, no blur)
 *   art      artwork contain-fit in a well, or the branded fallback
 *
 * Nothing here allocates or touches the Memory Stick; all of it is a handful
 * of untextured rects or one tiled sprite, cheap enough for any frame.
 */
#pragma once

#include "frontend/text/font.h"
#include "frontend/ui/prim.h"
#include "platform/psp/gu_renderer.h"
#include "rs_common.h"

#include <string>

namespace rs {
class App;
}

namespace rs::ui {

namespace layout {
constexpr float MARGIN = 16.f;
constexpr float RIGHT = RS_SCREEN_W - MARGIN;     /* 464 */
constexpr float HEADER_RULE_Y = 27.f;
constexpr float CONTENT_TOP = 36.f;
constexpr float FOOTER_RULE_Y = 246.f;
constexpr float CONTENT_BOTTOM = 238.f;
constexpr float ROW_H = 20.f;                     /* list / menu row */
}  // namespace layout

/* Colour with its alpha scaled by `alpha` (0..255). */
inline u32 fade(u32 color, u32 alpha) {
    return rsWithAlpha(color, rsAlphaOf(color) * alpha / 255u);
}

/* The shell's name for a system: title case, and "NES" rather than the
 * database's "Nintendo", which reads as a company on its own. */
const char* systemName(int systemId);

/* --- surfaces -------------------------------------------------------- */
/* Rect with pixel-stepped corners (radius 0..3). Integer aligned so edges
 * stay crisp on the LCD; no texture, no overdraw. */
void pixelRect(gfx::Renderer& r, float x, float y, float w, float h,
               int radius, u32 color);
/* Frame of `t` px (1 or 2) with stepped corners, drawn without overlap so a
 * translucent colour stays even. */
void pixelFrame(gfx::Renderer& r, float x, float y, float w, float h,
                int t, int radius, u32 color);

/* --- focus ------------------------------------------------------------ */
void focusFill(App& app, float x, float y, float w, float h, u32 alpha = 255);
void focusUnderline(App& app, float cx, float y, float w, u32 alpha = 255);
/* Frame drawn 1px outside the target with a 1px gap: x/y/w/h is the
 * focused element itself. */
void focusFrame(App& app, float x, float y, float w, float h, u32 alpha = 255);

/* --- text helpers ----------------------------------------------------- */
/* Uppercase technical label (section headers, overlines). */
void label(App& app, float x, float y, const char* text, u32 color,
           text::Align align = text::Align::Left);
/* Small flat chip with a monospaced label; returns its width. */
/* Chips: `system` is the filled identity chip ("GBC"); the others are
 * outlined tags ("Platformer"). */
float chip(App& app, float x, float y, const char* text, u32 alpha = 255,
           bool system = false);
constexpr float CHIP_H = 15.f;
float chipWidth(App& app, const char* text);

/* --- brand ------------------------------------------------------------ */
/* The RetroShell mark (assets/branding, website/assets): nine `dot` px
 * squares touching corner to corner in a diamond, 5*dot square. Cells are
 * numbered as a 3x3 grid, row major, turned 45 degrees (cell 0 is the top
 * point, BRAND_CENTER the middle); bit i of `skip` leaves out cell i. */
constexpr int BRAND_CELLS = 9;
constexpr int BRAND_CENTER = 4;
void brandMark(gfx::Renderer& r, float x, float y, int dot, u32 color,
               unsigned skip = 0u);
float brandMarkSize(int dot);

/* --- panels ------------------------------------------------------------ */
/* Backdrop under a popup: one rect, no stacked alpha. */
void backdrop(App& app, u32 alpha);
/* Popup surface: surface2 fill, 1px line border, radius 3. */
void panel(App& app, float x, float y, float w, float h, u32 alpha = 255);
/* Card / pane surface: surface fill, 1px line border, radius 3. */
void card(App& app, float x, float y, float w, float h, u32 alpha = 255);
/* Hairline between list rows, inset from both ends. */
void rowRule(App& app, float x, float y, float w, u32 alpha = 255);

/* One menu row inside a panel or list: focus fill when selected, label
 * left, optional value right (with < > when adjustable and focused). */
struct RowStyle {
    bool focused = false;
    bool disabled = false;
    bool adjustable = false;     /* draws ‹ value › when focused */
    bool strong = false;         /* SemiBold label even when unfocused */
    bool chevron = false;        /* › at the right edge when focused */
    bool soft = false;           /* focus is a surface highlight, not the
                                  * accent: a second-level focus (Settings
                                  * values while the category is active) */
    int  icon = -1;              /* ui::Icon drawn before the label */
};
void menuRow(App& app, float x, float y, float w, float h, const char* text,
             const char* value, const RowStyle& style, u32 alpha = 255);

/* --- artwork ------------------------------------------------------------ */
/* Contain-fits `art` into a surface2 well so no cover is ever cropped or
 * stretched. */
void artWell(App& app, const gfx::Texture& art, float x, float y, float w,
             float h, u32 alpha = 255);
/* Branded fallback: pattern field, RetroShell mark, system icon and
 * abbreviation. Intentional, never an empty frame or a broken image. */
void artFallback(App& app, int systemId, float x, float y, float w, float h,
                 u32 alpha = 255);

/* --- motion ------------------------------------------------------------- */
/* Pixel-snapped value, for anything that moves. */
inline float snap(float v) { return float(int(v + (v < 0.f ? -.5f : .5f))); }

}  // namespace rs::ui
