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


#include "common.h"


/* RetroShell patches the stack fast path to resolve the actual mapped GBA RAM
 * page rather than assuming every stack address belongs to IWRAM. */
u32 option_stack_optimize = 1;
u32 option_boot_mode = 0;

char main_path[MAX_PATH];
char dir_save[MAX_PATH];

typedef enum
{
  TIMER_INACTIVE,
  TIMER_PRESCALE,
  TIMER_CASCADE
} TIMER_STATUS_TYPE;

typedef enum
{
  TIMER_NO_IRQ,
  TIMER_TRIGGER_IRQ
} TIMER_IRQ_TYPE;

typedef enum
{
  TIMER_DS_CHANNEL_NONE,
  TIMER_DS_CHANNEL_A,
  TIMER_DS_CHANNEL_B,
  TIMER_DS_CHANNEL_BOTH
} TIMER_DS_CHANNEL_TYPE;

typedef struct
{
  TIMER_DS_CHANNEL_TYPE direct_sound_channels;
  TIMER_IRQ_TYPE irq;
  TIMER_STATUS_TYPE status;
  FIXED08_24 frequency_step;
  s32 count;
  u32 reload;
  u32 reload_update;
  u32 prescale;
} TimerType;

const u8 ALIGN_DATA prescale_table[] = { 0, 6, 8, 10 };
TimerType ALIGN_DATA timer[4];

u32 dma_cycle_count = 0;

u32 cpu_ticks = 0;
s32 video_count = 272;

u32 irq_ticks = 0;
u8 cpu_init_state = 0;

u32 skip_next_frame = 0;
u32 frames = 0;

static u8 caches_inited = 0;

static void cpu_interrupt(void);

static void init_main(void);

#define SOUND_UPDATE_FREQUENCY_STEP                                           \
  FLOAT_TO_FP08_24((16777216.0 / SOUND_FREQUENCY) / timer_reload)             \

#define CHECK_COUNT(count_var)                                                \
  if (count_var < reg[EXECUTE_CYCLES])                                        \
  {                                                                           \
    reg[EXECUTE_CYCLES] = count_var;                                          \
  }                                                                           \

#define CHECK_TIMER(timer_number)                                             \
  if (timer[timer_number].status == TIMER_PRESCALE)                           \
  {                                                                           \
    CHECK_COUNT((u32)timer[timer_number].count);                              \
  }                                                                           \


void timer_control_low(u8 timer_number, u32 value)
{
  TimerType *tm = timer + timer_number;

  tm->reload = 0x10000 - value;
  tm->reload_update = 1;
}

CPU_ALERT_TYPE timer_control_high(u8 timer_number, u32 value)
{
  TimerType *tm = timer + timer_number;

  if ((value & 0x80) != 0)
  {
    if (tm->status == TIMER_INACTIVE)
    {
      if ((value & 0x04) != 0)
      {
        tm->status = TIMER_CASCADE;
        tm->prescale = 0;
      }
      else
      {
        tm->status = TIMER_PRESCALE;
        tm->prescale = prescale_table[value & 0x03];
      }

      tm->irq = (value >> 6) & 0x01;

      u32 timer_reload = tm->reload;

      pIO_REG(REG_TM0D + (timer_number << 1)) = 0x10000 - timer_reload;

      timer_reload <<= tm->prescale;
      tm->count = timer_reload;

      if (timer_number < 2)
      {
        tm->frequency_step = SOUND_UPDATE_FREQUENCY_STEP;
        tm->reload_update = 0;

        if ((tm->direct_sound_channels & 0x01) != 0)
          adjust_direct_sound_buffer(0, cpu_ticks + timer_reload);

        if ((tm->direct_sound_channels & 0x02) != 0)
          adjust_direct_sound_buffer(1, cpu_ticks + timer_reload);
      }

      return CPU_ALERT_TIMER;
    }
  }
  else
  {
    tm->status = TIMER_INACTIVE;
  }

  return CPU_ALERT_NONE;
}

