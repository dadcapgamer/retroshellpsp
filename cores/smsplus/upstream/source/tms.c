/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   TMS9918 and legacy video mode support.
 *
 ******************************************************************************/

/*
 * See git commit history for more information.
 * - Gameblabla
 * March 13th 2019 : Minor fixes as part of the CrabZ80's revert. (mostly whitepacing but the TMS code was also broken to some extent)
 * March 7th 2019 : Some whitepacing and changing variables to c99 datatypes.
*/

#include "shared.h"

int32_t text_counter;               /* Text offset counter */

static uint8_t tms_lookup[16][256][2];   /* Expand BD, PG data into 8-bit pixels (G1,G2) */
static uint8_t mc_lookup[16][256][8];    /* Expand BD, PG data into 8-bit pixels (MC) */
static uint8_t txt_lookup[256][2];       /* Expand BD, PG data into 8-bit pixels (TX) */
static uint8_t bp_expand[256][8];        /* Expand PG data into 8-bit pixels */
static uint8_t tms_obj_lut[16*256];      /* Look up priority between SG and display pixels */

static const uint8_t diff_mask[]  = {0x07, 0x07, 0x0F, 0x0F};
static const uint8_t name_mask[]  = {0xFF, 0xFF, 0xFC, 0xFC};
static const uint8_t diff_shift[] = {0, 1, 0, 1};
static const uint8_t size_tab[]   = {8, 16, 16, 32};

/* Internally latched sprite data in the VDP */
typedef struct {
    int32_t xpos;
    uint8_t attr;
    uint8_t sg[2];
} tms_sprite;

static tms_sprite sprites[4];
static int32_t sprites_found;

static void render_bg_m0(int32_t line);
static void render_bg_m1(int32_t line);
static void render_bg_m1x(int32_t line);
static void render_bg_inv(int32_t line);
static void render_bg_m3(int32_t line);
static void render_bg_m3x(int32_t line);
static void render_bg_m2(int32_t line);

void parse_line(int32_t line)
{
    int32_t yp, i;
    int32_t mode = vdp.reg[1] & 3;
    int32_t size = size_tab[mode];
    int32_t diff, name;
    uint8_t *sa, *sg;
    tms_sprite *p;

    /* Reset # of sprites found */
    sprites_found = 0;

    /* Parse sprites */
    for(i = 0; i < 32; i++)
    {
        /* Point32_t to current sprite in SA and our current sprite record */
        p = &sprites[sprites_found];
        sa = &vdp.vram[vdp.sa + (i << 2)];

        /* Fetch Y coordinate */
        yp = sa[0];

        /* Check for end marker */
        if(yp == 0xD0)
            goto parse_end;

        /* Wrap Y position */
        if(yp > 0xE0)
            yp -= 256;

        /* Check if sprite falls on following line */
        if(line >= yp && line < (yp + size))
        {
            /* Sprite overflow on this line */
            if(sprites_found == 4)
            {
                /* Set 5S and abort parsing */
                vdp.status |= 0x40;
                goto parse_end;
            }

            /* Fetch X position */
            p->xpos = sa[1];

            /* Fetch name */
            name = sa[2] & name_mask[mode];

            /* Load attribute into attribute storage */
            p->attr = sa[3];

            /* Apply early clock bit */
            if(p->attr & 0x80)
                p->xpos -= 32;

            /* Calculate offset in pattern */
            diff = ((line - yp) >> diff_shift[mode]) & diff_mask[mode];

            /* Insert additional name bit for 16-pixel tall sprites */
            if(diff & 8)
                name |= 1;

            /* Fetch SG data */
            sg = &vdp.vram[vdp.sg | (name << 3) | (diff & 7)];
            p->sg[0] = sg[0x00];
            p->sg[1] = sg[0x10];

            /* Bump found sprite count */
            ++sprites_found;
        }
    }
parse_end:

    /* Insert number of last sprite entry processed */
    vdp.status = (vdp.status & 0xE0) | (i & 0x1F);
}

