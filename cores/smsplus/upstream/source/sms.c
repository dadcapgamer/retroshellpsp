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
 *   Sega Master System console emulation
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * July 16th 2019 : Add 4PAK support.
 * March 13th 2019 : Reverted previous changes (partially) due to regressions.
 * March 7th 2019 : Removing CrabZ80's core.
 * Feb 19th 2019 : Minor fix to sms_irq function.
 * June 6th 2019 : Japanese BIOS does not clear BIOS, fix that.
*/

#include "shared.h"

/* SMS context */
sms_t sms;

/* BIOS/CART ROM */
bios_t bios;
slot_t slot;

/* Colecovision support */
t_coleco coleco;

uint8_t dummy_write[0x400];
uint8_t dummy_read[0x400];

uint8_t *MMapPtrs[64] = { 0 };

static void writemem_mapper_none(uint16_t offset, uint8_t data)
{
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

static void writemem_mapper_sega(uint16_t offset, uint8_t data)
{
	if(offset >= 0xFFFC)
	{
		mapper_16k_w(offset & 3, data);
	}
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

static void writemem_mapper_codies(uint16_t offset, uint8_t data)
{
	if (offset == 0x0000)
	{
		mapper_16k_w(1,data);
		return;
	}
	if (offset == 0x4000)
	{
		mapper_16k_w(2,data);
		return;
	}
	if (offset == 0x8000)
	{
		mapper_16k_w(3,data);
		return;
	}
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

static void writemem_mapper_korea_msx(uint16_t offset, uint8_t data)
{
	if (offset <= 0x0003)
	{
		mapper_8k_w(offset,data);
		return;
	}
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

static void writemem_mapper_korea(uint16_t offset, uint8_t data)
{
	if (offset == 0xA000)
	{
		mapper_16k_w(3,data);
		return;
	}
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

/*
	Mostly similar to Codemasters's writemap function except for 8000-0xBFFF range.
	Write xx to 3FFE: map bank (xx)&maxbankmask to memory slot in 0000-3FFF region
	Write yy to 7FFF: map bank (yy)&maxbankmask to memory slot in 4000-7FFF region
	Write zz to BFFF: map bank ((xx&0x30)+(zz))&maxbankmask to memory slot in 8000-BFFF region
*/
static void writemem_mapper_4pak(uint16_t offset, uint8_t data)
{
	if (offset == 0x3FFE)
	{
		mapper_16k_w(1,data);
		return;
	}
	if (offset == 0x7FFF)
	{
		mapper_16k_w(2,data);
		return;
	}
	/* https://github.com/OpenEmu/CrabEmu-Core/blob/a25c7277cd14f9e32647e1b9e1a45621fb051f13/consoles/sms/mapper-4PAA.c#91 
	 * I could not figure this bit out but it looks like xx was just in fact paging register 1. */
	if (offset == 0xBFFF)
	{
		mapper_16k_w(3, (slot.fcr[1] & 0x30) + data);
		return;
	}
	cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

void MemoryAddRAM(int bank, int offset, void *ptr)
{
	MMapPtrs[bank] = ptr + offset;
}

void mapper_reset(void)
{
	switch(slot.mapper)
	{
		case MAPPER_NONE:
			cpu_writemem16 = writemem_mapper_none;
		break;
		case MAPPER_CODIES:
			cpu_writemem16 = writemem_mapper_codies;
		break;
		case MAPPER_KOREA:
			cpu_writemem16 = writemem_mapper_korea;
		break;
		case MAPPER_KOREA_MSX:
			cpu_writemem16 = writemem_mapper_korea_msx;
		break;
		case MAPPER_4PAK:
			cpu_writemem16 = writemem_mapper_4pak;
		break;
		default:
			cpu_writemem16 = writemem_mapper_sega;
		break;
	}
}

void sms_init(void)
{
	CPUZ80_Init();
	/* Default: open bus */
	data_bus_pullup   = 0x00;
	data_bus_pulldown = 0x00;
	
	/* Initialize port handlers */
	switch(sms.console)
	{
		case CONSOLE_COLECO:
			cpu_writeport16 = coleco_port_w;
			cpu_readport16 = coleco_port_r;
			data_bus_pullup = 0xFF;
		break;
		#ifdef SORDM5_EMU
		case CONSOLE_SORDM5:
			cpu_writeport16 = sordm5_port_w;
			cpu_readport16 = sordm5_port_r;
			data_bus_pullup = 0xFF;
		break;
		#endif
		case CONSOLE_SG1000:
		case CONSOLE_SC3000:
		case CONSOLE_SF7000:
			cpu_writeport16 = tms_port_w;
			cpu_readport16 = tms_port_r;
			data_bus_pullup = 0xFF;
		break;

		case CONSOLE_SMS:
			cpu_writeport16 = sms_port_w;
			cpu_readport16 = sms_port_r;
		break;

		case CONSOLE_SMS2:
			cpu_writeport16 = sms_port_w;
			cpu_readport16 = sms_port_r;
			data_bus_pullup = 0xFF;
		break;

		case CONSOLE_GG:
			cpu_writeport16 = gg_port_w;
			cpu_readport16 = gg_port_r;
			data_bus_pullup = 0xFF;
		break;

		case CONSOLE_GGMS:
			cpu_writeport16 = ggms_port_w;
			cpu_readport16 = ggms_port_r;
			data_bus_pullup = 0xFF;
		break;

		case CONSOLE_GEN:
		case CONSOLE_MD:
			cpu_writeport16 = md_port_w;
			cpu_readport16 = md_port_r;
		break;

		case CONSOLE_GENPBC:
		case CONSOLE_MDPBC:
			cpu_writeport16 = md_port_w;
			cpu_readport16 = md_port_r;
			data_bus_pullup = 0xFF;
		break;
	}
}

void sms_shutdown(void)
{
	/* Nothing to do */
}

void sms_reset(void)
{
  uint_fast8_t i;

  /* reset Z80 state */
  CPUZ80_Reset();

  /* clear SMS context */
  memset(dummy_write,data_bus_pullup, sizeof(dummy_write));
  memset(dummy_read,data_bus_pullup, sizeof(dummy_read));
  /* The joys of uninitiliazed memory ! The Japanese and Korean BIOS do not properly clear the memory and some games like Alibaba and 40 Thieves relies on this.
   * Without said fix, Alibaba will flicker (or it seems that it constantly resets ?). See the thread about this behaviour here :
   * https://www.smspower.org/forums/13333-UninitializedMemoryPatterns
   * 
   * Megadrive : To be confirmed. It seems that on a flashcart at least RAM gets cleared
   * but to be tested properly, this should be tested on a rewriteable cartridge.
   * */
  if (sms.territory == TERRITORY_DOMESTIC && sms.console == CONSOLE_SMS)
  {
	  memset(sms.wram, 0xF0, sizeof(sms.wram));
  }
  else
  {
	  memset(sms.wram, 0, sizeof(sms.wram));
  }

  sms.paused    = 0x00;
  sms.save      = 0x00;
  sms.fm_detect = 0x00;
  sms.ioctrl    = 0xFF;
  sms.hlatch    = 0x00;
  sms.memctrl   = 0xAB;

  /* enable Cartridge ROM by default*/
  slot.rom      = cart.rom;
  slot.pages    = cart.pages;
  slot.mapper   = cart.mapper;
  slot.fcr      = &cart.fcr[0];

  /* reset Memory Mapping */
  switch(sms.console)
  {
    case CONSOLE_COLECO:
    {
      /* $0000-$1FFF mapped to internal ROM (8K) */
      for(i = 0x00; i < 0x08; i++)
      {
        cpu_readmap[i]  = &coleco.rom[i << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, i << 10, coleco.rom);
      }

      /* $2000-$5FFF mapped to expansion */
      for(i = 0x08; i < 0x18; i++)
      {
        cpu_readmap[i]  = dummy_read;
        cpu_writemap[i] = dummy_write;
      }

      /* $6000-$7FFF mapped to RAM (1K mirrored) */
      for(i = 0x18; i < 0x20; i++)
      {
        cpu_readmap[i]  = &sms.wram[0];
        cpu_writemap[i] = &sms.wram[0];
		MemoryAddRAM(i, 0, sms.wram);
      }

      /* $8000-$FFFF mapped to Cartridge ROM (max. 32K) */
      for(i = 0x20; i < 0x40; i++)
      {
        cpu_readmap[i]  = &cart.rom[(i&0x1F) << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, (i & 0x1F) << 10, cart.rom);
      }

      /* reset I/O */
      coleco.keypad[0] = 0xf0;
      coleco.keypad[1] = 0xf0;
      coleco.pio_mode  = 0x00;

      break;
    }

	#ifdef SORDM5_EMU
    case CONSOLE_SORDM5:
	  /* Taken from http://www.retropc.net/mm/m5/memory.html */
      /* $0000-$1FFF mapped to internal ROM (8K) */
      for(i = 0x00; i < 0x08; i++)
      {
        cpu_readmap[i]  = &coleco.rom[i << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, i << 10, coleco.rom);
      }
    
      /* $2000-$5FFF mapped to cartridge ROM (max. 16K) */
      for(i = 0x08; i < 0x17; i++)
      {
        cpu_readmap[i]  = &cart.rom[i << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, i << 10, cart.rom);
      }
      
      /* $6000-$6FFF : Reserved */
      for(i = 0x17; i < 0x19; i++)
      {
        cpu_readmap[i]  = dummy_read;
        cpu_writemap[i] = dummy_write;
      }
      
      /* enable internal RAM at $7000-$7FFF (4k internal RAM) */
      for(i = 0x19; i < 0x21; i++)
      {
		cpu_readmap[i]  = &sms.wram[i << 10];
		cpu_writemap[i] = &sms.wram[i << 10];
		MemoryAddRAM(i, i << 10, sms.wram);
      }
      printf("Sord M5 mode\n");
    break;
    #endif
    case CONSOLE_SC3000:
    case CONSOLE_SF7000:
    {
      /* $0000-$7FFF mapped to cartridge ROM (max. 32K) */
      for(i = 0x00; i < 0x20; i++)
      {
        cpu_readmap[i]  = &cart.rom[i << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, i << 10, cart.rom);
      }

      /* $8000-$BFFF mapped to external RAM (lower 16K) */
      for(i = 0x20; i < 0x30; i++)
      {
        cpu_readmap[i]  = &cart.sram[(i & 0x0F) << 10];
        cpu_writemap[i] = &cart.sram[(i & 0x0F) << 10];
		MemoryAddRAM(i, (i & 0x0f) << 10, cart.sram);
      }

      /* $C000-$FFFF mapped to internal RAM (2K) or external RAM (upper 16K) */
      for(i = 0x30; i < 0x40; i++)
      {
        cpu_readmap[i]  = &cart.sram[0x4000 + ((i & 0x0F) << 10)];
        cpu_writemap[i] = &cart.sram[0x4000 + ((i & 0x0F) << 10)];
		MemoryAddRAM(i, 0x4000 + ((i & 0x0F) << 10), cart.sram);
      }

      break;
    }

    default:
    {
      /* SMS BIOS support */
      if (IS_SMS)
      {
        if (bios.enabled == 3)
        {
          /* reset BIOS paging */
          bios.fcr[0] = 0;
          bios.fcr[1] = 0;
          bios.fcr[2] = 1;
          bios.fcr[3] = 2;

          /* enable BIOS ROM */
          slot.rom    = bios.rom;
          slot.pages  = bios.pages;
          slot.mapper = MAPPER_SEGA;
          slot.fcr    = &bios.fcr[0];
          sms.memctrl = 0xE0;
        }
        else
        {
          /* save Memory Control register value in RAM */
          sms.wram[0] = sms.memctrl;
        }
      }

      /* default cartridge ROM mapping at $0000-$BFFF (first 32k mirrored) */
      for(i = 0x00; i <= 0x2F; i++)
      {
        cpu_readmap[i]  = &slot.rom[(i & 0x1F) << 10];
        cpu_writemap[i] = dummy_write;
		MemoryAddRAM(i, (i & 0x1f) << 10, slot.rom);
      }

      /* enable internal RAM at $C000-$FFFF (8k mirrored) */
      for(i = 0x30; i <= 0x3F; i++)
      {
          cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
          cpu_writemap[i] = &sms.wram[(i & 0x07) << 10];
		  MemoryAddRAM(i, (i & 0x07) << 10, sms.wram);
      }

      /* reset cartridge paging registers */
      switch(cart.mapper)
      {
        case MAPPER_NONE:
        case MAPPER_SEGA:
          cart.fcr[0] = 0;
          cart.fcr[1] = 0;
          cart.fcr[2] = 1;
          cart.fcr[3] = 2;
          break;

        default:
          cart.fcr[0] = 0;
          cart.fcr[1] = 0;
          cart.fcr[2] = 1;
          cart.fcr[3] = 0;
          break;
      }

      /* reset memory map */
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
      break;
    }
  }

  /* reset SLOT mapper */
  mapper_reset();
}

void mapper_8k_w(uint16_t address, uint8_t data)
{
	uint_fast8_t i;

	/* cartridge ROM page (8k) index */
	uint16_t page = data % (slot.pages << 1);
  
	/* Save frame control register data */
	slot.fcr[address] = data;

	switch (address & 3)
	{
		case 0: /* cartridge ROM bank (16k) at $8000-$9FFF */
		for(i = 0x20; i <= 0x27; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 13) | ((i & 0x07) << 10)];
			MemoryAddRAM(i, (page << 13) | ((i & 0x07) << 10), slot.rom);
		}
		break;
		case 1: /* cartridge ROM bank (16k) at $A000-$BFFF */
		for(i = 0x28; i <= 0x2F; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 13) | ((i & 0x07) << 10)];
			MemoryAddRAM(i, (page << 13) | ((i & 0x07) << 10), slot.rom);
		}
		break;
		case 2: /* cartridge ROM bank (16k) at $4000-$5FFF */
		for(i = 0x10; i <= 0x17; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 13) | ((i & 0x07) << 10)];
			MemoryAddRAM(i, (page << 13) | ((i & 0x07) << 10), slot.rom);
		}
		break;
		case 3: /* cartridge ROM bank (16k) at $6000-$7FFF */
		for(i = 0x18; i <= 0x1F; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 13) | ((i & 0x07) << 10)];
			MemoryAddRAM(i, (page << 13) | ((i & 0x07) << 10), slot.rom);
		}
		break;
	}
}
    
