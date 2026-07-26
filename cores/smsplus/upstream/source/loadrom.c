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
 *   ROM File Loading support
 *
 ******************************************************************************/
 
/*
 * See git commit history for more information.
 * - Gameblabla
 * July 16th 2019 : Add 4PAK support to the list. It also makes use of PAL mode.
 * June 6th 2019 : Add Blockhole & Alibaba to the list as they rely on the Japanese BIOS and its uninitiliazed memory.
 * March 14th 2019 : Add NOZIP for building without zip support.
 * March 11th 2019 : Fantastic Dizzy should run in PAL mode to avoid issues.
 * August 12th 2018 : Add Bad Apple to the list. (plus minor fixes)
*/

#include "shared.h"

uint8_t gaiden_hack = 0;

#define GAME_DATABASE_CNT 106

typedef struct
{
	uint32_t crc;
	uint8_t glasses_3d;
	uint8_t device;
	uint8_t mapper;
	uint8_t display;
	uint8_t territory;
	uint8_t console;
	uint8_t fm_compatible;
	const char *name;
} rominfo_t;

static rominfo_t game_list[GAME_DATABASE_CNT] =
{
	/* Games requiring CODEMASTER mapper */
	{0x29822980, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Cosmic Spacehead"},
	{0x6CAA625B, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GG, FM_COMPATIBLE,
	"Cosmic Spacehead (GG)"}, 
	{0xEA5C3A6F, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Dinobasher - Starring Bignose the Caveman [Proto]"}, 
	{0x152F0DCC, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Drop Zone"}, 
	{0x5E53C7F7, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GG, FM_COMPATIBLE,
	"Ernie Els Golf"}, 
	{0x8813514B, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Excellent Dizzy Collection, The [Proto]"},
	{0xAA140C9C, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Excellent Dizzy Collection, The [SMS-GG]"},
	{0xB9664AE1, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Fantastic Dizzy"},
	{0xC888222B, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Fantastic Dizzy [SMS-GG]"},
	{0x76C5BDFB, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Jang Pung 2 [SMS-GG]"},
	{0xD9A7F170, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Man Overboard!"}, 
	{0xA577CE46, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Micro Machines"}, 
	{0xF7C524F6, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Micro Machines [BAD DUMP]"}, 
	{0xDBE8895C, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Micro Machines 2 - Turbo Tournament"},
	{0xC1756BEE, 0, DEVICE_PAD2B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Pete Sampras Tennis"},

	/* Games requiring KOREA mappers */
	{0x17AB6883, 0, DEVICE_PAD2B, MAPPER_NONE, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"FA Tetris (KR)"},
	{0x61E8806F, 0, DEVICE_PAD2B, MAPPER_NONE, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Flash Point (KR)"},
	{0x89B79E77, 0, DEVICE_PAD2B, MAPPER_KOREA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Dodgeball King (KR)"},
	{0x18FB98A3, 0, DEVICE_PAD2B, MAPPER_KOREA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Jang Pung 3 (KR)"},
	{0x97D03541, 0, DEVICE_PAD2B, MAPPER_KOREA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sangokushi 3 (KR)"},
	{0x67C2F0FF, 0, DEVICE_PAD2B, MAPPER_KOREA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Super Boy 2 (KR)"},
	{0x445525E2, 0, DEVICE_PAD2B, MAPPER_KOREA_MSX, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Penguin Adventure (KR)"},
	{0x83F0EEDE, 0, DEVICE_PAD2B, MAPPER_KOREA_MSX, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Street Master (KR)"},
	{0xA05258F5, 0, DEVICE_PAD2B, MAPPER_KOREA_MSX, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Won-Si-In (KR)"},
	{0x06965ED9, 0, DEVICE_PAD2B, MAPPER_KOREA_MSX, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"F-1 Spirit - The way to Formula-1 (KR)"},

	/* Games that require PAL timings (from MEKA.nam by Omar Cornut) */
	{0x72420F38, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Addams Familly"},
	{0x2D48C1D3, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Back to the Future Part III"},
	{0x1CBB7BF1, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Battlemaniacs (BR)"}, 
	{0x1B10A951, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Bram Stoker's Dracula"},
	{0xC0E25D62, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"California Games II"}, 
	{0xC9DBF936, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Home Alone"},
	{0xA109A6FE, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Power Strike II"},
	{0x4FF0CEC7, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Power Strike II (Game Gear Micro)"},
	
	{0x0047B615, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Predator2"},
	{0xF42E145C, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Quest for the Shaven Yak Starring Ren Hoek & Stimpy (BR)"}, 
	{0x9F951756, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"RoboCop 3"}, 
	{0x1575581D, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Shadow of the Beast"}, 
	{0x96B3F29E, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sonic Blast (BR)"}, 
	{0x5B3B922C, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sonic the Hedgehog 2 [V0]"},
	{0xD6F2BFCA, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sonic the Hedgehog 2 [V1]"},
	{0xCA1D3752, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Space Harrier [50 Hz]"}, 
	{0x85CFC9C9, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Taito Chase H.Q."},  
	{0x38434560, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Bad Apple SMS"},  
	{0xDA2A68C6, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"The Synchrobots"},  

	/* Games requiring 315-5124 VDP (Mark-III, Sega Master System) */
	{0x32759751, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Y's (J)"},

	/* Games requiring Game Gear SMS compatibility mode */
	{0x59840FD6, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Castle of Illusion - Starring Mickey Mouse"},
	{0x9942B69B, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Castle of Illusion - Starring Mickey Mouse (J)"},
	{0x5877B10D, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Castle of Illusion - Starring Mickey Mouse (J) [HACK]"},
	{0x9C76FB3A, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Rastan Saga [SMS-GG]"},
	{0x7BB81E3D, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Taito Chase H.Q. [SMS-GG]"},
	{0x44FBE8F6, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Taito Chase H.Q. [SMS-GG][HACK]"},
	{0x18086B70, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Taito Chase H.Q. [SMS-GG][HACK][BAD]"},
	{0xDA8E95A9, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"WWF Wrestlemania Steel Cage Challenge [SMS-GG]"},
	{0xCB42BD33, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"WWF Wrestlemania Steel Cage Challenge [SMS-GG] [BAD DUMP]"},
	{0x1D93246E, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Olympic Gold [SMS-GG] [A]"},
	{0xA2f9C7AF, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Olympic Gold [SMS-GG] [B]"},
	{0xF037EC00, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Out Run Europa [SMS-GG]"},
	{0xE5F789B9, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Predator 2 [SMS-GG]"},
	{0x311D2863, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Prince of Persia [SMS-GG] [A]"},
	{0x45F058d6, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Prince of Persia [SMS-GG] [B]"},
	{0x56201996, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"R.C. Grand Prix [SMS-GG]"},
	{0x10DBBEF4, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Super Kick Off [SMS-GG]"},
	{0xBD1CC7DF, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_GGMS, FM_NOT_COMPATIBLE,
	"Super Tetris (KR)"},
	
	/* Games requiring uninitialized work RAM due to Japanese BIOS not clearing memory. */
	{0x08BF3DE3, 0, DEVICE_PAD2B, MAPPER_NONE, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Alibaba and 40 Thieves"},
	{0x643B6B76, 0, DEVICE_PAD2B, MAPPER_NONE, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Block Hole"},
	
	/* 4-PAK mapper for Australian exclusive */
	{0xA67F2A5C, 0, DEVICE_PAD2B, MAPPER_4PAK, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"4 PAK All Action"},

	/* Games requiring 3D Glasses */
	{0xFBF96C81, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Blade Eagle 3-D (BR)"},
	{0x8ECD201C, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Blade Eagle 3-D"},
	{0x31B8040B, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Maze Hunter 3-D"},
	{0x871562b0, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Maze Walker"},
	{0xABD48AD2, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Poseidon Wars 3-D"},
	{0x6BD5C2BF, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Space Harrier 3-D"},
	{0x156948f9, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Space Harrier 3-D (J)"},
	{0xA3EF13CB, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Zaxxon 3-D"},
	{0xBBA74147, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Zaxxon 3-D [Proto]"},
	{0xD6F43DDA, 1, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Out Run 3-D"},

	/* Games requiring Light Phaser & 3D Glasses */
	{0xFBE5CFBB, 1, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Missile Defense 3D"},
	{0xE79BB689, 1, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Missile Defense 3D [BIOS]"},
  
	/* Games requiring Light Phaser */
	{0x861B6E79, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Assault City [Light Phaser]"},
	{0x5FC74D2A, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Gangster Town"},
	{0xE167A561, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Hang-On / Safari Hunt"},
	{0xC5083000, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Hang-On / Safari Hunt [BAD DUMP]"},
	{0x91E93385, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Hang-On / Safari Hunt [BIOS]"},
	{0xE8EA842C, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Marksman Shooting / Trap Shooting"},
	{0xE8215C2E, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Marksman Shooting / Trap Shooting / Safari Hunt"},
	{0x205CAAE8, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_PAL, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Operation Wolf"}, /* Can be also played using the PLAYER2 gamepad */
	{0x23283F37, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Operation Wolf [A]"}, /* Can be also played using the PLAYER2 gamepad */
	{0xDA5A7013, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Rambo 3"},
	{0x79AC8E7F, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Rescue Mission"},
	{0x4B051022, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Shooting Gallery"},
	{0xA908CFF5, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Spacegun"},
	/* This game won't work with an FM board connected */
	{0x5359762D, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_NOT_COMPATIBLE,
	"Wanted"},
	{0x0CA95637, 0, DEVICE_LIGHTGUN, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Laser Ghost"},

	/* Games requiring Paddle */
	{0xF9DBB533, 0, DEVICE_PADDLE, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Alex Kidd BMX Trial"},
	{0xA6FA42D0, 0, DEVICE_PADDLE, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Galactic Protector"},
	{0x29BC7FAD, 0, DEVICE_PADDLE, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Megumi Rescue"},
	{0x315917D4, 0, DEVICE_PADDLE, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_COMPATIBLE,
	"Woody Pop"},

	/* Games requiring Sport Pad (NOT EMULATED YET) */
	{0x946B8C4A, 0, DEVICE_SPORTSPAD, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Great Ice Hockey"},
	{0xE42E4998, 0, DEVICE_SPORTSPAD, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sports Pad Football"},
	{0x41C948BF, 0, DEVICE_SPORTSPAD, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_SMS2, FM_COMPATIBLE,
	"Sports Pad Soccer"},
   
	/* Games using FM sound only when a certain region is set. Not required for the games to be playable
	* but most people prefer the FM soundtrack anyway. */
	{0x679E1676, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Wonder Boy III - The dragon's Trap"},
	{0x22CCA9BB, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS2, FM_COMPATIBLE,
	"Turma da Monica em O Resgate"},
	{0x23BAC434, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_EXPORT, CONSOLE_GG, FM_NOT_COMPATIBLE,
	"Shining Force Final Conflict"},
	
	/* These games have control issues if there's an FM board connected. */
	{0xE6795C53, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_NOT_COMPATIBLE,
	"Fushigi no Oshiro Pit Pot"},
	{0x89E98A7C, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_NOT_COMPATIBLE,
	"Great Baseball (Japan)"},
	{0x316727DD, 0, DEVICE_PAD2B, MAPPER_SEGA, DISPLAY_NTSC, TERRITORY_DOMESTIC, CONSOLE_SMS, FM_NOT_COMPATIBLE,
	"Teddy Boy Blues (Japan)"},
	
};

static void set_config()
{
	uint32_t i;

	/* default sms settings */
	cart.mapper = MAPPER_SEGA;
	sms.console = CONSOLE_SMS2;
	sms.territory = TERRITORY_EXPORT;
	sms.display = DISPLAY_NTSC;
	sms.glasses_3d = 0;
	sms.device[0] = DEVICE_PAD2B;
	sms.device[1] = DEVICE_PAD2B;
	sms.use_fm = option.fm;
	gaiden_hack = 0;

	/* console type detection */
	/* SMS Header is located at 0x7ff0 */
	if ((cart.size > 0x7000) && (!memcmp (&cart.rom[0x7ff0], "TMR SEGA", 8)))
	{
		uint8_t region = (cart.rom[0x7fff] & 0xf0) >> 4;
		switch (region)
		{
		  case 5:
			sms.console = CONSOLE_GG;
			sms.territory = TERRITORY_DOMESTIC;
			break;

		  case 6:
		  case 7:
			sms.console = CONSOLE_GG;
			sms.territory = TERRITORY_EXPORT;
			break;

		  case 3:
			sms.console = CONSOLE_SMS;
			sms.territory = TERRITORY_DOMESTIC;
			break;
		
		  default:
			sms.console = CONSOLE_SMS2;
			sms.territory = TERRITORY_EXPORT;
			break;
		}
	}

	sms.gun_offset = 20; /* default offset */

	/* retrieve game settings from database */
	for (i = 0; i < GAME_DATABASE_CNT; i++)
	{
		if (cart.crc == game_list[i].crc)
		{
			cart.mapper = game_list[i].mapper;
			sms.display = game_list[i].display;
			sms.territory = game_list[i].territory;
			sms.glasses_3d = game_list[i].glasses_3d;
			sms.console =  game_list[i].console;
			sms.device[0] = game_list[i].device;
			sms.use_fm = game_list[i].fm_compatible;
			
			if (game_list[i].device != DEVICE_LIGHTGUN) sms.device[1] = game_list[i].device;
			
			/* Games's specific hacks */
			
			if ((strcmp(game_list[i].name, "Spacegun") == 0) ||
			(strcmp(game_list[i].name, "Gangster Town") == 0))
			{
				/* these games seem to use different gun position calculation method */
				sms.gun_offset = 16;
			}
			
			if (strcmp(game_list[i].name, "Shining Force Final Conflict") == 0)
			{
				gaiden_hack = 1;
			}
			
			i = GAME_DATABASE_CNT;
		}
	}

	/* enable BIOS on SMS only */
	bios.enabled &= 2;

	/* force settings if AUTO is not set*/
	switch(option.console)
	{
		case 1:
			sms.console = CONSOLE_SMS;
		break;
		case 2:
			sms.console = CONSOLE_SMS2;
		break;
		case 3:
			sms.console = CONSOLE_GG;
		break;
		case 4:
			sms.console = CONSOLE_GGMS;
		break;
		case 5:
			sms.console = CONSOLE_SG1000;
			cart.mapper = MAPPER_NONE;
		break;
		case 6:
			sms.console = CONSOLE_COLECO;
			cart.mapper = MAPPER_NONE;
		break;
		#ifdef SORDM5_EMU
		case 7:
			sms.console = CONSOLE_SORDM5;
			cart.mapper = MAPPER_NONE;
		break;
		#endif
	}
  
	switch(option.country)
	{
		/* USA */
		case 1:
			sms.display = DISPLAY_NTSC;
			sms.territory = TERRITORY_EXPORT;
		break;
		/* EUROPE */
		case 2:
			sms.display = DISPLAY_PAL;
			sms.territory = TERRITORY_EXPORT;
		break;
		/* JAPAN */
		case 3:
			sms.display = DISPLAY_NTSC;
			sms.territory = TERRITORY_DOMESTIC;
		break;
	}
}

#ifdef NOZIP_SUPPORT
/* Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C implementation that balances processor cache usage against speed": http://www.geocities.com/malbrain/ */
static uint32_t crc32(uint32_t crc, const uint8_t *ptr, size_t buf_len)
{
	static const uint32_t s_crc32[16] = { 0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c };
	uint32_t crcu32 = (uint32_t)crc;
	crcu32 = ~crcu32;
	while (buf_len--)
	{
		uint8_t b = *ptr++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)];
	}
	return ~crcu32;
}
#endif


void free_rom(void)
{
	if (cart.rom)
	{
		free(cart.rom);
		cart.rom = NULL;
	}
}

uint32_t load_rom (char *filename)
{
	free_rom();

#ifndef NOZIP_SUPPORT
	if(check_zip(filename))
	{
		char name[PATH_MAX];
		cart.rom = loadFromZipByName((char*)filename, name, &cart.size);
		if (!cart.rom)
			return 0;
	}
	else
#endif
	{
		FILE *fd = NULL;
		fd = fopen(filename, "rb");
		if (!fd) 
			return 0;

		/* Seek to end of file, and get size */
		fseek(fd, 0, SEEK_END);
		cart.size = (uint32_t)ftell(fd);
		fseek(fd, 0, SEEK_SET);

		if (cart.size < 0x4000) cart.size = 0x4000;
		cart.rom = malloc(cart.size);
		if (!cart.rom)
		{
			fclose(fd);
			return 0;
		}
		fread(cart.rom, cart.size, 1, fd);

		fclose(fd);
	}

	/* Take care of image header, if present */
	if ((cart.size / 512) & 1)
	{
		cart.size -= 512;
		memcpy (cart.rom, cart.rom + 512, cart.size);
	}

	/* 16k pages */
	cart.pages = cart.size / 0x4000;

	cart.crc = crc32 (0, cart.rom, cart.size);
	cart.loaded = 1;

	set_config();

	return 1;
}

uint32_t load_rom_mem (const char *data, size_t size)
{
	if(cart.rom)
	{
		free(cart.rom);
		cart.rom = NULL;
	}

	cart.size = size;
	if (cart.size < 0x4000) cart.size = 0x4000;
	cart.rom = malloc(cart.size);	

	/* Take care of image header, if present */
	if ((cart.size / 512) & 1)
	{
		cart.size -= 512;
		memcpy (cart.rom, data + 512, size - 512);
	}
	else
		memcpy (cart.rom, data, size);	

	/* 16k pages */
	cart.pages = cart.size / 0x4000;

	cart.crc = crc32 (0, cart.rom, cart.size);
	cart.loaded = 1;

	set_config();

	return 1;
}