void render_obj_tms(int32_t line)
{
    int32_t i, x = 0;
    int32_t size, start, end, mode;
    uint8_t *lb, *lut, *ex[2];
    tms_sprite *p;

    mode = vdp.reg[1] & 3;
    size = size_tab[mode];

    /* Render sprites */
    for(i = 0; i < sprites_found; i++)
    {
        p = &sprites[i];
        lb = &linebuf[p->xpos];
        lut = &tms_obj_lut[(p->attr & 0x0F) << 8];

        /* Point32_t to expanded PG data */
        ex[0] = bp_expand[p->sg[0]];
        ex[1] = bp_expand[p->sg[1]];

        /* Clip left edge */
        if(p->xpos < 0)
            start = 0 - p->xpos;
        else
            start = 0;

        /* Clip right edge */
        if(p->xpos > 256 - size)
            end = 256 - p->xpos;
        else
            end = size;

        /* Render sprite line */
        switch(mode)
        {
            case 0: /* 8x8 */
                for(x = start; x < end; x++) {
                    if(ex[0][x])
                    {
                        /* Check sprite collision */
                        if ((lb[x] & 0x40) && !(vdp.status & 0x20))
                        {
                            /* pixel-accurate SPR_COL flag */
                            vdp.status |= 0x20;
                            vdp.spr_col = (line << 8) | ((p->xpos + x + 13) >> 1);
                        }
                        lb[x] = lut[lb[x]];
                    }
                }
                break;

            case 1: /* 8x8 zoomed */
                for(x = start; x < end; x++) {                   
                    if(ex[0][x >> 1])
                    {
                        /* Check sprite collision */
                        if ((lb[x] & 0x40) && !(vdp.status & 0x20))
                        {
                            /* pixel-accurate SPR_COL flag */
                            vdp.status |= 0x20;
                            vdp.spr_col = (line << 8) | ((p->xpos + x + 13) >> 1);
                        }
                        lb[x] = lut[lb[x]];
                    }
                }
                break;

            case 2: /* 16x16 */
                for(x = start; x < end; x++) {
                    if(ex[(x >> 3) & 1][x & 7])
                    {
                        /* Check sprite collision */
                        if ((lb[x] & 0x40) && !(vdp.status & 0x20))
                        {
                            /* pixel-accurate SPR_COL flag */
                            vdp.status |= 0x20;
                            vdp.spr_col = (line << 8) | ((p->xpos + x + 13) >> 1);
                        }
                        lb[x] = lut[lb[x]];
                    }
                }
                break;

            case 3: /* 16x16 zoomed */
                for(x = start; x < end; x++) {
                    if(ex[(x >> 4) & 1][(x >> 1) & 7])
                    {
                        /* Check sprite collision */
                        if ((lb[x] & 0x40) && !(vdp.status & 0x20))
                        {
                            /* pixel-accurate SPR_COL flag */
                            vdp.status |= 0x20;
                            vdp.spr_col = (line << 8) | ((p->xpos + x + 13) >> 1);
                        }
                        lb[x] = lut[lb[x]];
                    }
                }
                break;
        }
    }
}

/****
1.) NOTE: xpos can be negative, but the 'start' value that is added
    to xpos will ensure it is positive.
    
    For an EC sprite that is offscreen, 'start' will be larger
    than 'end' and the for-loop used for rendering will abort
    on the first pass.
***/


#define RENDER_TX_LINE \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ];

#define RENDER_TX_BORDER \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; \
        *lb++ = 0x10 | clut[0]; 

#define RENDER_GR_LINE \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ]; \
        *lb++ = 0x10 | clut[ *bpex++ ];

#define RENDER_MC_LINE \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++; \
        *lb++ = 0x10 | *mcex++;

