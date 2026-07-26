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
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * March 15th 2019 : Minor changes.
 * March 14th 2019 : Use fwrite instead of fputc. Also fix an issue with fread which could cause issues.
 * March 13th 2019 : Bring it back again due to regressions. Also i parsed some random shit by accident so i fixed it again.
 * March 9th 2019 : Decomment CrabZ80 related code and removing extra externs.
 * March 7th 2019 : Comment out CrabZ80 related code.
 * Feb 2nd 2019 : Sound function names were changed, fix accordingly.
*/

#include "shared.h"

#if !defined(MAXIM_PSG) && !defined(MAME_PSG)
extern sn76489_t psg_sn;
#endif

uint32_t system_save_state(FILE* fd)
{
    /* Save VDP context */
    fwrite(&vdp, sizeof(vdp_t), sizeof(int8_t), fd);

    /* Save SMS context */
    fwrite(&sms, sizeof(sms_t), sizeof(int8_t), fd);

	fwrite(cart.fcr, 4, sizeof(int8_t), fd);

    fwrite(cart.sram, 0x8000, sizeof(int8_t), fd);

    /* Save Z80 context */
	fwrite(Z80_Context, sizeof(z80_t), sizeof(int8_t), fd);
    
    /* Save YM2413 context */
    fwrite(FM_GetContextPtr(), FM_GetContextSize(), sizeof(int8_t), fd);

    /* Save SN76489 context */
    #ifdef MAXIM_PSG
    fwrite(SN76489_GetContextPtr(0), SN76489_GetContextSize(), sizeof(int8_t), fd);
    #elif defined(MAME_PSG)
    fwrite(&PSG, sizeof(sn76489_t), sizeof(int8_t), fd);
    #else
    fwrite(&psg_sn, sizeof(sn76489_t), sizeof(int8_t), fd);
    #endif
	
	return 0;
}

void system_load_state(FILE* fd)
{
	uint8_t *buf;
	uint32_t i;
	
	/* Initialize everything */
	system_reset();
   
    /* Load VDP context */
    fread(&vdp, sizeof(vdp_t), sizeof(int8_t), fd);

    /* Load SMS context */
    fread(&sms, sizeof(sms_t), sizeof(int8_t), fd);

	/** restore video & audio settings (needed if timing changed) ***/
	vdp_init();
	SMSPLUS_sound_init();

	fread(cart.fcr, 4, sizeof(int8_t), fd);

    fread(cart.sram, 0x8000, sizeof(int8_t), fd);

    /* Load Z80 context */
    fread(Z80_Context, sizeof(z80_t), sizeof(int8_t), fd);
    Z80.irq_callback = sms_irq_callback;

    /* Load YM2413 context */
    buf = malloc(FM_GetContextSize());
    fread(buf, FM_GetContextSize(), sizeof(int8_t), fd);
    FM_SetContext(buf);
    free(buf);

    /* Load SN76489 context */
    #ifdef MAXIM_PSG
    buf = malloc(SN76489_GetContextSize());
    fread(buf, SN76489_GetContextSize(), sizeof(int8_t), fd);
    SN76489_SetContext(0, buf);
    free(buf);
    #elif defined(MAME_PSG)
    buf = malloc(sizeof(sn76489_t));
    fread(buf, sizeof(sn76489_t), sizeof(int8_t), fd);
    memcpy(&PSG, buf, sizeof(sn76489_t));
    free(buf);
    #else
    buf = malloc(sizeof(sn76489_t));
    fread(buf, sizeof(sn76489_t), sizeof(int8_t), fd);
    memcpy(&psg_sn, buf, sizeof(sn76489_t));
    free(buf);
    #endif

	if ((sms.console != CONSOLE_COLECO) && (sms.console != CONSOLE_SG1000) && (sms.console != CONSOLE_SORDM5))
	{
		/* Cartridge by default */
		slot.rom    = cart.rom;
		slot.pages  = cart.pages;
		slot.mapper = cart.mapper;
		slot.fcr = &cart.fcr[0];

		/* Restore mapping */
		mapper_reset();
		cpu_readmap[0]  = &slot.rom[0];
		if (slot.mapper != MAPPER_KOREA_MSX)
		{
			mapper_16k_w(0,slot.fcr[0]);
			mapper_16k_w(1,slot.fcr[1]);
			mapper_16k_w(2,slot.fcr[2]);
			mapper_16k_w(3,slot.fcr[3]);
		}
		else
		{
			mapper_8k_w(0,slot.fcr[0]);
			mapper_8k_w(1,slot.fcr[1]);
			mapper_8k_w(2,slot.fcr[2]);
			mapper_8k_w(3,slot.fcr[3]);
		}
	}

	/* Force full pattern cache update */
	bg_list_index = 0x200;
	for(i = 0; i < 0x200; i++)
	{
		bg_name_list[i] = i;
		bg_name_dirty[i] = 255;
	}

	/* Restore palette */
	for(i = 0; i < PALETTE_SIZE; i++)
		palette_sync(i);
}
