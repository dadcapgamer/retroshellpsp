/*
 * fontbake — hinted .rsf font atlases via FreeType (host tool).
 *
 * stb_truetype (tools/assetgen.c) rasterizes unhinted outlines, which at the
 * PSP's 10-18 px sizes leaves stems straddling two pixel columns and reads
 * soft on the LCD. FreeType's hinter snaps stems and x-height to the pixel
 * grid, so the same face comes out crisp. Output is the identical .rsf
 * format parsed by src/frontend/text/font.cpp; the atlases are committed, so
 * only someone changing fonts needs FreeType.
 *
 * Build & run (macOS, Homebrew FreeType):
 *   cc -O2 -o build/fontbake tools/fontbake.c \
 *      $(pkg-config --cflags --libs freetype2)
 *   build/fontbake assets/fonts assets
 *
 * Usage for a single atlas (experiments):
 *   build/fontbake --one <ttf> <px> <mode> <out.rsf>
 *   mode: light | normal | mono
 */
#include <ft2build.h>
#include FT_FREETYPE_H

/* Hinting used for the committed atlases; chosen by eye on the PSP LCD. */
#ifndef HINT_MODE
#define HINT_MODE "normal"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t cp;
    int w, h, left, top, adv;
    unsigned char* bits;
    int ax, ay;            /* atlas position */
} Glyph;

static int build_codepoints(FT_Face face, uint32_t* cps) {
    static const uint32_t extras[] = {
        0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026
    };
    int n = 0;
    for (uint32_t cp = 0x20; cp <= 0x7E; cp++)
        if (FT_Get_Char_Index(face, cp)) cps[n++] = cp;
    for (uint32_t cp = 0xA0; cp <= 0xFF; cp++)
        if (FT_Get_Char_Index(face, cp)) cps[n++] = cp;
    for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++)
        if (FT_Get_Char_Index(face, extras[i])) cps[n++] = extras[i];
    return n;
}

/* `px` matches assetgen's meaning (ascent-to-descent height), so a size
 * keeps the same line metrics whichever baker produced it. */
