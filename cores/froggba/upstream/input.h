/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef INPUT_H
#define INPUT_H

#include "libretro.h"

typedef struct
{
   unsigned retropad ;
   unsigned gba;
} map;
static const map btn_map[] = {
   { RETRO_DEVICE_ID_JOYPAD_L,      0x0200 },
   { RETRO_DEVICE_ID_JOYPAD_R,      0x0100 },
   { RETRO_DEVICE_ID_JOYPAD_DOWN,   0x0080 },
   { RETRO_DEVICE_ID_JOYPAD_UP,     0x0040 },
   { RETRO_DEVICE_ID_JOYPAD_LEFT,   0x0020 },
   { RETRO_DEVICE_ID_JOYPAD_RIGHT,  0x0010 },
   { RETRO_DEVICE_ID_JOYPAD_START,  0x0008 },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, 0x0004 },
   { RETRO_DEVICE_ID_JOYPAD_B,      0x0002 },
   { RETRO_DEVICE_ID_JOYPAD_A,      0x0001 }
};

extern u32 enable_tilt_sensor;
extern u32 tilt_sensorX;
extern u32 tilt_sensorY;

void update_input(void);

void input_read_savestate(void);
void input_write_savestate(void);

#endif /* INPUT_H */
