/* The PSP system on-screen keyboard (sceUtilityOsk), run modally. */
#pragma once

#include "platform/psp/gu_renderer.h"

#include <string>

namespace rs::osk {

enum class Result { Accepted, Cancelled, Failed };

/* Shows the system keyboard titled `title`, prefilled with `initial`
 * (UTF-8), until the player confirms or cancels. Each frame `drawBackground`
 * draws the screen behind it into an open frame. On Accepted, `out` holds the
 * text (UTF-8, at most `maxChars` characters). Failed means the keyboard
 * could not start; nothing was shown. */
Result run(gfx::Renderer& renderer, const char* title,
           const std::string& initial, int maxChars, std::string& out,
           void (*drawBackground)(void*), void* ctx);

}  // namespace rs::osk
