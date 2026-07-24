#include "platform/psp/vram.h"

#include <pspge.h>

namespace rs::gfx::vram {

namespace {
u8* s_base    = nullptr;   /* VRAM base (absolute, uncached-view not needed) */
u32 s_size    = 0;
u32 s_cursor  = 0;         /* next free byte, relative to base */
u32 s_high    = 0;
}  // namespace

void init() {
    s_base   = static_cast<u8*>(sceGeEdramGetAddr());
    s_size   = sceGeEdramGetSize();
    s_cursor = FB1_OFFSET + FB_BYTES;
    s_high   = s_cursor;
}

void* alloc(u32 size, u32 align) {
    u32 start = (s_cursor + align - 1) & ~(align - 1);
    if (start + size > s_size) return nullptr;
    s_cursor = start + size;
    if (s_cursor > s_high) s_high = s_cursor;
    return s_base + start;
}

void freeAll()      { s_cursor = FB1_OFFSET + FB_BYTES; }
u32  available()    { return s_size - s_cursor; }
u32  highWater()    { return s_high; }

}  // namespace rs::gfx::vram
