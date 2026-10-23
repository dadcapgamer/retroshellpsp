/** Theme engine.
 *
 * A theme is a directory ms0:/RETROSHELL/themes/<name>/ containing
 * theme.json and optional assets:
 *
 *   {
 *     "name": "Midnight",
 *     "dark": true,
 *     "colors": { "bg": "#0E1218", "accent": "#4C9AFF", ... },
 *     "background": "bg.png",     // optional 480x272 image
 *     "waves": true,              // optional legacy animated ribbons
 *     "presentation": "gallery", // optional artwork-led layout
 *     "systemArt": { "gb": "system-art/gb.png", ... },
 *     "systemLogos": { "gb": "logos/gb.png", ... }
 *   }
 *
 * Unspecified colors inherit from the built-in Light or Dark palette
 * (chosen by "dark"), so a theme can override a single color. The built-in
 * themes "dark", "graphite", "light" and "mist" always exist and need no
 * files. Asset-bearing presentations are installed as independent folders.
 */
#pragma once

#include "frontend/themes/palette.h"
#include "platform/psp/gu_renderer.h"

#include <string>
#include <vector>

namespace rs::theme {

enum class Presentation : u8 { RetroShell, Gallery };

struct Theme {
    std::string id;        /* "dark", "light", or directory name */
    std::string title;     /* display name */
    Palette palette;
    bool waves = false;
    Presentation presentation = Presentation::RetroShell;
    gfx::Texture background;   /* optional; check .valid() */
    std::vector<gfx::Texture> systemArt;
    std::vector<gfx::Texture> systemLogos;

    void freeAssets() {
        gfx::Renderer::freeTexture(background);
        for (auto& texture : systemArt) gfx::Renderer::freeTexture(texture);
        for (auto& texture : systemLogos) gfx::Renderer::freeTexture(texture);
        systemArt.clear();
        systemLogos.clear();
    }
    bool isGallery() const { return presentation == Presentation::Gallery; }
    const gfx::Texture* artForSystem(int system) const {
        return system >= 0 && system < int(systemArt.size()) &&
                       systemArt[size_t(system)].valid()
                   ? &systemArt[size_t(system)] : nullptr;
    }
    const gfx::Texture* logoForSystem(int system) const {
        return system >= 0 && system < int(systemLogos.size()) &&
                       systemLogos[size_t(system)].valid()
                   ? &systemLogos[size_t(system)] : nullptr;
    }
};

/* Loads built-ins by id, otherwise from the themes directory. Falls back
 * to built-in dark on any error. */
Theme loadTheme(const std::string& id);

/* A theme's colours alone (no background image is loaded), for drawing
 * before the UI is up. Same fallbacks as loadTheme. */
Palette paletteFor(const std::string& id);

/* Discovered theme ids: always starts with the four built-ins. */
std::vector<std::string> availableThemes();

}  // namespace rs::theme
