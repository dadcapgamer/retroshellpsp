/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additionnal code by Eke-Eke (SMS Plus GX)
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
 *   VDP rendering core
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * June 3rd 2019 : Added NOBLANKING_LEFTCOLUM for platforms like the RS-90.
 * March 15th 2019 : Correct the brightness level in the Master system palette. It doesn't look dark anymore.
 * March 14th 2019 : Fix PAL code as it would cause issues with Fantastic Dizzy.
 * March 13th 2019 : Minor fixes as part of the CrabZ80's revert. (mostly whitepacing)
 * March 11th 2019 : Fixed scrolling issues with Gauntlet. Fixed PAL issues too.
 * December 7th 2018 : LIGHTGUN define and some whitepacing.
 * October 12th 2018 : Whitepacing and minor fixes.
*/

#include "shared.h"
/*** Vertical Counter Tables ***/
extern uint8_t *vc_table[2][3];

static struct
{
  uint16_t yrange;
  uint16_t xpos;
  uint16_t attr;
} object_info[64];

/* Background drawing function */
void (*render_bg)(int32_t line) = NULL;
void (*render_obj)(int32_t line) = NULL;

/* Pointer to output buffer */
uint8_t *linebuf;

/* Pixel 8-bit color tables */
uint8_t sms_cram_expand_table[4];
uint8_t gg_cram_expand_table[16];

/* Dirty pattern info */
uint8_t bg_name_dirty[0x200];     /* 1= This pattern is dirty */
uint16_t bg_name_list[0x200];     /* List of modified pattern indices */
uint16_t bg_list_index;           /* # of modified patterns in list */

/* Internal buffer for drawing non 8-bit displays */
#ifndef _8BPP_COLOR
static uint8_t internal_buffer[0x200];
/* Precalculated pixel table */
static uint16_t pixel[PALETTE_SIZE];
#endif

static uint8_t bg_pattern_cache[0x20000];/* Cached and flipped patterns */

/* Pixel look-up table */
static uint8_t lut[0x10000];

static uint8_t object_index_count;

/* Top Border area height */
static uint8_t active_border[2][3] =
{
	{24, 8,  0},  /* NTSC VDP */
	{24, 8,  0}   /* PAL  VDP */
};

/* Active Scan Area height */
static uint16_t active_range[2] =
{
	243, /* NTSC VDP */
	294  /* PAL  VDP */
};

/* CRAM palette in TMS compatibility mode */
static const uint8_t tms_crom[] =
{
	0x00, 0x00, 0x08, 0x0C,
	0x10, 0x30, 0x01, 0x3C,
	0x02, 0x03, 0x05, 0x0F,
	0x04, 0x33, 0x15, 0x3F
};

/* original TMS palette for SG-1000 & Colecovision */
static uint8_t tms_palette[16*3][3] =
{
	/* from Sean Young (http://www.smspower.org/dev/docs/tms9918a.txt) */
	{  0,  0,  0},
	{  0,  0,  0},
	{ 33,200, 66},
	{ 94,220,120},
	{ 84, 85,237},
	{125,118,252},
	{212, 82, 77},
	{ 66,235,245},
	{252, 85, 84},
	{255,121,120},
	{212,193, 84},
	{230,206,128},
	{ 33,176, 59},
	{201, 91,186},
	{204,204,204},
	{255,255,255},
  /* from Omar Cornut (http://www.smspower.org/dev/docs/sg1000.txt) */
	{  0,  0,  0},
	{  0,  0,  0},
	{ 32,192, 30},
	{ 96,224, 96},
	{ 32, 32,224},
	{ 64, 96,224},
	{160, 32, 32},
	{ 64,192,224},
	{224, 32, 32},
	{224, 64, 64},
	{192,192, 32},
	{192,192,128},
	{ 32,128, 32},
	{192, 64,160},
	{160,160,160},
	{224,224,224},
  /* from Richard F. Drushel (http://users.stargate.net/~drushel/pub/coleco/twwmca/wk961118.html) */
	{  0,  0,  0},
	{  0,  0,  0},
	{ 71,183, 59},
	{124,207,111},
	{ 93, 78,255},
	{128,114,255},
	{182, 98, 71},
	{ 93,200,237},
	{215,107, 72},
	{251,143,108},
	{195,205, 65},
	{211,218,118},
	{ 62,159, 47},
	{182,100,199},
	{204,204,204},
	{255,255,255}
};

