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
 *   Sega Master System manager
 *
 ******************************************************************************/
#ifndef SYSTEM_H_
#define SYSTEM_H_

#define APP_NAME            "SMS Plus GX"
#define APP_VERSION         "1.8"

#define PALETTE_SIZE        0x20

/* Mask for removing unused pixel data */
#define PIXEL_MASK          0x1F

/* These can be used for 'input.pad[]' */
#define INPUT_UP            0x00000001
#define INPUT_DOWN          0x00000002
#define INPUT_LEFT          0x00000004
#define INPUT_RIGHT         0x00000008
#define INPUT_BUTTON1       0x00000010
#define INPUT_BUTTON2       0x00000020

/* These can be used for 'input.system' */
#define INPUT_START         0x00000001  /* Game Gear only */
#define INPUT_PAUSE         0x00000002  /* Master System only */
#define INPUT_RESET         0x00000004  /* Master System only */

enum 
{
	SRAM_SAVE   = 0,
	SRAM_LOAD   = 1
};

/* User input structure */
typedef struct
{
	int32_t analog[2][2];
	uint8_t pad[2];
	uint8_t system;
} input_t;

/* Game image structure */
typedef struct
{
	uint8_t *rom;
	uint8_t loaded;
	uint32_t size;
	/* We need to use an unsigned short for pages, as Bad Apple SMS requires it !*/
	uint16_t pages;
	uint32_t crc;
	uint32_t sram_crc;
	uint8_t mapper;
	uint8_t sram[0x8000];
	uint8_t fcr[4];
} cart_t;

/* Bitmap structure */
typedef struct
{
	uint8_t *data;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t depth;
	struct 
	{
		int32_t x, y, w, h;
		int32_t ox, oy, ow, oh;
		int32_t changed;
	} viewport;
    struct
    {
        uint8_t color[PALETTE_SIZE][3];
        uint8_t dirty[PALETTE_SIZE];
        uint8_t update;
    } pal;
} bitmap_t;

/* Global variables */
extern bitmap_t bitmap;   /* Display bitmap */
extern cart_t cart;       /* Game cartridge data */
extern input_t input;     /* Controller input */

/* Function prototypes */
extern void system_frame(uint32_t skip_render);
extern void system_init(void);
extern void system_shutdown(void);
extern void system_reset(void);
extern void system_manage_sram(uint8_t *sram, uint8_t slot_number, uint8_t mode);
extern void system_poweron(void);
extern void system_poweroff(void);

#endif /* _SYSTEM_H_ */


