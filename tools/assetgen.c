/*
 * assetgen — RetroSuite host-side asset baker (runs on the build machine).
 *
 * Bakes TTF fonts into .rsf atlases (format documented below and parsed by
 * src/frontend/text/font.cpp) and generates the PBP artwork (ICON0/PIC1).
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

#define STB_TRUETYPE_IMPLEMENTATION
#include "../external/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"

/* ------------------------------------------------------------------ */

static unsigned char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "assetgen: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "assetgen: short read on %s\n", path); exit(1);
    }
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
}

/* Codepoints baked into every atlas: printable ASCII, Latin-1 supplement,
 * and the typographic punctuation the UI uses. */
static int build_codepoint_list(const stbtt_fontinfo* info, int* cps) {
    static const int extras[] = {
        0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026
    };
    int n = 0, cp;
    for (cp = 0x20; cp <= 0x7E; cp++)
        if (stbtt_FindGlyphIndex(info, cp)) cps[n++] = cp;
    for (cp = 0xA0; cp <= 0xFF; cp++)
        if (stbtt_FindGlyphIndex(info, cp)) cps[n++] = cp;
    for (size_t i = 0; i < sizeof(extras) / sizeof(extras[0]); i++)
        if (stbtt_FindGlyphIndex(info, extras[i])) cps[n++] = extras[i];
    return n;
}

static void bake_font(const char* ttf_path, float px, const char* out_path) {
    long ttf_size;
    unsigned char* ttf = read_file(ttf_path, &ttf_size);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        fprintf(stderr, "assetgen: bad font %s\n", ttf_path); exit(1);
    }

    int cps[512];
    int ncp = build_codepoint_list(&info, cps);

    /* Grow the atlas until everything packs. */
    static const int sizes[][2] = {
        {256, 128}, {256, 256}, {512, 256}, {512, 512}
    };
    unsigned char* atlas = NULL;
    stbtt_packedchar pcd[512];
    int aw = 0, ah = 0, packed = 0;
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]) && !packed; s++) {
        aw = sizes[s][0]; ah = sizes[s][1];
        free(atlas);
        atlas = calloc(1, (size_t)(aw * ah));
        stbtt_pack_context pc;
        stbtt_PackBegin(&pc, atlas, aw, ah, aw, 1, NULL);
        stbtt_pack_range range = {0};
        range.font_size = px;
        range.array_of_unicode_codepoints = cps;
        range.num_chars = ncp;
        range.chardata_for_range = pcd;
        packed = stbtt_PackFontRanges(&pc, ttf, 0, &range, 1);
        stbtt_PackEnd(&pc);
    }
    if (!packed) {
        fprintf(stderr, "assetgen: %s @%.0fpx does not fit 512x512\n",
                ttf_path, px);
        exit(1);
    }

    float scale = stbtt_ScaleForPixelHeight(&info, px);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

    FILE* f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "assetgen: cannot write %s\n", out_path); exit(1); }

    uint32_t magic = 0x31465352u; /* "RSF1" */
    uint16_t u16v; int16_t s16v;
    fwrite(&magic, 4, 1, f);
    u16v = (uint16_t)aw; fwrite(&u16v, 2, 1, f);
    u16v = (uint16_t)ah; fwrite(&u16v, 2, 1, f);
    s16v = (int16_t)(ascent * scale + 0.5f);            fwrite(&s16v, 2, 1, f);
    s16v = (int16_t)(descent * scale - 0.5f);           fwrite(&s16v, 2, 1, f);
    s16v = (int16_t)((ascent - descent + line_gap) * scale + 0.5f);
    fwrite(&s16v, 2, 1, f);
    u16v = (uint16_t)ncp; fwrite(&u16v, 2, 1, f);
    u16v = 0;             fwrite(&u16v, 2, 1, f);

    for (int i = 0; i < ncp; i++) {
        const stbtt_packedchar* c = &pcd[i];
        uint32_t cp = (uint32_t)cps[i];
        fwrite(&cp, 4, 1, f);
        u16v = c->x0;                       fwrite(&u16v, 2, 1, f);
        u16v = c->y0;                       fwrite(&u16v, 2, 1, f);
        u16v = (uint16_t)(c->x1 - c->x0);   fwrite(&u16v, 2, 1, f);
        u16v = (uint16_t)(c->y1 - c->y0);   fwrite(&u16v, 2, 1, f);
        s16v = (int16_t)(c->xoff + (c->xoff < 0 ? -0.5f : 0.5f)); fwrite(&s16v, 2, 1, f);
        s16v = (int16_t)(c->yoff + (c->yoff < 0 ? -0.5f : 0.5f)); fwrite(&s16v, 2, 1, f);
        s16v = (int16_t)(c->xadvance + 0.5f); fwrite(&s16v, 2, 1, f);
        s16v = 0;                             fwrite(&s16v, 2, 1, f);
    }
    fwrite(atlas, 1, (size_t)(aw * ah), f);
    fclose(f);

    printf("baked %-28s %3dpx  %dx%d  %d glyphs\n", out_path, (int)px, aw, ah, ncp);
    free(atlas);
    free(ttf);
}