void make_tms_tables(void)
{
    int32_t i, j, x;
    int32_t bd, pg, ct;
    int32_t sx, bx;

    for(sx = 0; sx < 16; sx++)
    {
        for(bx = 0; bx < 256; bx++)
        {
/*          uint8_t bd = (bx & 0x0F); */
            uint8_t bs = (bx & 0x40);
/*          uint8_t bt = (bd == 0) ? 1 : 0; */
            uint8_t sd = (sx & 0x0F);

            /* opaque sprite pixel, choose 2nd pal and set sprite marker */
            if(sd && !bs)
                tms_obj_lut[(sx<<8)|(bx)] = sd | 0x10 | 0x40;
            else
				tms_obj_lut[(sx<<8)|(bx)] = bx;
        }
    }


    /* Text lookup table */
    for(bd = 0; bd < 256; bd++)
    {
        uint8_t bg = (bd >> 0) & 0x0F;
        uint8_t fg = (bd >> 4) & 0x0F;

        /* If foreground is transparent, use background color */
        if(fg == 0) fg = bg;

        txt_lookup[bd][0] = bg;
        txt_lookup[bd][1] = fg;
    }

    /* Multicolor lookup table */
    for(bd = 0; bd < 16; bd++)
    {
        for(pg = 0; pg < 256; pg++)
        {
            int32_t l = (pg >> 4) & 0x0F;
            int32_t r = (pg >> 0) & 0x0F;

            /* If foreground is transparent, use background color */
            if(l == 0) l = bd;
            if(r == 0) r = bd;

            /* Unpack 2 nibbles across eight pixels */
            for(x = 0; x < 8; x++)
            {
                int32_t c = (x & 4) ? r : l;

                mc_lookup[bd][pg][x] = c;
            }
        }
    }

    /* Make bitmap data expansion table */
    memset(bp_expand, 0, sizeof(bp_expand));
    for(i = 0; i < 256; i++)
    {
        for(j = 0; j < 8; j++)
        {
            int32_t c = (i >> (j ^ 7)) & 1;
            bp_expand[i][j] = c;
        }
    }

    /* Graphics I/II lookup table */
    for(bd = 0; bd < 0x10; bd++)
    {
        for(ct = 0; ct < 0x100; ct++)
        {
            int32_t backdrop = (bd & 0x0F);
            int32_t background = (ct >> 0) & 0x0F;
            int32_t foreground = (ct >> 4) & 0x0F;

            /* If foreground is transparent, use background color */
            if(background == 0) background = backdrop;
            if(foreground == 0) foreground = backdrop;

            tms_lookup[bd][ct][0] = background;
            tms_lookup[bd][ct][1] = foreground;
        }
    }
}


void render_bg_tms(int32_t line)
{
    switch(vdp.mode & 7)
    {
        case 0: /* Graphics I */
            render_bg_m0(line);
            break;

        case 1: /* Text */
            render_bg_m1(line);
            break;

        case 2: /* Graphics II */
            render_bg_m2(line);
            break;

        case 3: /* Text (Extended PG) */
            render_bg_m1x(line);
            break;

        case 4: /* Multicolor */
            render_bg_m3(line);
            break;

        case 5: /* Invalid (1+3) */
            render_bg_inv(line);
            break;

        case 6: /* Multicolor (Extended PG) */
            render_bg_m3x(line);
            break;

        case 7: /* Invalid (1+2+3) */
            render_bg_inv(line);
            break;
    }
}

/* Graphics I */
static void render_bg_m0(int32_t line)
{
    int32_t v_row  = (line & 7);
    int32_t column;
    int32_t name;

    uint8_t *clut;
    uint8_t *bpex;
    uint8_t *lb = &linebuf[0];
    uint8_t *pn = &vdp.vram[vdp.pn + ((line >> 3) << 5)];
    uint8_t *ct = &vdp.vram[vdp.ct];
    uint8_t *pg = &vdp.vram[vdp.pg | (v_row)];

    for(column = 0; column < 32; column++)
    {
        name = pn[column];
        clut = &tms_lookup[vdp.bd][ct[name >> 3]][0];
        bpex = &bp_expand[pg[name << 3]][0];
        RENDER_GR_LINE
    }
}

