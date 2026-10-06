/** 1-bit pixel icons, 11x11, drawn as horizontal runs of rects: integer
 * aligned, tinted by colour, no textures and no anti-aliasing, so they stay
 * as crisp on the LCD as the type beside them. */
#pragma once

#include "platform/psp/gu_renderer.h"
#include "rs_common.h"

namespace rs::ui {

enum class Icon : u8 {
    Play, Star, Info, Return, Player, Card, Gear, Gauge, Speaker, Library,
    Count
};

constexpr float ICON_SIZE = 11.f;

/* (x,y) is the top-left of the 11x11 cell. */
void icon(gfx::Renderer& r, Icon which, float x, float y, u32 color);

}  // namespace rs::ui
