/*
 * assetgen — RetroShell host-side asset baker (runs on the build machine).
 *
 * Generates the PBP artwork (SPLASH/PIC1/ICON0). Font atlases (.rsf, format
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

static void make_pbp_art(const char* out_dir) {
    char source_path[1024], output_path[1024];
    int sw = 0, sh = 0, channels = 0;

    /* The committed Figma export is exactly 2x PSP resolution. Average each
     * 2x2 block so antialiased type survives the native-resolution bake. */
    snprintf(source_path, sizeof source_path,
             "%s/branding/retroshell-splash-2x.png", out_dir);
    unsigned char* source =
        stbi_load(source_path, &sw, &sh, &channels, 4);
    if (!source || sw != 960 || sh != 544) {
        fprintf(stderr, "assetgen: expected a 960x544 splash at %s\n",
                source_path);
        exit(1);
    }
    Canvas splash = canvas_new(480, 272);
    for (int y = 0; y < splash.h; y++) {
        for (int x = 0; x < splash.w; x++) {
            unsigned char* dst = splash.px + (y * splash.w + x) * 4;
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
    snprintf(output_path, sizeof output_path, "%s/SPLASH.PNG", out_dir);
    stbi_write_png(output_path, splash.w, splash.h, 4, splash.px,
                   splash.w * 4);
    printf("wrote %s\n", output_path);
    snprintf(output_path, sizeof output_path, "%s/PIC1.PNG", out_dir);
    stbi_write_png(output_path, splash.w, splash.h, 4, splash.px,
                   splash.w * 4);
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

    stbi_image_free(source);
    snprintf(output_path, sizeof output_path, "%s/ICON0.PNG", out_dir);
    stbi_write_png(output_path, iw, ih, 4, icon, iw * 4);
    printf("wrote %s\n", output_path);

    stbi_image_free(icon);
    free(splash.px);
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
