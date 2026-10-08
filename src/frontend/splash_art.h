/* The startup splash for the theme in use (assets/SPLASH_<THEME>.PNG, baked
 * by tools/assetgen.c from the Figma "Splash screens" frames). Each is drawn
 * as-is over its theme's background colour. A custom theme gets the Dark
 * splash. */
#pragma once

#include "rs_common.h"

#include "rs_asset_splash_dark_png.h"
#include "rs_asset_splash_graphite_png.h"
#include "rs_asset_splash_light_png.h"
#include "rs_asset_splash_mist_png.h"

#include <string>

namespace rs::splash {

struct Art {
    const unsigned char* png;
    unsigned int len;
    u32 bg;              /* the theme's background, behind the image */
};

inline Art forTheme(const std::string& id) {
    if (id == "graphite")
        return {rs_asset_splash_graphite_png, rs_asset_splash_graphite_png_len,
                rsHex(0x131517)};
    if (id == "light")
        return {rs_asset_splash_light_png, rs_asset_splash_light_png_len,
                rsHex(0xF2EFE7)};
    if (id == "mist")
        return {rs_asset_splash_mist_png, rs_asset_splash_mist_png_len,
                rsHex(0xEDF1F5)};
    return {rs_asset_splash_dark_png, rs_asset_splash_dark_png_len,
            rsHex(0x081828)};
}

}  // namespace rs::splash