void direct_sound_timer_select(u32 value)
{
  TIMER_DS_CHANNEL_TYPE timer_select;

  timer_select = ((value >> 13) & 0x02) | ((value >> 10) & 0x01);

  timer[0].direct_sound_channels = timer_select ^ 0x03;
  timer[1].direct_sound_channels = timer_select;
}


static void cpu_interrupt(void)
{
  // Interrupt handler in BIOS
  reg_mode[MODE_IRQ][6] = reg[REG_PC] + 4;
  spsr[MODE_IRQ] = reg[REG_CPSR];
  reg[REG_CPSR] = (reg[REG_CPSR] & ~0xFF) | 0x92; // set mode IRQ & disable IRQ
  reg[REG_PC] = 0x00000018;
  set_cpu_mode(MODE_IRQ);

  bios_read_protect = 0xe55ec002;

  reg[CPU_HALT_STATE] = CPU_ACTIVE;
  reg[CHANGED_PC_STATUS] = 1;
}

#define UPDATE_TIMER(timer_number)                                            \
{                                                                             \
  TimerType *tm = timer + timer_number;                                       \
                                                                              \
  if (tm->status != TIMER_INACTIVE)                                           \
  {                                                                           \
    if (tm->status != TIMER_CASCADE)                                          \
      tm->count -= reg[EXECUTE_CYCLES];                                       \
                                                                              \
    if (tm->count <= 0)                                                       \
    {                                                                         \
      if (tm->irq == TIMER_TRIGGER_IRQ)                                       \
      {                                                                       \
        irq_raised |= (IRQ_TIMER0 << timer_number);                           \
      }                                                                       \
                                                                              \
      if (timer_number != 3)                                                  \
      {                                                                       \
        if (tm[1].status == TIMER_CASCADE)                                    \
          tm[1].count--;                                                      \
      }                                                                       \
                                                                              \
      u32 timer_reload = tm->reload << tm->prescale;                          \
                                                                              \
      if (timer_number < 2)                                                   \
      {                                                                       \
        if ((tm->direct_sound_channels & 0x01) != 0)                          \
          sound_timer(tm->frequency_step, 0);                                 \
                                                                              \
        if ((tm->direct_sound_channels & 0x02) != 0)                          \
          sound_timer(tm->frequency_step, 1);                                 \
                                                                              \
        if (tm->reload_update != 0)                                           \
        {                                                                     \
          tm->frequency_step = SOUND_UPDATE_FREQUENCY_STEP;                   \
          tm->reload_update = 0;                                              \
        }                                                                     \
      }                                                                       \
                                                                              \
      tm->count += timer_reload;                                              \
    }                                                                         \
                                                                              \
    pIO_REG(REG_TM0D + (timer_number << 1)) = 0x10000 - (tm->count >> tm->prescale); \
  }                                                                           \
}                                                                             \

#define TRANSFER_HBLANK_DMA(channel)                                          \
  if (dma[channel].start_type == DMA_START_HBLANK)                            \
  {                                                                           \
    dma_transfer(dma + channel);                                              \
  }                                                                           \

#define TRANSFER_VBLANK_DMA(channel)                                          \
  if (dma[channel].start_type == DMA_START_VBLANK)                            \
  {                                                                           \
    dma_transfer(dma + channel);                                              \
  }                                                                           \

//void exit_cpu_loop(void);
void switch_to_main_thread(void);

