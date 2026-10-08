#include "frontend/splash.h"

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
    /* SPLASH_MASK.PNG (tools/assetgen.c): red = mark + title coverage,
     * green = tagline coverage, over the background. */
    int channels = 0;
    stbi_uc* px = stbi_load_from_memory(rs_asset_splash_mask_png,
                                        int(rs_asset_splash_mask_png_len),
                                        &w, &h, &channels, 4);
    if (!px) return nullptr;
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
        for (int c = 0; c < 3; c++)
            d[c] = static_cast<stbi_uc>(
                (bg[c] * (255 - a - b) + fg[c] * a + tag[c] * b + 127) / 255);
        d[3] = 255;
    }
    return px;
}

}  // namespace rs::splash