/* ---------------- PBP artwork ------------------------------------- */

typedef struct { unsigned char* px; int w, h; } Canvas;

static Canvas canvas_new(int w, int h) {
    Canvas c = { calloc(1, (size_t)(w * h * 4)), w, h };
    return c;
}

static void canvas_vgradient(Canvas* c, uint32_t top, uint32_t bottom) {
    for (int y = 0; y < c->h; y++) {
        float t = (float)y / (float)(c->h - 1);
        unsigned char r = (unsigned char)(((top >> 16) & 0xFF) + t * (((bottom >> 16) & 0xFF) - ((top >> 16) & 0xFF)));
        unsigned char g = (unsigned char)(((top >> 8) & 0xFF) + t * (((bottom >> 8) & 0xFF) - ((top >> 8) & 0xFF)));
        unsigned char b = (unsigned char)((top & 0xFF) + t * ((bottom & 0xFF) - (top & 0xFF)));
        for (int x = 0; x < c->w; x++) {
            unsigned char* p = c->px + (y * c->w + x) * 4;
            p[0] = r; p[1] = g; p[2] = b; p[3] = 0xFF;
        }
    }
}

static void canvas_fill_rect(Canvas* c, int x0, int y0, int w, int h, uint32_t rgb) {
    for (int y = y0; y < y0 + h && y < c->h; y++) {
        if (y < 0) continue;
        for (int x = x0; x < x0 + w && x < c->w; x++) {
            if (x < 0) continue;
            unsigned char* p = c->px + (y * c->w + x) * 4;
            p[0] = (rgb >> 16) & 0xFF; p[1] = (rgb >> 8) & 0xFF; p[2] = rgb & 0xFF;
        }
    }
}

static int canvas_text(Canvas* c, const stbtt_fontinfo* font, float px,
                       int x, int baseline, const char* text, uint32_t rgb,
                       float alpha, int measure_only) {
    float scale = stbtt_ScaleForPixelHeight(font, px);
    int pen = x;
    for (const char* s = text; *s; s++) {
        int adv, lsb, gx0, gy0, gx1, gy1;
        stbtt_GetCodepointHMetrics(font, *s, &adv, &lsb);
        if (!measure_only) {
            stbtt_GetCodepointBitmapBox(font, *s, scale, scale, &gx0, &gy0, &gx1, &gy1);
            int gw = gx1 - gx0, gh = gy1 - gy0;
            if (gw > 0 && gh > 0) {
                unsigned char* bmp = malloc((size_t)(gw * gh));
                stbtt_MakeCodepointBitmap(font, bmp, gw, gh, gw, scale, scale, *s);
                for (int yy = 0; yy < gh; yy++) {
                    int py = baseline + gy0 + yy;
                    if (py < 0 || py >= c->h) continue;
                    for (int xx = 0; xx < gw; xx++) {
                        int pxx = pen + gx0 + xx;
                        if (pxx < 0 || pxx >= c->w) continue;
                        float a = alpha * bmp[yy * gw + xx] / 255.0f;
                        unsigned char* p = c->px + (py * c->w + pxx) * 4;
                        p[0] = (unsigned char)(p[0] + a * (((rgb >> 16) & 0xFF) - p[0]));
                        p[1] = (unsigned char)(p[1] + a * (((rgb >> 8) & 0xFF) - p[1]));
                        p[2] = (unsigned char)(p[2] + a * ((rgb & 0xFF) - p[2]));
                    }
                }
                free(bmp);
            }
        }
        pen += (int)(adv * scale + 0.5f);
    }
    return pen - x;
}