void mapper_16k_w(uint16_t address, uint8_t data)
{
	extern uint8_t gaiden_hack;
	uint_fast8_t i;

	/* cartridge ROM page (16k) index */
	uint16_t page = data % slot.pages;
  
	/* page index increment (SEGA mapper) */
	
	/* 
		About the "gaiden_hack" bit :
		I initially thought that it was a hack.
		But it turns out the translation hack actually expects this :
		https://github.com/libretro/smsplus-gx/issues/25
		
		TODO : Probably reimplement this ?
	*/
	if (slot.fcr[0] & 0x03 && gaiden_hack != 1)
	{
		page = (page + ((4 - (slot.fcr[0] & 0x03)) << 3)) % slot.pages;
	}
	
	/* save frame control register data */
	slot.fcr[address] = data;

	switch (address)
	{
		case 0: /* control register (SEGA mapper) */
		if(data & 0x08)
		{
			/* external RAM (upper or lower 16K) mapped at $8000-$BFFF */
			int32_t offset = (data & 0x04) ? 0x4000 : 0x0000;
			for(i = 0x20; i <= 0x2F; i++)
			{
				cpu_readmap[i] = cpu_writemap[i] = &cart.sram[offset + ((i & 0x0F) << 10)];
				MemoryAddRAM(i, offset + ((i & 0x0F) << 10), cart.sram);
			}
			sms.save = 1;
		}
		else
		{
			page = slot.fcr[3] % slot.pages;
			/* page index increment (SEGA mapper) */
			if (slot.fcr[0] & 0x03)
			{
				page = (page + ((4 - (slot.fcr[0] & 0x03)) << 3)) % slot.pages;
			}

			/* cartridge ROM mapped at $8000-$BFFF */
			for(i = 0x20; i <= 0x2F; i++)
			{
				cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
				cpu_writemap[i] = dummy_write;
				MemoryAddRAM(i, (page << 14) | ((i & 0x0F) << 10), slot.rom);
			}
		}
		
		if(data & 0x10)
		{
			/* external RAM (lower 16K) mapped at $C000-$FFFF */
			for(i = 0x30; i <= 0x3F; i++)
			{
			  cpu_writemap[i] = cpu_readmap[i]  = &cart.sram[(i & 0x0F) << 10];
			  MemoryAddRAM(i, (i & 0x0F) << 10, cart.sram);
			}
			sms.save = 1;
		}
		else
		{
			/* internal RAM (8K mirrorred) mapped at $C000-$FFFF */
			for(i = 0x30; i <= 0x3F; i++)
			{
			  cpu_writemap[i] = cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
			  MemoryAddRAM(i, (i & 0x07) << 10, sms.wram);
			}
		}
		break;
		case 1: /* cartridge ROM bank (16k) at $0000-$3FFF */
		/* first 1k is not fixed (CODEMASTER and 4PAK mapper) */
		if (slot.mapper == MAPPER_CODIES || slot.mapper == MAPPER_4PAK)
		{
			cpu_readmap[0] = &slot.rom[(page << 14)];
			MemoryAddRAM(0, (page << 14), slot.rom);
		}

		for(i = 0x01; i <= 0x0F; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
			MemoryAddRAM(i, (page << 14) | ((i & 0x0F) << 10), slot.rom);
		}
		break;
		case 2: /* cartridge ROM bank (16k) at $4000-$7FFF */
		for(i = 0x10; i <= 0x1F; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
			MemoryAddRAM(i, (page << 14) | ((i & 0x0F) << 10), slot.rom);
		}

		/* Ernie Elf's Golf external RAM switch */
		if (slot.mapper == MAPPER_CODIES)
		{
			if (data & 0x80)
			{
				/* external RAM (8k) mapped at $A000-$BFFF */
				for(i = 0x28; i <= 0x2F; i++)
				{
					cpu_writemap[i] = cpu_readmap[i]  = &cart.sram[(i & 0x0F) << 10];
					MemoryAddRAM(i, (i & 0x0F) << 10, cart.sram);
				}
				sms.save = 1;
			}
			else
			{
				/* cartridge ROM mapped at $A000-$BFFF */
				for(i = 0x28; i <= 0x2F; i++)
				{
					cpu_readmap[i] = &slot.rom[((slot.fcr[3] % slot.pages) << 14) | ((i & 0x0F) << 10)];
					cpu_writemap[i] = dummy_write;
					MemoryAddRAM(i, ((slot.fcr[3] % slot.pages) << 14) | ((i & 0x0F) << 10), slot.rom);
				}
			}
		}
		break;
		case 3: /* cartridge ROM bank (16k) at $8000-$BFFF */
		/* check that external RAM (16k) is not mapped at $8000-$BFFF (SEGA mapper) */
		if ((slot.fcr[0] & 0x08)) break;

		/* first 8k */
		for(i = 0x20; i <= 0x27; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
			MemoryAddRAM(i, (page << 14) | ((i & 0x0F) << 10), slot.rom);
		}

		/* check that external RAM (8k) is not mapped at $A000-$BFFF (CODEMASTER mapper) */
		if ((slot.mapper == MAPPER_CODIES) && (slot.fcr[2] & 0x80)) break;

		/* last 8k */
		for(i = 0x28; i <= 0x2F; i++)
		{
			cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
			MemoryAddRAM(i, (page << 14) | ((i & 0x0F) << 10), slot.rom);
		}
		break;
	}
}

int32_t sms_irq_callback(int32_t param)
{
	return 0xFF;
}