/* Attribute expansion table */
static const uint32_t atex[4] =
{
	0x00000000,
	0x10101010,
	0x20202020,
	0x30303030,
};

/* Bitplane to packed pixel LUT */
static uint32_t bp_lut[0x10000];

static void parse_satb(int32_t line);
static void update_bg_pattern_cache(void);
#ifndef _8BPP_COLOR
static void remap_8_to_16(int32_t line);
#endif

/* Macros to access memory 32-bits at a time (from MAME's drawgfx.c) */

#ifdef ALIGN_DWORD

static __inline__ uint32_t read_dword(void *address)
{
  if ((uint32_t)address & 3)
  {
#ifdef LSB_FIRST  /* little endian version */
    return ( *((uint8_t *)address) +
            (*((uint8_t *)address+1) << 8)  +
            (*((uint8_t *)address+2) << 16) +
            (*((uint8_t *)address+3) << 24) );
#else       /* big endian version */
    return ( *((uint8_t *)address+3) +
            (*((uint8_t *)address+2) << 8)  +
            (*((uint8_t *)address+1) << 16) +
            (*((uint8_t *)address)   << 24) );
#endif
  }
  else
    return *(uint32_t *)address;
}


static __inline__ void write_dword(void *address, uint32_t data)
{
  if ((uint32_t)address & 3)
  {
#ifdef LSB_FIRST
    *((uint8_t *)address) =  data;
    *((uint8_t *)address+1) = (data >> 8);
    *((uint8_t *)address+2) = (data >> 16);
    *((uint8_t *)address+3) = (data >> 24);
#else
    *((uint8_t *)address+3) =  data;
    *((uint8_t *)address+2) = (data >> 8);
    *((uint8_t *)address+1) = (data >> 16);
    *((uint8_t *)address)   = (data >> 24);
#endif
    return;
  }
  else
    *(uint32_t *)address = data;
}
#else
#define read_dword(address) *(uint32_t *)address
#define write_dword(address,data) *(uint32_t *)address=data
#endif


/****************************************************************************/


void render_shutdown(void)
{
}

/* Initialize the rendering data */
void render_init(void)
{
	int32_t i, j;
	int32_t bx, sx, b, s, bp, bf, sf, c;

	make_tms_tables();

	/* Generate 64k of data for the look up table */
	for(bx = 0; bx < 0x100; bx++)
	{
		for(sx = 0; sx < 0x100; sx++)
		{
			/* Background pixel */
			b  = (bx & 0x0F);

			/* Background priority */
			bp = (bx & 0x20) ? 1 : 0;

			/* Full background pixel + priority + sprite marker */
			bf = (bx & 0x7F);

			/* Sprite pixel */
			s  = (sx & 0x0F);

			/* Full sprite pixel, w/ palette and marker bits added */
			sf = (sx & 0x0F) | 0x10 | 0x40;

			/* Overwriting a sprite pixel ? */
			if(bx & 0x40)
			{
				/* Return the input */
				c = bf;
			}
			else
			{
				/* Work out priority and transparency for both pixels */
				if(bp)
				{
					/* Underlying pixel is high priority */
					if(b)
					{
						c = bf | 0x40;
					}
					else
					{
						if(s) c = sf;
						else c = bf;
					}
				}
				else
				{
					/* Underlying pixel is low priority */
					if(s) c = sf;
					else c = bf;
				}
			}
			/* Store result */
			lut[(bx << 8) | (sx)] = c;
		}
	}

	/* Make bitplane to pixel lookup table */
	for(i = 0; i < 0x100; i++)
	{
		for(j = 0; j < 0x100; j++)
		{
			int32_t x;
			uint32_t out = 0;
			for(x = 0; x < 8; x++)
			{
				out |= (j & (0x80 >> x)) ? (uint32_t)(8 << (x << 2)) : 0;
				out |= (i & (0x80 >> x)) ? (uint32_t)(4 << (x << 2)) : 0;
			}
#if LSB_FIRST
			bp_lut[(j << 8) | (i)] = out;
#else
			bp_lut[(i << 8) | (j)] = out;
#endif
		}
	}

	sms_cram_expand_table[0] =  0;
	sms_cram_expand_table[1] = 0x55;
	sms_cram_expand_table[2] = 0xAA;
	sms_cram_expand_table[3] = 0xFF;

	for(i = 0; i < 16; i++)
	{
		gg_cram_expand_table[i] = i << 4 | i;    
	}
}