static void make_pbp_art(const char* semibold_path, const char* out_dir) {
    long size;
    unsigned char* ttf = read_file(semibold_path, &size);
    stbtt_fontinfo font;
    stbtt_InitFont(&font, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
    char path[1024];

    /* ICON0 — 144x80 tile shown in the XMB game list. */
    Canvas icon = canvas_new(144, 80);
    canvas_vgradient(&icon, 0x141A24, 0x0C1017);
    canvas_fill_rect(&icon, 0, 76, 144, 4, 0x2E7CF6);
    int w = canvas_text(&icon, &font, 34, 0, 0, "RS", 0, 0, 1);
    canvas_text(&icon, &font, 34, (144 - w) / 2, 40, "RS", 0xFFFFFF, 1.0f, 0);
    w = canvas_text(&icon, &font, 13, 0, 0, "RetroSuite", 0, 0, 1);
    canvas_text(&icon, &font, 13, (144 - w) / 2, 62, "RetroSuite", 0x7FA8E8, 1.0f, 0);
    snprintf(path, sizeof path, "%s/ICON0.PNG", out_dir);
    stbi_write_png(path, icon.w, icon.h, 4, icon.px, icon.w * 4);
    printf("wrote %s\n", path);

    /* PIC1 — 480x272 XMB background while the game is highlighted. */
    Canvas pic = canvas_new(480, 272);
    canvas_vgradient(&pic, 0x101722, 0x090C12);
    w = canvas_text(&pic, &font, 30, 0, 0, "RetroSuite", 0, 0, 1);
    canvas_text(&pic, &font, 30, 480 - w - 24, 236, "RetroSuite", 0xE8EEF8, 0.92f, 0);
    canvas_fill_rect(&pic, 480 - w - 24, 246, w, 3, 0x2E7CF6);
    snprintf(path, sizeof path, "%s/PIC1.PNG", out_dir);
    stbi_write_png(path, pic.w, pic.h, 4, pic.px, pic.w * 4);
    printf("wrote %s\n", path);

    free(icon.px);
    free(pic.px);
    free(ttf);
}

/* ------------------------------------------------------------------ */

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: assetgen <font-dir> <out-dir>\n");
        return 1;
    }
    const char* fdir = argv[1];
    const char* out = argv[2];
    char ttf[1024], rsf[1024];

    snprintf(ttf, sizeof ttf, "%s/Inter-SemiBold.ttf", fdir);
    snprintf(rsf, sizeof rsf, "%s/fonts/font_title.rsf", out);
    bake_font(ttf, 26, rsf);
    snprintf(rsf, sizeof rsf, "%s/fonts/font_large.rsf", out);
    bake_font(ttf, 19, rsf);

    snprintf(ttf, sizeof ttf, "%s/Inter-Regular.ttf", fdir);
    snprintf(rsf, sizeof rsf, "%s/fonts/font_body.rsf", out);
    bake_font(ttf, 15, rsf);
    snprintf(rsf, sizeof rsf, "%s/fonts/font_small.rsf", out);
    bake_font(ttf, 12, rsf);

    snprintf(ttf, sizeof ttf, "%s/Inter-SemiBold.ttf", fdir);
    make_pbp_art(ttf, out);
    return 0;
}
