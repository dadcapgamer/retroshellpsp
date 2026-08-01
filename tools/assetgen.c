/*
 * assetgen — RetroShell host-side asset baker (runs on the build machine).
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
#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"
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

    make_pbp_art(out);
    return 0;
}
