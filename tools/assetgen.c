/*
 * assetgen — RetroShell host-side asset baker (runs on the build machine).
 *
 * Generates the PBP artwork (SPLASH_MASK/SPLASH_ICONS/PIC1/ICON0). Font atlases (.rsf, format
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

/* Area-resamples an RGBA master (premultiplied while averaging) to dst x dst
 * pixels and composites it at (ox, oy) of a 480x272 RGBA layer with
 * `opacity`. */
static void place_icon(Canvas* layer, const char* path, int ox, int oy, int dst,
                       float opacity) {
    int w = 0, h = 0, ch = 0;
    unsigned char* src = stbi_load(path, &w, &h, &ch, 4);
    if (!src || w != h) {
        fprintf(stderr, "assetgen: expected a square icon master at %s\n", path);
        exit(1);
    }
    const double scale = (double)w / dst;
    for (int y = 0; y < dst; y++) {
        for (int x = 0; x < dst; x++) {
            double acc[4] = {0, 0, 0, 0}, area = 0;
            const double x0 = x * scale, x1 = (x + 1) * scale;
            const double y0 = y * scale, y1 = (y + 1) * scale;
            for (int sy = (int)y0; sy < (int)(y1 + 0.999) && sy < h; sy++) {
                const double wy = (sy + 1 < y1 ? sy + 1 : y1) - (sy > y0 ? sy : y0);
                for (int sx = (int)x0; sx < (int)(x1 + 0.999) && sx < w; sx++) {
                    const double wx =
                        (sx + 1 < x1 ? sx + 1 : x1) - (sx > x0 ? sx : x0);
                    const unsigned char* p = src + (sy * w + sx) * 4;
                    const double a = p[3] / 255.0, k = wx * wy;
                    for (int c = 0; c < 3; c++) acc[c] += p[c] * a * k;
                    acc[3] += a * k;
                    area += k;
                }
            }
            if (acc[3] <= 0) continue;
            unsigned char* d = layer->px + ((oy + y) * layer->w + ox + x) * 4;
            for (int c = 0; c < 3; c++)
                d[c] = (unsigned char)(acc[c] / acc[3] + 0.5);
            d[3] = (unsigned char)(acc[3] / area * opacity * 255.0 + 0.5);
        }
    }
    stbi_image_free(src);
}

static void make_pbp_art(const char* out_dir) {
    char source_path[1024], output_path[1024];
    int channels = 0;
    /* XMB background (PIC1): one fixed frame, independent of the splash. */
    snprintf(source_path, sizeof source_path,
             "%s/branding/retroshell-xmb-background-2x.png", out_dir);
    Canvas pic1 = bake_half(source_path);
    snprintf(output_path, sizeof output_path, "%s/PIC1.PNG", out_dir);
    stbi_write_png(output_path, pic1.w, pic1.h, 4, pic1.px, pic1.w * 4);
    printf("wrote %s\n", output_path);
    free(pic1.px);

    /* The startup splash (Figma "splash - new") is themed at run time by
     * src/frontend/splash.cpp from two layers baked here:
     *  - SPLASH_MASK: red = mark coverage, green = tagline coverage (the
     *    tagline's 80% opacity is folded in), read from the Figma frame;
     *  - SPLASH_ICONS: the four console icons, from RetroShell's own masters,
     *    at the frame's positions and opacities (straight alpha). */
    snprintf(source_path, sizeof source_path,
             "%s/branding/retroshell-splash-2x.png", out_dir);
    int sw = 0, sh = 0;
    unsigned char* src = stbi_load(source_path, &sw, &sh, &channels, 4);
    if (!src || sw != 960 || sh != 544) {
        fprintf(stderr, "assetgen: expected a 960x544 splash at %s\n",
                source_path);
        exit(1);
    }
    static const int BG[3] = {0x13, 0x15, 0x17};     /* frame background */
    static const int FG[3] = {0xF2, 0xF1, 0xEE};     /* mark */
    static const int TAG[3] = {0xA0, 0x98, 0x86};    /* tagline grey */
    Canvas mask = canvas_new(480, 272);
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            unsigned mark = 0, tag = 0;
            for (int yy = 0; yy < 2; yy++)
                for (int xx = 0; xx < 2; xx++) {
                    const int sy = y * 2 + yy, sx = x * 2 + xx;
                    const unsigned char* p = src + (sy * sw + sx) * 4;
                    /* 2x frame boxes: mark 390..570 x 182..362, tagline
                     * rows 366..412 (clear of the icons either side). */
                    if (sx >= 386 && sx < 574 && sy >= 178 && sy < 366)
                        mark += coverage(p, BG, FG);
                    else if (sy >= 366 && sy < 414 && sx >= 180 && sx < 780)
                        tag += coverage(p, BG, TAG);
                }
            unsigned char* d = mask.px + (y * 480 + x) * 4;
            d[0] = (unsigned char)((mark + 2) / 4);
            d[1] = (unsigned char)((tag + 2) / 4);
            d[2] = 0;
            d[3] = 255;
        }
    }
    stbi_image_free(src);
    snprintf(output_path, sizeof output_path, "%s/SPLASH_MASK.PNG", out_dir);
    unsigned char* rgb = malloc(480 * 272 * 3);
    for (int i = 0; i < 480 * 272; i++)
        memcpy(rgb + i * 3, mask.px + i * 4, 3);
    stbi_write_png(output_path, 480, 272, 3, rgb, 480 * 3);
    free(rgb);
    free(mask.px);
    printf("wrote %s\n", output_path);

    /* 2x frame: 120 px icons at x 118.4 / 254.2 / 585.8 / 721.6, y 212;
     * the outer pair at 20% opacity, the inner pair at 40%. */
    static const struct { const char* master; int x; float opacity; } ICONS[] = {
        {"game-boy-color", 59, 0.2f}, {"snes", 127, 0.4f},
        {"game-gear", 293, 0.4f},     {"pc-engine", 361, 0.2f},
    };
    Canvas icons = canvas_new(480, 272);
    for (int i = 0; i < 4; i++) {
        snprintf(source_path, sizeof source_path,
                 "%s/icons/consoles/%s-192.png", out_dir, ICONS[i].master);
        place_icon(&icons, source_path, ICONS[i].x, 106, 60, ICONS[i].opacity);
    }
    snprintf(output_path, sizeof output_path, "%s/SPLASH_ICONS.PNG", out_dir);
    stbi_write_png(output_path, 480, 272, 4, icons.px, 480 * 4);
    free(icons.px);
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
