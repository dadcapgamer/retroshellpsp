/*
 * assetgen — RetroShell host-side asset baker (runs on the build machine).
 *
 * Generates the PBP artwork (SPLASH_MASK/PIC1/ICON0). Font atlases (.rsf, format
 * below, parsed by src/frontend/text/font.cpp) are baked by tools/fontbake.c.
 * Outputs are committed to the repo so contributors don't need this tool;
 * rerun it only when changing fonts or artwork.
 *
 * Build & run:
 *   cc -O2 -o assetgen tools/assetgen.c -lm
 *   ./assetgen assets/fonts assets
 *
 * .rsf layout (little endian):
 *   u32 magic "RSF1"
 *   u16 atlas_w, atlas_h
 *   s16 ascent, descent, line_height        (pixels; descent is negative)
 *   u16 glyph_count, reserved
 *   glyph_count * { u32 cp; u16 x,y,w,h; s16 xoff,yoff,xadv,reserved }
 *   atlas_w*atlas_h bytes of 8-bit coverage
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"

/* ------------------------------------------------------------------ */

/* ---------------- PBP artwork ------------------------------------- */

typedef struct { unsigned char* px; int w, h; } Canvas;

static Canvas canvas_new(int w, int h) {
    Canvas c = { calloc(1, (size_t)(w * h * 4)), w, h };
    return c;
}

/* Average each 2x2 block of a 960x544 Figma export into a 480x272 canvas,
 * so antialiased type survives the native-resolution bake. */
static Canvas bake_half(const char* source_path) {
    int sw = 0, sh = 0, channels = 0;
    unsigned char* source = stbi_load(source_path, &sw, &sh, &channels, 4);
    if (!source || sw != 960 || sh != 544) {
        fprintf(stderr, "assetgen: expected a 960x544 splash at %s\n",
                source_path);
        exit(1);
    }
    Canvas out = canvas_new(480, 272);
    for (int y = 0; y < out.h; y++) {
        for (int x = 0; x < out.w; x++) {
            unsigned char* dst = out.px + (y * out.w + x) * 4;
            for (int c = 0; c < 4; c++) {
                unsigned sum = 0;
                for (int yy = 0; yy < 2; yy++)
                    for (int xx = 0; xx < 2; xx++)
                        sum += source[
                            (((y * 2 + yy) * sw + x * 2 + xx) * 4) + c];
                dst[c] = (unsigned char)((sum + 2) / 4);
            }
        }
    }
    stbi_image_free(source);
    return out;
}

/* Coverage (0..255) of `p` along the line from `bg` to `fg`. */
static unsigned char coverage(const unsigned char* p, const int bg[3],
                              const int fg[3]) {
    long num = 0, den = 0;
    for (int c = 0; c < 3; c++) {
        num += (long)(p[c] - bg[c]) * (fg[c] - bg[c]);
        den += (long)(fg[c] - bg[c]) * (fg[c] - bg[c]);
    }
    long v = den ? (num * 255 + den / 2) / den : 0;
    return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
}

