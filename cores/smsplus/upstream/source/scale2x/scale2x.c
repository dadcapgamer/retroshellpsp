/* scaler.c: code for selecting (etc) scalers
 * Copyright (C) 2003 Fredrick Meunier, Philip Kendall
 * Copyright (c) 2015 Sergio Baldoví
 * 
 * $Id: scaler.c 5432 2016-05-01 04:16:09Z fredm $
 *
 * Originally taken from ScummVM - Scumm Interpreter
 * Copyright (C) 2001  Ludvig Strigeus
 * Copyright (C) 2001/2002 The ScummVM project
 *
 * HQ2x and HQ3x scalers taken from HiEnd3D Demos (http://www.hiend3d.com)
 * Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include "scale2x.h"

/* No license ? Scale2x was licensed under the GPLv2 and this code was found among in GPLv2 licensed code.
 * I had to make some minor modifications to it and the code is fairly small but i think we should be good. */

void scale2x(uint16_t* restrict srcpixels, uint16_t* restrict dstpixels, const uint32_t srcpitch, const uint32_t dstpitch, const uint32_t width, uint32_t height)
{
	uint32_t nextlineSrc = srcpitch / sizeof(uint16_t);
	uint16_t* restrict p = (uint16_t* restrict)srcpixels;

	uint32_t nextlineDst = dstpitch / sizeof(uint16_t);
	uint16_t* restrict q = (uint16_t* restrict)dstpixels;
	
	uint32_t i, j;
	uint16_t B, D, E, F, H;
	
	while(height--) 
	{
		j = 0;
		for(i = 0; i < width; ++i, j += 2) {
			B = *(p + i - nextlineSrc);
			D = *(p + i - 1);
			E = *(p + i);
			F = *(p + i + 1);
			H = *(p + i + nextlineSrc);

			*(q + j) = (uint16_t)(D == B && B != F && D != H ? D : E);
			*(q + j + 1) = (uint16_t)(B == F && B != D && F != H ? F : E);
			*(q + j + nextlineDst) = (uint16_t)(D == H && D != B && H != F ? D : E);
			*(q + j + nextlineDst + 1) = (uint16_t)(H == F && D != H && B != F ? F : E);
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}

}