/* Reset the rendering data */
void render_reset(void)
{
	int32_t i;

	/* Clear display bitmap */
	memset(bitmap.data, 0, (long) bitmap.pitch * bitmap.height);

	/* Clear palette */
	for(i = 0; i < PALETTE_SIZE; i++)
	{
		palette_sync(i);
	}

	/* Invalidate pattern cache */
	memset(bg_name_dirty, 0, sizeof(bg_name_dirty));
	memset(bg_name_list, 0, sizeof(bg_name_list));
	bg_list_index = 0;
	memset(bg_pattern_cache, 0, sizeof(bg_pattern_cache));
	
#ifdef _8BPP_COLOR
    /* Mark all colors as dirty */
    bitmap.pal.update = 1;
    memset(bitmap.pal.dirty, 1, PALETTE_SIZE);
    memset(bitmap.pal.color, 0, PALETTE_SIZE * 3);
#endif

	/* Pick default render routine */
	if (vdp.reg[0] & 4)
	{
		render_bg = render_bg_sms;
		render_obj = render_obj_sms;
	}
	else
	{
		render_bg = render_bg_tms;
		render_obj = render_obj_tms;
	}
}

static int32_t prev_line = -1;

/* Draw a line of the display */
void render_line(int32_t line)
{
	int32_t view = 1;

	/* Ensure we have not already rendered this line */
	if (prev_line == line) return;
	prev_line = line;

	/* Ensure we're within the VDP active area (incl. overscan) */
	int32_t top_border = active_border[sms.display][vdp.extended];
	int32_t vline = (line + top_border) % vdp.lpf;
	
	if (vline >= active_range[sms.display]) return;

	/* adjust for Game Gear screen */
	top_border = top_border + (vdp.height - bitmap.viewport.h) / 2;

	/* Point to current line in output buffer */
	#ifdef _8BPP_COLOR
	linebuf = &bitmap.data[(line * bitmap.pitch)];
	#else
	linebuf = &internal_buffer[0];
	#endif

	/* Sprite limit flag is set at the beginning of the line */
	if (vdp.spr_ovr)
	{
		vdp.spr_ovr = 0;
		vdp.status |= 0x40;
	}

	/* Vertical borders */
	if ((vline < top_border) || (vline >= (bitmap.viewport.h + top_border)))
	{
		/* Sprites are still processed offscreen */
		if ((vdp.mode > 7) && (vdp.reg[1] & 0x40)) 
			render_obj(line);
		/* Line is only displayed where overscan is emulated */
		view = 0;
	}
	/* Active display */
	else
	{
		/* Display enabled ? */
		if (vdp.reg[1] & 0x40)
		{
			/* Update pattern cache */
			update_bg_pattern_cache();

			/* Draw background */
			render_bg(line);

			/* Draw sprites */
			render_obj(line);

			/* This can take some CPU time. Some targets like the Retromini RS-90 code will crop this section
			 * as to make sure it fits on the screen and to avoid less artifacts from downscaling.
			 * This can be useful for other targets as well but adding a test case
			 * to check this might make it more expensive so best to do this only for the RS-90.
			 * */
#ifndef NOBLANKING_LEFTCOLUM
			/* Blank leftmost column of display */
			if((vdp.reg[0] & 0x20) && (IS_SMS || IS_MD))
				memset(linebuf, BACKDROP_COLOR, 8);
#endif
		}
		else
		{
			/* Background color */
			memset(linebuf, BACKDROP_COLOR, bitmap.viewport.w + 2*bitmap.viewport.x);
		}
	}

	/* Parse Sprites for next line */
	if (vdp.mode > 7)
		parse_satb(line);
	else
		parse_line(line);

	/* LightGun mark */
	#ifdef LIGHTGUN_ENABLED
	/* Putting this in an ifdef because the condition loop is executed every render_line */
	if (sms.device[0] == DEVICE_LIGHTGUN)
	{
		int32_t dy = vdp.line - input.analog[0][1];
		if (abs(dy) < 6)
		{
			int32_t i;
			int32_t start = input.analog[0][0] - 4;
			int32_t end = input.analog[0][0] + 4;
			if (start < 0) start = 0;
			if (end > 255) end = 255;
			for (i=start; i<end+1; i++)
			{
				linebuf[i] = 0xFF;
			}
		}
	}
	#endif
	
	/* Only draw lines within the video output range ! */
	if (view)
	{
		/* adjust output line */
		vline -= top_border;
		#ifndef _8BPP_COLOR
		remap_8_to_16(vline);
		#endif
	}
}

