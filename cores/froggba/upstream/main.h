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

#ifndef MAIN_H
#define MAIN_H


// video scale type
#define SCALED_NONE   0
#define SCALED_X15_GU 1
#define SCALED_X15_SW 2
#define SCALED_USER   3

// video filter type
#define FILTER_NEAREST  0
#define FILTER_BILINEAR 1

// frameskip type
#define FRAMESKIP_AUTO   0
#define FRAMESKIP_MANUAL 1
#define FRAMESKIP_NONE   2

// psp cpu clock frequency
#define PSP_CLOCK_222 0
#define PSP_CLOCK_266 1
#define PSP_CLOCK_300 2
#define PSP_CLOCK_333 3

#define CONFIRMATION_NONE  0
#define CONFIRMATION_CONT  1
#define CONFIRMATION_QUIT  2


extern u32 option_screen_scale;
extern u32 option_screen_mag;
extern u32 option_screen_filter;
extern u32 option_sound_volume;
extern u32 option_stack_optimize;
extern u32 option_boot_mode;
extern u32 option_update_backup;
extern u32 option_screen_capture_format;
extern u32 option_enable_analog;
extern u32 option_analog_sensitivity;
extern u32 option_language;

extern u32 option_clock_speed;

extern u32 skip_next_frame;

extern char main_path[MAX_PATH];
extern char dir_save[MAX_PATH];

extern int date_format;
extern u32 enable_home_menu;

extern u32 sleep_flag;

void direct_sound_timer_select(u32 value);

void timer_control_low(u8 timer_number, u32 value);
CPU_ALERT_TYPE timer_control_high(u8 timer_number, u32 value);

extern u32 dma_cycle_count;

u32 update_gba(void);

void reset_gba(void);
void quit_gba(void);

void error_msg(const char *text);
void info_msg(const char *text);

void change_ext(char *src, char *buffer, const char *extension);

u32 file_length(const char *filename);

int set_cpu_clock(u32 psp_clock);

SceUID psp_fopen(const char *filename, const char *mode);
void psp_fclose(SceUID filename_tag);

void *safe_malloc(size_t size);

void main_write_savestate(void);
void main_read_savestate(void);


#endif /* MAIN_H */
