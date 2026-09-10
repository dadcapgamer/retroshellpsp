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

#ifndef COMMON_H
#define COMMON_H

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include <psprtc.h>

#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


#define MAX_PATH (512)
#define MAX_FILE (256)

#define ALIGN_PSPDATA __attribute__((aligned(16)))
#define ALIGN_DATA    __attribute__((aligned(64)))

#define MEM_ALIGN     (4)

#define ROR(dest, value, shift)                                               \
  dest = __builtin_allegrex_rotr(value, shift)                                \

#define LIMIT_MAX(input, maxout)                                              \
  __builtin_allegrex_min(input, maxout)                                       \

#define LIMIT_MIN(input, minout)                                              \
  __builtin_allegrex_max(input, minout)                                       \


#define PSP_FILE_OPEN_READ  "r"
#define PSP_FILE_OPEN_WRITE "w"

#define FILE_OPEN(filename_tag, filename, mode)                               \
  filename_tag = psp_fopen(filename, PSP_FILE_OPEN_##mode)                    \

#define FILE_CHECK_VALID(filename_tag)                                        \
  ((filename_tag) >= 0)                                                       \

#define FILE_CLOSE(filename_tag)                                              \
  psp_fclose(filename_tag)                                                    \

#define FILE_READ(filename_tag, buffer, size)                                 \
  sceIoRead(filename_tag, buffer, size)                                       \

#define FILE_WRITE(filename_tag, buffer, size)                                \
  sceIoWrite(filename_tag, buffer, size)                                      \

// type = FILE_OPEN_READ / FILE_OPEN_WRITE
#define FILE_SEEK(filename_tag, offset, type)                                 \
  sceIoLseek(filename_tag, offset, PSP_##type)                                \

#define MEM_WRITE(var_ptr, size)                                 \
{                                                                             \
  memcpy(gba_state_write_ptr, var_ptr, size);                                              \
  gba_state_write_ptr += (size);                                                          \
}                                                                             \

#define MEM_READ(var_ptr, size)                                  \
{                                                                             \
  memcpy(var_ptr, gba_state_read_ptr, size);                                              \
  gba_state_read_ptr += (size);                                                          \
}

// These must be variables, not constants.

#define MEM_READ_VARIABLE(variable)                             \
  MEM_READ(&variable, sizeof(variable))                        \

#define MEM_WRITE_VARIABLE(variable)                            \
  MEM_WRITE(&variable, sizeof(variable))                        \

// These must be statically declared arrays (ie, global or on the stack,
// not dynamically allocated on the heap)


#define MEM_READ_ARRAY(array)                                   \
  MEM_READ(array, sizeof(array))                               \

#define MEM_WRITE_ARRAY(array)                                  \
  MEM_WRITE(array, sizeof(array))                               \


typedef u32 FIXED08_24;

#define FLOAT_TO_FP08_24(value)                                               \
  (FIXED08_24)((value) * 16777216.0)                                          \

#define FP08_24_TO_FLOAT(value)                                               \
  (float)((value) / 16777216.0)                                               \

#define U32_TO_FP08_24(value)                                                 \
  ((value) << 24)                                                             \

#define FP08_24_TO_U32(value)                                                 \
  ((value) >> 24)                                                             \

#define FP08_24_FRACTIONAL_PART(value)                                        \
  ((value) & 0x00FFFFFF)                                                      \


#define FIXED_DIV(numerator, denominator, bits)                               \
  (((numerator * (1 << bits)) + (denominator / 2)) / denominator)             \


#define ADDRESS8(base, offset)                                                \
  *((u8 *)((u8 *)(base) + (offset)))                                          \

#define ADDRESS16(base, offset)                                               \
  *((u16 *)((u8 *)(base) + (offset)))                                         \

#define ADDRESS32(base, offset)                                               \
  *((u32 *)((u8 *)(base) + (offset)))                                         \


#include "cpu.h"
#include "memory.h"
#include "video.h"
#include "input.h"
#include "sound.h"
#include "main.h"
#include "zip.h"

void switch_to_main_thread(void);
extern volatile bool save_state_loaded;
extern const u8* gba_state_read_ptr;
extern u8* gba_state_write_ptr;

#include "video_ge.h"
#ifdef HW_RENDER_TEST
extern int ge_render_enable;
#endif

#ifdef PERF_TEST

extern struct retro_perf_callback perf_cb;

#define RETRO_PERFORMANCE_INIT(X) \
   static struct retro_perf_counter X = {#X}; \
   do { \
      if (!(X).registered) \
         perf_cb.perf_register(&(X)); \
   } while(0)

#define RETRO_PERFORMANCE_START(X) perf_cb.perf_start(&(X))
#define RETRO_PERFORMANCE_STOP(X) perf_cb.perf_stop(&(X))
#else
#define RETRO_PERFORMANCE_INIT(X)
#define RETRO_PERFORMANCE_START(X)
#define RETRO_PERFORMANCE_STOP(X)

#endif

#endif /* COMMON_H */