/* Text */
static void render_bg_m1(int32_t line)
{
    int32_t v_row  = (line & 7);
    int32_t column;

    uint8_t *clut;
    uint8_t *bpex;
    uint8_t *lb = &linebuf[0];
/*  uint8_t *pn = &vdp.vram[vdp.pn + ((line >> 3) * 40)]; */

    uint8_t *pn = &vdp.vram[vdp.pn + text_counter];

    uint8_t *pg = &vdp.vram[vdp.pg | (v_row)];
    uint8_t bk = vdp.reg[7];

    clut = &txt_lookup[bk][0];

    for(column = 0; column < 40; column++)
    {
        bpex = &bp_expand[pg[pn[column] << 3]][0];
        RENDER_TX_LINE        
    }

    /* V3 */
    if((vdp.line & 7) == 7)
        text_counter += 40;

    RENDER_TX_BORDER
}

/* Text + extended PG */
static void render_bg_m1x(int32_t line)
{
    int32_t v_row  = (line & 7);
    int32_t column;

    uint8_t *clut;
    uint8_t *bpex;
    uint8_t *lb = &linebuf[0];
    uint8_t *pn = &vdp.vram[vdp.pn + ((line >> 3) * 40)];
    uint8_t *pg = &vdp.vram[vdp.pg + (v_row) + ((line & 0xC0) << 5)];
    uint8_t bk = vdp.reg[7];

    clut = &tms_lookup[0][bk][0];

    for(column = 0; column < 40; column++)
    {
        bpex = &bp_expand[pg[pn[column] << 3]][0];
        RENDER_TX_LINE
    }
    RENDER_TX_BORDER
}

/* Invalid (2+3/1+2+3) */
static void render_bg_inv(int32_t line)
{
    int32_t column;
    uint8_t *clut;
    uint8_t *bpex;
    uint8_t *lb = &linebuf[0];
    uint8_t bk = vdp.reg[7];

    clut = &txt_lookup[bk][0];

    for(column = 0; column < 40; column++)
    {
        bpex = &bp_expand[0xF0][0];
        RENDER_TX_LINE
    }
}

/* Multicolor */
static void render_bg_m3(int32_t line)
{
    int32_t column;
    uint8_t *mcex;
    uint8_t *lb = &linebuf[0];

    uint8_t *pn = &vdp.vram[vdp.pn + ((line >> 3) << 5)];
    uint8_t *pg = &vdp.vram[vdp.pg + ((line >> 2) & 7)];

    for(column = 0; column < 32; column++)
    {
        mcex = &mc_lookup[vdp.bd][pg[pn[column]<<3]][0];
        RENDER_MC_LINE
    }
}

/* Multicolor + extended PG */
static void render_bg_m3x(int32_t line)
{
    int32_t column;
    uint8_t *mcex;
    uint8_t *lb = &linebuf[0];
    uint8_t *pn = &vdp.vram[vdp.pn + ((line >> 3) << 5)];
    uint8_t *pg = &vdp.vram[vdp.pg + ((line >> 2) & 7) + ((line & 0xC0) << 5)];

    for(column = 0; column < 32; column++)
    {
        mcex = &mc_lookup[vdp.bd][pg[pn[column]<<3]][0];
        RENDER_MC_LINE
    }
}

/* Graphics II */
static void render_bg_m2(int32_t line)
{
    int32_t v_row  = (line & 7);
    int32_t column;
    int32_t name;

    uint8_t *clut;
    uint8_t *bpex;
    uint8_t *lb = &linebuf[0];
    uint8_t *pn = &vdp.vram[vdp.pn | ((line & 0xF8) << 2)];
    uint8_t *ct = &vdp.vram[(vdp.ct & 0x2000) | (v_row) | ((line & 0xC0) << 5)];
    uint8_t *pg = &vdp.vram[(vdp.pg & 0x2000) | (v_row) | ((line & 0xC0) << 5)];

    for(column = 0; column < 32; column++)
    {
        name = pn[column] << 3;
        clut = &tms_lookup[vdp.bd][ct[name]][0];
        bpex = &bp_expand[pg[name]][0];
        RENDER_GR_LINE
    }
}