u32 update_gba(void)
{
  s32 i;
  IRQ_TYPE irq_raised = IRQ_NONE;

  do
  {
    reg[CPU_DMA_HACK] = 0;

    update_gba_loop:

    cpu_ticks += reg[EXECUTE_CYCLES];

    if (gbc_sound_update != 0)
    {
      update_gbc_sound(cpu_ticks);
      gbc_sound_update = 0;
    }

    for (i = 0; i < 4; i++)
    {
      UPDATE_TIMER(i);
    }

    video_count -= reg[EXECUTE_CYCLES];

    if (video_count <= 0)
    {
      u16 dispstat = pIO_REG(REG_DISPSTAT);
      u16 vcount   = pIO_REG(REG_VCOUNT);

      if ((dispstat & 0x02) == 0)
      {
        // Transition from hrefresh to hblank
        video_count += 272;
        dispstat |= 0x02;

        if ((dispstat & 0x01) == 0)
        {
#ifdef HW_RENDER_TEST
           if(ge_render_enable)
              update_scanline_ge();
           else
#endif
           {
              if (!skip_next_frame)
                 update_scanline();
           }

          // If in visible area also fire HDMA
          for (i = 0; i < 4; i++)
          {
            TRANSFER_HBLANK_DMA(i);
          }
        }

        // H-blank interrupts do occur during v-blank (unlike hdma, which does not)
        if ((dispstat & 0x10) != 0)
        {
          irq_raised |= IRQ_HBLANK;
        }
      }
      else
      {
        // Transition from hblank to next line
        video_count += 960;
        dispstat &= ~0x02;

        vcount++;

        if (vcount == 160)
        {
          // Transition from vrefresh to vblank
          dispstat |= 0x01;

#ifdef HW_RENDER_TEST
          if(ge_render_enable)
            update_frame_ge();
#endif

//          update_input();

          switch_to_main_thread();

          update_gbc_sound(cpu_ticks);
          gbc_sound_update = 0;

          affine_reference_x[0] = (s32)(ADDRESS32(io_registers, 0x28) << 4) >> 4;
          affine_reference_y[0] = (s32)(ADDRESS32(io_registers, 0x2C) << 4) >> 4;
          affine_reference_x[1] = (s32)(ADDRESS32(io_registers, 0x38) << 4) >> 4;
          affine_reference_y[1] = (s32)(ADDRESS32(io_registers, 0x3C) << 4) >> 4;

          for (i = 0; i < 4; i++)
          {
            TRANSFER_VBLANK_DMA(i);
          }

          if ((dispstat & 0x08) != 0)
          {
            irq_raised |= IRQ_VBLANK;
          }
        }
        else

        if (vcount > 227)
        {
          // Transition from vblank to next screen
          dispstat &= ~0x01;

// TODO
// frame ready for display here


          vcount = 0;
        }

        if (vcount == (dispstat >> 8))
        {
          // vcount trigger
          dispstat |= 0x04;

          if ((dispstat & 0x20) != 0)
          {
            irq_raised |= IRQ_VCOUNT;
          }
        }
        else
        {
          dispstat &= ~0x04;
        }

        pIO_REG(REG_VCOUNT) = vcount;
      }

      pIO_REG(REG_DISPSTAT) = dispstat;
    }

    reg[EXECUTE_CYCLES] = video_count;

    for (i = 0; i < 4; i++)
    {
      CHECK_TIMER(i);
    }

    if (dma_cycle_count != 0)
    {
      CHECK_COUNT(dma_cycle_count);
      dma_cycle_count -= reg[EXECUTE_CYCLES];

      goto update_gba_loop;
    }

    if (irq_raised != IRQ_NONE)
    {
      pIO_REG(REG_IF) |= irq_raised;
      irq_raised = IRQ_NONE;
    }

    if ((pIO_REG(REG_IF) != 0) && GBA_IME_STATE && ARM_IRQ_STATE)
    {
      u16 irq_mask = (reg[CPU_HALT_STATE] == CPU_STOP) ? 0x3080 : 0x3FFF;

      if ((pIO_REG(REG_IE) & pIO_REG(REG_IF) & irq_mask) != 0)
      {
        if (cpu_init_state != 0)
        {
          if (irq_ticks == 0)
          {
            cpu_interrupt();
            cpu_init_state = 0;
          }
        }
        else
        {
          if (reg[CPU_HALT_STATE] != CPU_ACTIVE)
          {
            cpu_interrupt();
          }
          else
          {
            // IRQ delay - Tsyncmax=3, Texc=3, Tirq=2, Tldm=20
            //             Tsyncmin=2
            irq_ticks = 9;
            cpu_init_state = 1;
          }
        }
      }
    }

    if (irq_ticks != 0)
    {
      CHECK_COUNT(irq_ticks);
      irq_ticks -= reg[EXECUTE_CYCLES];
    }
  }
  while (reg[CPU_HALT_STATE] != CPU_ACTIVE);

  return reg[EXECUTE_CYCLES];
}


