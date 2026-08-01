/** Theme engine.
 *
 * A theme is a directory ms0:/RETROSHELL/themes/<name>/ containing
 * theme.json and optional assets:
 *
 *   {
 *     "name": "Midnight",
 *     "dark": true,
 *     "colors": { "bgTop": "#0E1218", "accent": "#4C9AFF", ... },
 *     "background": "bg.png",     // optional 480x272 image
 *     "waves": true               // optional legacy animated ribbons
 *   }
 *
 * Unspecified colors inherit from the built-in Light or Dark palette
 * (chosen by "dark"), so a theme can override a single color. The built-in
 * themes "dark" and "light" always exist and need no files.
 */
#pragma once

#include "frontend/themes/palette.h"
#include "platform/psp/gu_renderer.h"

#include <string>
#include <vector>

namespace rs::theme {

struct Theme {
    std::string id;        /* "dark", "light", or directory name */
    std::string title;     /* display name */
    Palette palette;
    bool waves = false;
    gfx::Texture background;   /* optional; check .valid() */

    void freeAssets() { gfx::Renderer::freeTexture(background); }
};

/* Loads built-ins by id, otherwise from the themes directory. Falls back
 * to built-in dark on any error. */
Theme loadTheme(const std::string& id);

/* Discovered theme ids: always starts with "dark", "light". */
std::vector<std::string> availableThemes();

}  // namespace rs::theme