/* Draw the Master System background */
void render_bg_sms(int32_t line)
{
	int32_t locked = 0;
	int32_t yscroll_mask = (vdp.extended) ? 256 : 224;
	int32_t v_line = (line + vdp.vscroll) % yscroll_mask;
	int32_t v_row  = (v_line & 7) << 3;
	int32_t hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10) && (sms.console != CONSOLE_GG)) ? 0 : (0x100 - vdp.reg[8]);
	int32_t column = 0;
	uint16_t attr;
	uint16_t SMS_VDP_BUG = (((sms.console == CONSOLE_SMS) && !(vdp.reg[2] & 1)) ? ~0x400 :0xFFFF);
	uint16_t nt_addr = (vdp.ntab + ((v_line >> 3) << 6)) & SMS_VDP_BUG;
	uint16_t nt_addr_stop_verticalscroll = (vdp.ntab + ((line >> 3) << 6)) & SMS_VDP_BUG;
	uint16_t *nt = (uint16_t *)&vdp.vram[nt_addr];
	int32_t nt_scroll = (hscroll >> 3);
	int32_t shift = (hscroll & 7);
	uint32_t atex_mask;
	uint32_t *cache_ptr;
	uint32_t *linebuf_ptr = (uint32_t *)&linebuf[0 - shift];

	/* Draw first column (clipped) */
	if(shift)
	{
		int32_t x;
		for(x = shift; x < 8; x++)
			linebuf[(0 - shift) + (x)] = 0;
		column++;
	}

	/* Draw a line of the background */
	for(; column < 32; column++)
	{
		/* Stop vertical scrolling for leftmost eight columns */
		if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
		{
			locked = 1;
			v_row = (line & 7) << 3;
			nt = (uint16_t *)&vdp.vram[nt_addr_stop_verticalscroll];
		}

		/* Get name table attribute word */
		attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
		attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
		/* Expand priority and palette bits */
		atex_mask = atex[(attr >> 11) & 3];

		/* Point to a line of pattern data in cache */
		cache_ptr = (uint32_t *)&bg_pattern_cache[((attr & 0x7FF) << 6) | (v_row)];
    
		/* Copy the left half, adding the attribute bits in */
		write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

		/* Copy the right half, adding the attribute bits in */
		write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
	}

	/* Draw last column (clipped) */
	if(shift)
	{
		int32_t x;
		uint8_t c, a;
		uint8_t *p = &linebuf[(0 - shift)+(column << 3)];
		attr = nt[(column + nt_scroll) & 0x1F];
#ifndef LSB_FIRST
		attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
		a = (attr >> 7) & 0x30;

		for(x = 0; x < shift; x++)
		{
			c = bg_pattern_cache[((attr & 0x7FF) << 6) | (v_row) | (x)];
			p[x] = ((c) | (a));
		}
	}
}

