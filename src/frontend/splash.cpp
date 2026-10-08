#include "frontend/splash.h"

#include "rs_asset_splash_icons_png.h"
#include "rs_asset_splash_mask_png.h"
#include "stb_image.h"

namespace rs::splash {

namespace {
/* Every theme's tagline is the same warm grey (Figma "Splash screens"). */
constexpr u32 TAGLINE = 0xA09886;

unsigned char channel(u32 rgb, int shift) {
    return static_cast<unsigned char>((rgb >> shift) & 0xFF);
}
}  // namespace

unsigned char* compose(const theme::Palette& p, int& w, int& h) {
    /* tools/assetgen.c bakes both layers from the Figma "splash - new"
     * frame: SPLASH_ICONS is the four faded console icons (straight alpha),
     * SPLASH_MASK the mark (red) and tagline (green) coverage. */
    int channels = 0;
    stbi_uc* px = stbi_load_from_memory(rs_asset_splash_mask_png,
                                        int(rs_asset_splash_mask_png_len),
                                        &w, &h, &channels, 4);
    if (!px) return nullptr;
    int iw = 0, ih = 0;
    stbi_uc* icons = stbi_load_from_memory(rs_asset_splash_icons_png,
                                           int(rs_asset_splash_icons_png_len),
                                           &iw, &ih, &channels, 4);
    if (icons && (iw != w || ih != h)) {
        stbi_image_free(icons);
        icons = nullptr;
    }
    /* Palette colours are ABGR (rsHex); the tagline above is plain RGB. */
    const int bg[3] = {int(p.bg & 0xFF), int((p.bg >> 8) & 0xFF),
                       int((p.bg >> 16) & 0xFF)};
    const int fg[3] = {int(p.textPrimary & 0xFF),
                       int((p.textPrimary >> 8) & 0xFF),
                       int((p.textPrimary >> 16) & 0xFF)};
    const int tag[3] = {channel(TAGLINE, 16), channel(TAGLINE, 8),
                        channel(TAGLINE, 0)};
    for (int i = 0; i < w * h; i++) {
        stbi_uc* d = px + i * 4;
        const int a = d[0], b = d[1];
        int base[3] = {bg[0], bg[1], bg[2]};
        if (icons) {
            const stbi_uc* s = icons + i * 4;
            const int ia = s[3];
            for (int c = 0; c < 3; c++)
                base[c] = (base[c] * (255 - ia) + s[c] * ia + 127) / 255;
        }
        for (int c = 0; c < 3; c++)
            d[c] = static_cast<stbi_uc>(
                (base[c] * (255 - a - b) + fg[c] * a + tag[c] * b + 127) / 255);
        d[3] = 255;
    }
    if (icons) stbi_image_free(icons);
    return px;
}

}  // namespace rs::splash