static void init_main(void)
{
  s32 i;

  init_cpu();

  for (i = 0; i < 4; i++)
  {
    memset(&dma[i], 0, sizeof(DmaTransferType));

    dma[i].start_type = DMA_INACTIVE;
    dma[i].direct_sound_channel = DMA_NO_DIRECT_SOUND;

    memset(&timer[i], 0, sizeof(TimerType));

    timer[i].status = TIMER_INACTIVE;
    timer[i].reload = 0x10000;
    timer[i].direct_sound_channels = TIMER_DS_CHANNEL_NONE;
  }

  cpu_ticks = 0;

  video_count = option_boot_mode ? 960 : 272;
  reg[EXECUTE_CYCLES] = video_count;

  irq_ticks = 0;
  cpu_init_state = 0;

  if (!caches_inited)
  {
    flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_INITIALIZING);
    flush_translation_cache(TRANSLATION_REGION_WRITABLE, FLUSH_REASON_INITIALIZING);
  }
  else
  {
    flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_LOADING_ROM);
    clear_metadata_area(METADATA_AREA_EWRAM, CLEAR_REASON_LOADING_ROM);
    clear_metadata_area(METADATA_AREA_IWRAM, CLEAR_REASON_LOADING_ROM);
    clear_metadata_area(METADATA_AREA_VRAM,  CLEAR_REASON_LOADING_ROM);
  }

  caches_inited = 1;
}




void quit_gba(void)
{
  memory_term();

  sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
  sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
}

void reset_gba(void)
{
  clear_texture(0x7FFF);
  init_memory();
  init_main();
  reset_sound();
}


u32 file_length(const char *filename)
{
  SceIoStat stats;

  sceIoGetstat(filename, &stats);

  return stats.st_size;
}

void change_ext(char *src, char *buffer, const char *extension)
{
  char *dot_position;

  strcpy(buffer, src);
  dot_position = strrchr(buffer, '.');

  if (dot_position)
  {
    strcpy(dot_position, extension);
  }
}

SceUID psp_fopen(const char *filename, const char *mode)
{
  SceUID tag;

  if ((*mode != 'r') && (*mode != 'w'))
    return -1;

  tag = sceIoOpen(filename, PSP_O_RDONLY, 0777);

  if (*mode == 'w')
  {
    int flags;

    if (tag < 0)
    {
      flags = PSP_O_WRONLY | PSP_O_CREAT;
    }
    else
    {
      sceIoClose(tag);
      flags = PSP_O_WRONLY | PSP_O_TRUNC;
    }

    tag = sceIoOpen(filename, flags, 0777);
  }

  return tag;
}

void psp_fclose(SceUID filename_tag)
{
  if (filename_tag < 0)
    return;

  sceIoClose(filename_tag);
  filename_tag = -1;
}


void *safe_malloc(size_t size)
{
  void *p;

  if ((p = memalign(MEM_ALIGN, size)) == NULL)
  {
    error_msg("Could not allocate memory.");
    quit_gba();
  }

  return p;
}


// type = READ / WRITE_MEM
#define MAIN_SAVESTATE_BODY(type)                                    \
{                                                                    \
  MEM_##type##_VARIABLE(cpu_ticks);                          \
  MEM_##type##_VARIABLE(video_count);                        \
  MEM_##type##_VARIABLE(irq_ticks);                          \
  MEM_##type##_VARIABLE(cpu_init_state);                     \
  MEM_##type##_ARRAY(timer);                                 \
}

void main_read_savestate(void)
{
  MAIN_SAVESTATE_BODY(READ);
}

void main_write_savestate(void)
{
  MAIN_SAVESTATE_BODY(WRITE);
}
