
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pspthreadman.h>
#include "common.h"

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_environment_t environ_cb;

struct retro_perf_callback perf_cb;

#include "pspthreadman.h"
static SceUID main_thread;
static SceUID cpu_thread = -1;

/* Disable frame skipping by default since most games don't seem to need it */
static u32 option_frameskip_type = FRAMESKIP_NONE;
/* Use a low default for less choppy video when frame skipping is enabled */
static u32 option_frameskip_value = 1;
static u32 num_skipped_frames = 0;
/* Count of the actual number of frames drawn */
static u32 real_frame_count = 0;
static u32 virtual_frame_count = 0;

void switch_to_main_thread(void)
{
   sceKernelWakeupThread(main_thread);
   sceKernelSleepThread();
}

static inline void switch_to_cpu_thread(void)
{
   sceKernelWakeupThread(cpu_thread);
   sceKernelSleepThread();
}

static int cpu_thread_entry(SceSize args, void* argp)
{
   sceKernelSleepThread();

   execute_arm_translate(reg[EXECUTE_CYCLES]);

   return 0;
}

static inline bool init_context_switch(void)
{
   main_thread = sceKernelGetThreadId();
   cpu_thread = sceKernelCreateThread ("CPU thread", cpu_thread_entry, 0x12, 0x20000, 0, NULL);
   if (cpu_thread < 0)
      return false;
   if (sceKernelStartThread(cpu_thread, 0, NULL) < 0)
   {
      sceKernelDeleteThread(cpu_thread);
      cpu_thread = -1;
      return false;
   }
   return true;
}

static inline void deinit_context_switch(void)
{
   if (cpu_thread >= 0)
   {
      sceKernelTerminateDeleteThread(cpu_thread);
      cpu_thread = -1;
   }
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "FrogGBA";
   info->library_version = "0.4.0-rc.1-retroshell";
   /* Just pass the path of the ROM to the core and let it handle reading the ROM */
   info->need_fullpath = true;
   /* Tell the frontend (RetroArch) not to extract the ROM; the core will handle extraction */
   info->block_extract = true;
   info->valid_extensions = "gba|bin|agb|gbz|zip";
}


void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width = GBA_SCREEN_WIDTH;
   info->geometry.base_height = GBA_SCREEN_HEIGHT;
   info->geometry.max_width = GBA_SCREEN_WIDTH;
   info->geometry.max_height = GBA_SCREEN_HEIGHT;
   info->geometry.aspect_ratio = 0;
   info->timing.fps = ((float) CPU_FREQUENCY) / (308 * 228 * 4); // 59.72750057 hz
   info->timing.sample_rate = SOUND_FREQUENCY;
//   info->timing.sample_rate = 32 * 1024;
}


void retro_init(void)
{
   init_gamepak_buffer();
   init_sound();
#ifdef HW_RENDER_TEST
   init_video_ge();
#endif
}

void retro_deinit(void)
{
   if (perf_cb.perf_log)
      perf_cb.perf_log();
   quit_gba();
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback log;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   memset(&perf_cb, 0, sizeof(perf_cb));
   environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);

   struct retro_variable vars[] = {
      { "froggba_frameskip_type", "Frameskip type; off|auto|manual" },
      { "froggba_frameskip_value", "Frameskip value; 0|1|2|3|4|5|6|7|8|9" },
      { 0, 0 }
   };

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, vars);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

void retro_set_controller_port_device(unsigned port, unsigned device) {}

void retro_reset(void)
{
   deinit_context_switch();

   reset_gba();

   if (!init_context_switch())
      error_msg("Could not create FrogGBA CPU thread.\n");
}


size_t retro_serialize_size(void)
{
   return SAVESTATE_SIZE;
}

bool retro_serialize(void *data, size_t size)
{
   if (size < SAVESTATE_SIZE)
      return false;

   gba_save_state(data);

   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   if (size < SAVESTATE_SIZE)
      return false;

   gba_load_state(data);

   return true;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

bool string_endswith_newline(const char *string)
{
  if (!string || !string[0]) return false;
  return string[strlen(string) - 1] == '\n';
}

void error_msg(const char *text)
{
   if (log_cb)
   {
      if (string_endswith_newline(text))
         log_cb(RETRO_LOG_ERROR, text);
      else
         log_cb(RETRO_LOG_ERROR, "%s\n", text);
   }
}

void info_msg(const char *text)
{
   if (log_cb)
   {
      if (string_endswith_newline(text))
         log_cb(RETRO_LOG_INFO, text);
      else
         log_cb(RETRO_LOG_INFO, "%s\n", text);
   }
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');

   if (base)
      *base = '\0';
   else
      strncpy(buf, ".", size);
}

static void check_variables(void)
{
   struct retro_variable var = {
      .key = "froggba_frameskip_value",
      .value = 0
   };
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      option_frameskip_value = strtol(var.value, NULL, 10);

   var.key = "froggba_frameskip_type";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "off"))
         option_frameskip_type = FRAMESKIP_NONE;
      else if (!strcmp(var.value, "auto"))
         option_frameskip_type = FRAMESKIP_AUTO;
      else if (!strcmp(var.value, "manual"))
         option_frameskip_type = FRAMESKIP_MANUAL;
   }
}