static void make_pbp_art(const char* out_dir) {
    char source_path[1024], output_path[1024];
    int channels = 0;
    /* XMB background (PIC1): its own Figma frame (the mark between faded
     * console icons), independent of the startup splash. */
    snprintf(source_path, sizeof source_path,
             "%s/branding/retroshell-xmb-background-2x.png", out_dir);
    Canvas pic1 = bake_half(source_path);
    snprintf(output_path, sizeof output_path, "%s/PIC1.PNG", out_dir);
    stbi_write_png(output_path, pic1.w, pic1.h, 4, pic1.px, pic1.w * 4);
    printf("wrote %s\n", output_path);
    free(pic1.px);

    /* The startup splash is themed at run time (src/frontend/splash.cpp):
     * one mask, baked from the Dark Figma frame, holds the mark + title
     * coverage in red and the tagline's in green; the app fills the theme's
     * background and tints them with its primary text colour and the brand
     * tagline grey. */
    snprintf(source_path, sizeof source_path,
             "%s/branding/retroshell-splash-2x.png", out_dir);
    int sw = 0, sh = 0;
    unsigned char* src = stbi_load(source_path, &sw, &sh, &channels, 4);
    if (!src || sw != 960 || sh != 544) exit(1);
    static const int BG[3] = {0x08, 0x18, 0x28};     /* Dark background */
    static const int FG[3] = {0xF4, 0xF1, 0xE8};     /* Dark primary text */
    static const int TAG[3] = {0xA0, 0x98, 0x86};    /* tagline grey */
    const int TAGLINE_TOP = 350;                     /* 2x rows */
    Canvas mask = canvas_new(480, 272);
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            unsigned sum = 0;
            for (int yy = 0; yy < 2; yy++)
                for (int xx = 0; xx < 2; xx++) {
                    const int sy = y * 2 + yy, sx = x * 2 + xx;
                    const unsigned char* p = src + (sy * sw + sx) * 4;
                    sum += coverage(p, BG, sy < TAGLINE_TOP ? FG : TAG);
                }
            unsigned char* d = mask.px + (y * 480 + x) * 4;
            const unsigned char a = (unsigned char)((sum + 2) / 4);
            d[0] = y * 2 < TAGLINE_TOP ? a : 0;
            d[1] = y * 2 < TAGLINE_TOP ? 0 : a;
            d[2] = 0;
            d[3] = 255;
        }
    }
    stbi_image_free(src);
    snprintf(output_path, sizeof output_path, "%s/SPLASH_MASK.PNG", out_dir);
    /* RGB is enough; three channels compress smaller than four. */
    unsigned char* rgb = malloc(480 * 272 * 3);
    for (int i = 0; i < 480 * 272; i++)
        memcpy(rgb + i * 3, mask.px + i * 4, 3);
    stbi_write_png(output_path, 480, 272, 3, rgb, 480 * 3);
    free(rgb);
    free(mask.px);
    printf("wrote %s\n", output_path);

    /* ICON0 is authored as its own native-resolution Figma frame. Treat that
     * export as the canonical source so a later font or splash bake cannot
     * silently redraw the XMB identity. */
    snprintf(source_path, sizeof source_path, "%s/xmb thumbnail.png", out_dir);
    int iw = 0, ih = 0;
    unsigned char* icon = stbi_load(source_path, &iw, &ih, &channels, 4);
    if (!icon || iw != 144 || ih != 80) {
        fprintf(stderr, "assetgen: expected a 144x80 XMB thumbnail at %s\n",
                source_path);
        exit(1);
    }

    snprintf(output_path, sizeof output_path, "%s/ICON0.PNG", out_dir);
    stbi_write_png(output_path, iw, ih, 4, icon, iw * 4);
    printf("wrote %s\n", output_path);

    stbi_image_free(icon);
}

/* ------------------------------------------------------------------ */

int main(int argc, char** argv) {
    /* "--fonts-only" skips the PBP artwork bake, which needs source frames
     * that are not committed; use it when only the .rsf atlases change. */
    const int fonts_only = argc == 4 && strcmp(argv[3], "--fonts-only") == 0;
    if (argc != 3 && !fonts_only) {
        fprintf(stderr,
                "usage: assetgen <font-dir> <out-dir> [--fonts-only]\n");
        return 1;
    }
    const char* fdir = argv[1];
    const char* out = argv[2];
    /* Font atlases are baked by tools/fontbake.c (FreeType hinting keeps
     * small sizes crisp on the LCD); this tool only bakes the PBP artwork. */
    (void)fdir;
    if (fonts_only) {
        fprintf(stderr, "assetgen: fonts are baked by tools/fontbake.c\n");
        return 0;
    }

    make_pbp_art(out);
    return 0;
}
