/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
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

#ifndef SOUND_H
#define SOUND_H

#define BUFFER_SIZE        (1 << 16)
#define BUFFER_SIZE_MASK   (BUFFER_SIZE - 1)

/* Runtime output rate, restricted to powers of two so that all fixed point
 * frequency steps below remain exact against the 2^24 Hz master clock.
 * sound_frequency == 1u << sound_freq_bits, and is at most
 * GBA_SOUND_FREQUENCY (which stays the compile-time maximum, used for
 * buffer sizing). */
extern u32 sound_frequency;
extern u32 sound_freq_bits;

/* PSG frequency steps in 16.16 sample-index units for the current rate.
 * Tone (ch 0/1): (131072/(2048-rate))*8 / f_s  ==  2^(36-b)/(2048-rate)
 * Wave (ch 2):   double the tone clock         ==  2^(37-b)/(2048-rate)
 * Noise (ch 3):  524288/(r * 2^(s+1)) / f_s; r==0 behaves as r=1/2.
 * All exact shift-and-divides; b=16 reproduces the historical constants
 * 1048576, 2097152, 1048576>>(s+1) and 524288. */
static INLINE fixed16_16 psg_tone_step(u32 rate)
{
   return (fixed16_16)((1u << (36 - sound_freq_bits)) / (2048 - rate));
}

static INLINE fixed16_16 psg_wave_step(u32 rate)
{
   return (fixed16_16)((1u << (37 - sound_freq_bits)) / (2048 - rate));
}

static INLINE fixed16_16 psg_noise_step(u32 dividing_ratio, u32 freq_shift)
{
   if(dividing_ratio == 0)
      return (fixed16_16)((1u << (36 - sound_freq_bits)) >> (freq_shift + 1));
   return (fixed16_16)((1u << (35 - sound_freq_bits)) /
                       (dividing_ratio << (freq_shift + 1)));
}

/* Recompute all rate-derived frequency steps from primary state (defined in
 * gba_memory.c since it needs timer state). Call on an output-rate change
 * and after loading a savestate. */
void sound_frequency_changed(void);

/* Drop pending ring contents and realign producers/consumer (rate change). */
void sound_flush_ring(void);

#define GBA_SOUND_FREQUENCY   (64 * 1024)

#ifdef OVERCLOCK_60FPS
  #define GBC_BASE_RATE ((float)(60 * 228 * (272+960)))
  /* Integer companion used by the deterministic audio path. Keeping the
   * float form above for the frontend timing field / RTC / rumble, which
   * legitimately want float results. */
  #define GBC_BASE_RATE_INT ((u32)(60 * 228 * (272+960)))
#else
  #define GBC_BASE_RATE ((float)(16 * 1024 * 1024))
  #define GBC_BASE_RATE_INT ((u32)(16 * 1024 * 1024))
#endif

#define DIRECT_SOUND_INACTIVE         0
#define DIRECT_SOUND_RIGHT            1
#define DIRECT_SOUND_LEFT             2
#define DIRECT_SOUND_LEFTRIGHT        3

typedef struct
{
   s8 fifo[32];
   u32 fifo_base;
   u32 fifo_top;
   fixed8_24 fifo_fractional;
   // The + 1 is to give some extra room for linear interpolation
   // when wrapping around.
   u32 buffer_index;
   u32 status;
   u32 volume_halve;
} direct_sound_struct;

#define GBC_SOUND_INACTIVE            0
#define GBC_SOUND_RIGHT               1
#define GBC_SOUND_LEFT                2
#define GBC_SOUND_LEFTRIGHT           3


typedef struct
{
   u32 rate;
   fixed16_16 frequency_step;
   fixed16_16 sample_index;
   fixed16_16 tick_counter;
   u32 total_volume;
   u32 envelope_initial_volume;
   u32 envelope_volume;
   u32 envelope_direction;
   u32 envelope_status;
   u32 envelope_ticks;
   u32 envelope_initial_ticks;
   u32 sweep_status;
   u32 sweep_direction;
   u32 sweep_ticks;
   u32 sweep_initial_ticks;
   u32 sweep_shift;
   u32 length_status;
   u32 length_ticks;
   u32 noise_type;
   u32 wave_type;
   u32 wave_bank;
   u32 wave_volume;
   u32 status;
   u32 active_flag;
   u32 master_enable;
   u32 sample_table_idx;
} gbc_sound_struct;

const extern s8 square_pattern_duty[4][8];
extern direct_sound_struct direct_sound_channel[2];
extern gbc_sound_struct gbc_sound_channel[4];
extern u32 gbc_sound_master_volume_left;
extern u32 gbc_sound_master_volume_right;
extern u32 gbc_sound_master_volume;
extern u32 gbc_sound_buffer_index;
extern u32 gbc_sound_last_cpu_ticks;

extern u32 sound_on;

void sound_timer_queue32(u32 channel, u32 value);
unsigned sound_timer(fixed8_24 frequency_step, u32 channel);
void sound_reset_fifo(u32 channel);
void render_gbc_sound();
void init_sound();

bool sound_check_savestate(const u8 *src);
unsigned sound_write_savestate(u8 *dst);
bool sound_read_savestate(const u8 *src);

u32 sound_read_samples(s16 *out, u32 frames);

void reset_sound(void);

#endif