bool retro_load_game(const struct retro_game_info *info)
{
   char filename_bios[MAX_PATH];
   const char *dir = NULL;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

      { 0 },
   };

   if (!info)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   /* The software renderer produces GBA BGR555 pixels (the original PSP
    * integration sampled them as GU_PSM_5551), not RGB565. */
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_0RGB1555;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[FrogGBA]: 0RGB1555 is not supported.\n");
      return false;
   }

   extract_directory(main_path,info->path,sizeof(main_path));

   const char *system_dir =
      (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
        ? dir : main_path;
   if (strlen(system_dir) + sizeof("/gba_bios.bin") > sizeof(filename_bios))
      return false;
   snprintf(filename_bios, sizeof(filename_bios), "%s/gba_bios.bin", system_dir);


   const char *save_dir =
      (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
        ? dir : main_path;
   if (strlen(save_dir) + 2 > sizeof(dir_save))
      return false;
   snprintf(dir_save, sizeof(dir_save), "%s/", save_dir);

   { size_t n = strlen(main_path);
     if (n + 1 < sizeof(main_path)) { main_path[n] = '/'; main_path[n + 1] = 0; }
   }

   if (load_bios(filename_bios) < 0)
   {
     error_msg("Could not load BIOS image file.\n");
     return false;
   }

   gamepak_filename[0] = 0;

   if (load_gamepak(info->path) < 0)
   {
     error_msg("Could not load the game file.\n");
     return false;
   }

   reset_gba();

   if (!init_context_switch())
   {
     error_msg("Could not create FrogGBA CPU thread.\n");
     return false;
   }

   check_variables();

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   return false;
}

void retro_unload_game(void)
{
   deinit_context_switch();
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
   return id == RETRO_MEMORY_SAVE_RAM ? gamepak_backup : 0;
}

size_t retro_get_memory_size(unsigned id)
{
   return id == RETRO_MEMORY_SAVE_RAM ? sizeof(gamepak_backup) : 0;
}

#include<psprtc.h>

void retro_run(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();

   skip_next_frame = 0;
   virtual_frame_count++;

   /* RetroShell runs bounded logic/audio recovery frames when the PSP audio
    * queue is close to starvation. Honour the standard video-disable bit so
    * these frames skip FrogGBA's expensive scanline renderer instead of doing
    * all rendering work and merely withholding the finished callback. */
   {
      int audio_video_enable = 3;
      if (environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE,
                     &audio_video_enable) && !(audio_video_enable & 1))
         skip_next_frame = 1;
   }

   if (!skip_next_frame && option_frameskip_type == FRAMESKIP_MANUAL)
   {
      if (num_skipped_frames < option_frameskip_value)
      {
         skip_next_frame = 1;
         num_skipped_frames++;
      } else {
         num_skipped_frames = 0;
      }
   }
   /* RetroShell owns audio-directed recovery. The upstream auto mode used a
    * global PSP VBlank handler, which is unsafe inside a loadable core. */

   input_poll_cb();

#ifdef HW_RENDER_TEST
   uint64_t start_tick, end_tick;
   sceRtcGetCurrentTick(&start_tick);
#endif

   switch_to_cpu_thread();

   update_input();

#ifdef HW_RENDER_TEST
   sceRtcGetCurrentTick(&end_tick);
// printf("frame time : %u\n", (uint32_t)(end_tick - start_tick));
   static int frames = 0;
   static float total = 0.0;

   if ( frames >= 200)
      total += (end_tick - start_tick);

   if (frames++ == 400)
      printf("total : %f\n", total / 200.0);
#endif

   render_audio();


   /* RetroShell owns PSP VRAM and GU state. Send the core's aligned RAM
    * framebuffer through the regular callback to avoid corrupting frontend
    * textures when the core unloads. */
   if (!skip_next_frame)
      video_cb(froggba_framebuffer, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT,
               GBA_LINE_SIZE * sizeof(u16));
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}
