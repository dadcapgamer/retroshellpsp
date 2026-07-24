/** Simple bump allocator for PSP VRAM (2MB eDRAM).
 *
 * The first two slots are reserved for the 16-bit draw/display buffers; the
 * remainder holds textures. free-all semantics only: texture VRAM is
 * reclaimed wholesale when the frontend evicts assets before launching a
 * core, never piecemeal.
 */
#pragma once

#include "rs_common.h"

namespace rs::gfx::vram {

/* Byte offsets of the two framebuffers, relative to VRAM base (what
 * sceGuDrawBuffer expects). 512-pixel stride, 16bpp. */
constexpr u32 FB_STRIDE  = 512;
constexpr u32 FB_BYTES   = FB_STRIDE * 272 * 2;
constexpr u32 FB0_OFFSET = 0;
constexpr u32 FB1_OFFSET = FB_BYTES;

void  init();
void* alloc(u32 size, u32 align = 16);  /* absolute pointer, or nullptr */
void  freeAll();                        /* reset texture region          */

/* Persistent boot assets (fonts, primitive masks) live below the boot
 * mark; theme/boxart textures above it are evicted before a core runs. */
void  setBootMark();
void  freeToBootMark();

u32   available();
u32   highWater();

}  // namespace rs::gfx::vram
