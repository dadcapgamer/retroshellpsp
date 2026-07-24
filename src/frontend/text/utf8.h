/** Minimal UTF-8 decoding for text rendering. */
#pragma once

#include "rs_common.h"

namespace rs::text {

/* Decodes one codepoint starting at *s, advances *s past it. Invalid bytes
 * decode as U+FFFD-ish (returns '?') and advance one byte, so bad input can
 * never wedge the renderer. */
inline u32 utf8Next(const char** s) {
    const u8* p = reinterpret_cast<const u8*>(*s);
    const u8 c = p[0];
    if (c < 0x80) { *s += 1; return c; }
    if ((c >> 5) == 0x6 && (p[1] & 0xC0) == 0x80) {
        *s += 2;
        return (u32(c & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if ((c >> 4) == 0xE && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *s += 3;
        return (u32(c & 0x0F) << 12) | (u32(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    if ((c >> 3) == 0x1E && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *s += 4;
        return (u32(c & 0x07) << 18) | (u32(p[1] & 0x3F) << 12) |
               (u32(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    *s += 1;
    return '?';
}

}  // namespace rs::text
