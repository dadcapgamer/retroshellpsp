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
 *  Video Display Processor (VDP) emulation.
 *
 ******************************************************************************/
#ifndef VDP_H_
#define VDP_H_

/*
    vdp1

    mode 4 when m4 set and m1 reset

    vdp2

    mode 4 when m4 set and m2,m1 != 1,0


*/

/* VDP context */
typedef struct
{
    uint8_t vram[0x4000];
    uint8_t cram[0x40]; 
    uint8_t reg[0x10];
    uint8_t vscroll;
    uint8_t status;
    uint8_t latch;
    uint8_t pending;
    uint8_t code;
    uint8_t buffer;
    uint8_t height;
    uint8_t extended;
    uint8_t irq;
    uint8_t vint_pending;
    uint8_t hint_pending;
    uint8_t spr_ovr;
    uint8_t bd;
    uint16_t lpf;
    uint16_t cram_latch;
    uint16_t addr;
    int32_t pn, ct, pg, sa, sg;
    int32_t ntab;
    int32_t satb;
    int32_t line;
    int32_t left;
    int32_t spr_col;
    int32_t mode;
} vdp_t;

/* Global data */
extern vdp_t vdp;
extern uint8_t hc_256[228];

/* Function prototypes */
extern void vdp_init(void);
extern void vdp_shutdown(void);
extern void vdp_reset(void);
extern void viewport_check(void);
extern uint8_t vdp_counter_r(int32_t offset);
extern uint8_t vdp_read(int32_t offset);
extern void vdp_write(int32_t offset, uint8_t data);
extern void gg_vdp_write(int32_t offset, uint8_t data);
extern void md_vdp_write(int32_t offset, uint8_t data);
extern void tms_write(int32_t offset, uint8_t data);

#endif /* _VDP_H_ */