/* Draw sprites */
void render_obj_sms(int32_t line)
{
	int32_t i,x,start,end,xp,yp,n;
	uint8_t sp,bg;
	uint8_t *linebuf_ptr;
	uint8_t *cache_ptr;

	int32_t width = 8;

	/* Adjust dimensions for double size sprites */
	if(vdp.reg[1] & 0x01)
		width *= 2;

	/* Draw sprites in front-to-back order */
	for(i = 0; i < object_index_count; i++)
	{
		/* Width of sprite */
		start = 0;
		end = width;

		/* Sprite X position */
		xp = object_info[i].xpos;

		/* Sprite Y range */
		yp = object_info[i].yrange;

		/* Pattern name */
		n = object_info[i].attr;

		/* X position shift */
		if(vdp.reg[0] & 0x08) xp -= 8;

		/* Add MSB of pattern name */
		if(vdp.reg[6] & 0x04) n |= 0x0100;

		/* Mask LSB for 8x16 sprites */
		if(vdp.reg[1] & 0x02) n &= 0x01FE;

		/* Point to offset in line buffer */
		linebuf_ptr = (uint8_t *)&linebuf[xp];

		/* Clip sprites on left edge */
		if(xp < 0)
		  start = (0 - xp);

		/* Clip sprites on right edge */
		if((xp + width) > 256)
		  end = (256 - xp);

		/* Draw double size sprite */
		if(vdp.reg[1] & 0x01)
		{
			/* Retrieve tile data from cached nametable */
			cache_ptr = (uint8_t *)&bg_pattern_cache[(n << 6) | ((yp >> 1) << 3)];
			
			/* Draw sprite line (at 1/2 dot rate) */
			for(x = start; x < end; x+=2)
			{
				/* Source pixel from cache */
				sp = cache_ptr[(x >> 1)];

				/* Only draw opaque sprite pixels */
				if(sp)
				{
					/* Background pixel from line buffer */
					bg = linebuf_ptr[x];

					/* Look up result */
					linebuf_ptr[x] = linebuf_ptr[x+1] = lut[(bg << 8) | (sp)];

					/* Check sprite collision */
					/* No sprite collision for 9th sprite. This passes Flubba's test */
					if ((bg & 0x40) && !(vdp.status & 0x20) && object_index_count != 9)
					{
						/* pixel-accurate SPR_COL flag */
						vdp.status |= 0x20;
						vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
					}
				}
			}
		}
		else /* Regular size sprite (8x8 / 8x16) */
		{
			/* Retrieve tile data from cached nametable */
			cache_ptr = (uint8_t *)&bg_pattern_cache[(n << 6) | (yp << 3)];

			/* Draw sprite line */
			for(x = start; x < end; x++)
			{
				/* Source pixel from cache */
				sp = cache_ptr[x];

				/* Only draw opaque sprite pixels */
				if(sp)
				{
					/* Background pixel from line buffer */
					bg = linebuf_ptr[x];

					/* Look up result */
					linebuf_ptr[x] = lut[(bg << 8) | (sp)];

					/* Check sprite collision */
					/* No sprite collision for 9th sprite. This passes Flubba's test */
					if ((bg & 0x40) && !(vdp.status & 0x20) && object_index_count != 9)
					{
						/* pixel-accurate SPR_COL flag */
						vdp.status |= 0x20;
						vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
					}
				}
			}
		}
	}
}

/* Update a palette entry */
void palette_sync(int32_t index)
{
	int32_t r, g, b;
	
	/* VDP Mode */
	if ((vdp.reg[0] & 4) || IS_GG)
	{
		/* Mode 4 or Game Gear TMS mode*/
		if(sms.console == CONSOLE_GG)
		{
			/* GG palette */
			/* ----BBBBGGGGRRRR */
			r = (vdp.cram[(index << 1) | (0)] >> 0) & 0x0F;
			g = (vdp.cram[(index << 1) | (0)] >> 4) & 0x0F;
			b = (vdp.cram[(index << 1) | (1)] >> 0) & 0x0F;

			r = gg_cram_expand_table[r];
			g = gg_cram_expand_table[g];
			b = gg_cram_expand_table[b];
		}
		else
		{
			/* SMS palette */
			/* --BBGGRR */
			r = (vdp.cram[index] >> 0) & 3;
			g = (vdp.cram[index] >> 2) & 3;
			b = (vdp.cram[index] >> 4) & 3;
			
			r = sms_cram_expand_table[r];
			g = sms_cram_expand_table[g];
			b = sms_cram_expand_table[b];
		}
	}
	else
	{
		/* TMS Mode (16 colors only) */
		int32_t color = index & 0x0F;

		if (sms.console < CONSOLE_SMS)
		{
			/* pick one of the original TMS9918 palettes */
			color += option.tms_pal * 16;

			r = tms_palette[color][0];
			g = tms_palette[color][1];
			b = tms_palette[color][2];
		}
		else
		{
			/* fixed CRAM palette in TMS mode */ 
			r = (tms_crom[color] >> 0) & 3;
			g = (tms_crom[color] >> 2) & 3;
			b = (tms_crom[color] >> 4) & 3;

			r = sms_cram_expand_table[r];
			g = sms_cram_expand_table[g];
			b = sms_cram_expand_table[b];
		}
	}
	
	
	#ifdef _8BPP_COLOR
    bitmap.pal.color[index][0] = r;
    bitmap.pal.color[index][1] = g;
    bitmap.pal.color[index][2] = b;
	bitmap.pal.dirty[index] = bitmap.pal.update = 1;
	#else
	pixel[index] = MAKE_PIXEL(r, g, b);
	#endif
}

