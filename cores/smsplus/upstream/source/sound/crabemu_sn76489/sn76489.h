/*
    This file is part of CrabEmu.

    Copyright (C) 2005, 2006, 2007, 2008, 2012 Lawrence Sebald

    CrabEmu is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 
    as published by the Free Software Foundation.

    CrabEmu is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CrabEmu; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SN76489_H
#define SN76489_H

#include <stdint.h>

typedef struct sn76489_struct {
    uint8_t volume[4];
    uint16_t tone[3];
    uint8_t noise;

    uint16_t noise_shift;
    uint16_t noise_bits;
    uint16_t noise_tapped;

    int8_t tone_state[4];

    uint8_t latched_reg;

    float counter[4];

    uint8_t enabled_channels;

    uint8_t output_channels;
    uint32_t channel_masks[2][4];

    float clocks_per_sample;
} sn76489_t;

#define LATCH_TONE0 0x00
#define LATCH_TONE1 0x20
#define LATCH_TONE2 0x40
#define LATCH_NOISE 0x60

#define LATCH_VOL0 0x10
#define LATCH_VOL1 0x30
#define LATCH_VOL2 0x50
#define LATCH_VOL3 0x70

#define ENABLE_TONE0 0x01
#define ENABLE_TONE1 0x02
#define ENABLE_TONE2 0x04
#define ENABLE_NOISE 0x08

/* Channel outputs */
#define TONE0_RIGHT 0x01
#define TONE1_RIGHT 0x02
#define TONE2_RIGHT 0x04
#define NOISE_RIGHT 0x08
#define TONE0_LEFT  0x10
#define TONE1_LEFT  0x20
#define TONE2_LEFT  0x40
#define NOISE_LEFT  0x80

/* Default settings */
#define SN76489_NOISE_TAPPED_NORMAL 0x0006
#define SN76489_NOISE_BITS_NORMAL   15

#define SN76489_NOISE_TAPPED_SMS    0x0009
#define SN76489_NOISE_BITS_SMS      16

#define SN76489_NOISE_TAPPED_SG1000 SN76489_NOISE_TAPPED_NORMAL
#define SN76489_NOISE_BITS_SG1000   SN76489_NOISE_BITS_NORMAL

uint32_t sn76489_init(sn76489_t *psg, float clock, float sample_rate, uint16_t noise_bits, uint16_t tapped);
uint32_t sn76489_reset(sn76489_t *psg, float clock, float sample_rate, uint16_t noise_bits, uint16_t tapped);
void sn76489_write(sn76489_t *psg, uint8_t byte);

void sn76489_execute_samples(sn76489_t *psg, int16_t *bufl, int16_t *bufr, uint32_t samples) ;
void sn76489_set_output_channels(sn76489_t *psg, uint8_t data);
int32_t SN76489_GetContextSize(void);
void SN76489_SetContext(uint8_t* data);


#endif
