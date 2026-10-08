/* The startup splash (shown after X on the XMB while RetroShell loads),
 * coloured for the theme in use. */
#pragma once

#include "frontend/themes/palette.h"

namespace rs::splash {

/* Builds the 480x272 RGBA splash for `p`: its background with the faded
 * console icons, the mark in its primary text colour, and the tagline in
 * the brand's warm grey. Free with stbi_image_free. nullptr on failure. */
unsigned char* compose(const theme::Palette& p, int& w, int& h);

}  // namespace rs::splash