static void bake(FT_Library lib, const char* ttf, int px, const char* mode,
                 const char* out_path) {
    FT_Face face;
    if (FT_New_Face(lib, ttf, 0, &face)) {
        fprintf(stderr, "fontbake: cannot open %s\n", ttf);
        exit(1);
    }
    const double em = (double)px * face->units_per_EM /
                      (double)(face->ascender - face->descender);
    FT_Set_Char_Size(face, 0, (FT_F26Dot6)(em * 64.0 + 0.5), 72, 72);

    FT_Int32 flags = FT_LOAD_RENDER;
    FT_Render_Mode render = FT_RENDER_MODE_NORMAL;
    if (!strcmp(mode, "light")) {
        flags |= FT_LOAD_TARGET_LIGHT;
        render = FT_RENDER_MODE_LIGHT;
    } else if (!strcmp(mode, "mono")) {
        flags |= FT_LOAD_TARGET_MONO;
        render = FT_RENDER_MODE_MONO;
    } else {
        flags |= FT_LOAD_TARGET_NORMAL;
    }
    (void)render;

    uint32_t cps[512];
    const int n = build_codepoints(face, cps);
    Glyph* g = calloc((size_t)n, sizeof *g);
    for (int i = 0; i < n; i++) {
        if (FT_Load_Char(face, cps[i], flags)) {
            fprintf(stderr, "fontbake: glyph %u failed\n", cps[i]);
            exit(1);
        }
        FT_GlyphSlot s = face->glyph;
        FT_Bitmap* b = &s->bitmap;
        g[i].cp = cps[i];
        g[i].w = (int)b->width;
        g[i].h = (int)b->rows;
        g[i].left = s->bitmap_left;
        g[i].top = s->bitmap_top;
        g[i].adv = (int)((s->advance.x + 32) >> 6);
        g[i].bits = calloc((size_t)(g[i].w * g[i].h) + 1, 1);
        for (int y = 0; y < g[i].h; y++)
            for (int x = 0; x < g[i].w; x++) {
                unsigned v;
                if (b->pixel_mode == FT_PIXEL_MODE_MONO)
                    v = (b->buffer[y * b->pitch + (x >> 3)] >> (7 - (x & 7))) & 1
                            ? 255 : 0;
                else
                    v = b->buffer[y * b->pitch + x];
                g[i].bits[y * g[i].w + x] = (unsigned char)v;
            }
    }

    /* Shelf-pack into the smallest atlas that fits. */
    static const int sizes[][2] = {
        {128, 128}, {256, 128}, {256, 256}, {512, 256}, {512, 512}
    };
    int aw = 0, ah = 0, fit = 0;
    for (size_t sz = 0; sz < sizeof sizes / sizeof sizes[0] && !fit; sz++) {
        aw = sizes[sz][0];
        ah = sizes[sz][1];
        int x = 1, y = 1, row = 0;
        fit = 1;
        for (int i = 0; i < n; i++) {
            if (x + g[i].w + 1 > aw) { x = 1; y += row + 1; row = 0; }
            if (y + g[i].h + 1 > ah) { fit = 0; break; }
            g[i].ax = x;
            g[i].ay = y;
            x += g[i].w + 1;
            if (g[i].h > row) row = g[i].h;
        }
    }
    if (!fit) {
        fprintf(stderr, "fontbake: %s @%d does not fit 512x512\n", ttf, px);
        exit(1);
    }
    unsigned char* atlas = calloc((size_t)(aw * ah), 1);
    for (int i = 0; i < n; i++)
        for (int y = 0; y < g[i].h; y++)
            memcpy(atlas + (g[i].ay + y) * aw + g[i].ax,
                   g[i].bits + y * g[i].w, (size_t)g[i].w);

    const int ascent = (int)((face->size->metrics.ascender + 32) >> 6);
    const int descent = (int)((face->size->metrics.descender - 32) >> 6);
    const int line = (int)((face->size->metrics.height + 32) >> 6);

    FILE* f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "fontbake: cannot write %s\n", out_path); exit(1); }
    uint32_t magic = 0x31465352u; /* "RSF1" */
    uint16_t u16v; int16_t s16v;
    fwrite(&magic, 4, 1, f);
    u16v = (uint16_t)aw; fwrite(&u16v, 2, 1, f);
    u16v = (uint16_t)ah; fwrite(&u16v, 2, 1, f);
    s16v = (int16_t)ascent;  fwrite(&s16v, 2, 1, f);
    s16v = (int16_t)descent; fwrite(&s16v, 2, 1, f);
    s16v = (int16_t)line;    fwrite(&s16v, 2, 1, f);
    u16v = (uint16_t)n; fwrite(&u16v, 2, 1, f);
    u16v = 0;           fwrite(&u16v, 2, 1, f);
    for (int i = 0; i < n; i++) {
        uint32_t cp = g[i].cp;
        fwrite(&cp, 4, 1, f);
        u16v = (uint16_t)g[i].ax; fwrite(&u16v, 2, 1, f);
        u16v = (uint16_t)g[i].ay; fwrite(&u16v, 2, 1, f);
        u16v = (uint16_t)g[i].w;  fwrite(&u16v, 2, 1, f);
        u16v = (uint16_t)g[i].h;  fwrite(&u16v, 2, 1, f);
        s16v = (int16_t)g[i].left;  fwrite(&s16v, 2, 1, f);
        s16v = (int16_t)-g[i].top;  fwrite(&s16v, 2, 1, f);   /* yoff from baseline */
        s16v = (int16_t)g[i].adv;   fwrite(&s16v, 2, 1, f);
        s16v = 0;                   fwrite(&s16v, 2, 1, f);
    }
    fwrite(atlas, 1, (size_t)(aw * ah), f);
    fclose(f);
    printf("baked %-34s %3dpx %-6s %dx%d  %d glyphs\n", out_path, px, mode,
           aw, ah, n);
    for (int i = 0; i < n; i++) free(g[i].bits);
    free(g);
    free(atlas);
    FT_Done_Face(face);
}

int main(int argc, char** argv) {
    FT_Library lib;
    if (FT_Init_FreeType(&lib)) { fprintf(stderr, "fontbake: no FreeType\n"); return 1; }
    if (argc == 6 && !strcmp(argv[1], "--one")) {
        bake(lib, argv[2], atoi(argv[3]), argv[4], argv[5]);
        return 0;
    }
    if (argc != 3) {
        fprintf(stderr, "usage: fontbake <font-dir> <out-dir>\n"
                        "       fontbake --one <ttf> <px> <mode> <out.rsf>\n");
        return 1;
    }
    char ttf[1024], rsf[1024];
    const char* fdir = argv[1];
    const char* out = argv[2];
    /* Keep sizes and roles in step with tools/assetgen.c. */
    struct { const char* face; int px; const char* name; } atlases[] = {
        {"IBMPlexMono-SemiBold.ttf", 18, "font_display"},
        {"IBMPlexMono-SemiBold.ttf", 15, "font_title"},
        {"IBMPlexMono-SemiBold.ttf", 13, "font_body_strong"},
        {"IBMPlexMono-Regular.ttf",  13, "font_body"},
        {"IBMPlexMono-Regular.ttf",  11, "font_small"},
        {"IBMPlexMono-Regular.ttf",  10, "font_tiny"},
    };
    for (size_t i = 0; i < sizeof atlases / sizeof atlases[0]; i++) {
        snprintf(ttf, sizeof ttf, "%s/%s", fdir, atlases[i].face);
        snprintf(rsf, sizeof rsf, "%s/fonts/%s.rsf", out, atlases[i].name);
        bake(lib, ttf, atlases[i].px, HINT_MODE, rsf);
    }
    FT_Done_FreeType(lib);
    return 0;
}