static void parse_satb(int32_t line)
{
	/* Pointer to sprite attribute table */
	uint8_t *st = (uint8_t *)&vdp.vram[vdp.satb];

	/* Sprite counter (64 max.) */
	int32_t i = 0;

	/* Line counter value */
	uint8_t vc = vc_table[sms.display][vdp.extended][line];

	/* Sprite height (8x8 by default) */
	uint8_t yp;
	uint8_t height = 8;
	uint8_t zoomed = vdp.reg[1] & 0x01;
  
	/* Adjust height for 8x16 sprites */
	if(vdp.reg[1] & 0x02) 
		height <<= 1;

	/* Adjust height for zoomed sprites */
	if(zoomed)
		height <<= 1;

	/* Sprite count for current line (8 max.) */
	object_index_count = 0;

	for(i = 0; i < 64; i++)
	{
		/* Sprite Y position */
		yp = st[i];

		/* Found end of sprite list marker for non-extended modes? */
		if(vdp.extended == 0 && yp == 208)
			return;

		/* Wrap Y coordinate for sprites > 240 */
		if(yp > 240) yp -= 256;

		/* Compare sprite position with current line counter */
		yp |= (zoomed);
 		yp = line - yp;
		
		/* Sprite is within vertical range? */
		if(yp < height)
		{
			/* Sprite limit reached? */
			if (object_index_count == 8)
			{
				/* Flag is set only during active area */
				if (line < vdp.height)
				vdp.spr_ovr = 1;

				/* End of sprite parsing */
				if (option.spritelimit)
					return;
			}

			/* Store sprite attributes for later processing */
			object_info[object_index_count].yrange = yp;
			object_info[object_index_count].xpos = st[0x80 + (i << 1)];
			object_info[object_index_count].attr = st[0x81 + (i << 1)];

			/* Increment Sprite count for current line */
			++object_index_count;
		}
	}
}

static void update_bg_pattern_cache(void)
{
	int32_t i;
	uint8_t x, y;
	uint16_t name;

	if(!bg_list_index) return;

	for(i = 0; i < bg_list_index; i++)
	{
		name = bg_name_list[i];
		bg_name_list[i] = 0;

		for(y = 0; y < 8; y++)
		{
			if(bg_name_dirty[name] & (1 << y))
			{
				uint8_t *dst = &bg_pattern_cache[name << 6];
				uint16_t bp01 = *(uint16_t *)&vdp.vram[(name << 5) | (y << 2) | (0)];
				uint16_t bp23 = *(uint16_t *)&vdp.vram[(name << 5) | (y << 2) | (2)];
				uint32_t temp = (bp_lut[bp01] >> 2) | (bp_lut[bp23]);

				for(x = 0; x < 8; x++)
				{
					uint8_t c = (temp >> (x << 2)) & 0x0F;
					dst[0x00000 | (y << 3) | (x)] = (c);
					dst[0x08000 | (y << 3) | (x ^ 7)] = (c);
					dst[0x10000 | ((y ^ 7) << 3) | (x)] = (c);
					dst[0x18000 | ((y ^ 7) << 3) | (x ^ 7)] = (c);
				}
			}
		}
		bg_name_dirty[name] = 0;
	}
	
	bg_list_index = 0;
}

#ifndef _8BPP_COLOR
static void remap_8_to_16(int32_t line)
{
	int32_t i;
	uint16_t *p = (uint16_t *)&bitmap.data[(line * bitmap.pitch)];
	int32_t width = (bitmap.viewport.w)+2 * bitmap.viewport.x;
	
	LOCK_VIDEO

	for(i = 0; i < width; i++)
	{
		p[i] = pixel[ internal_buffer[i] & PIXEL_MASK ];
	}
	
	UNLOCK_VIDEO
}
#endif
