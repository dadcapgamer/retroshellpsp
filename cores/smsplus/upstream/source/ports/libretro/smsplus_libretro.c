#include <libretro.h>
#include <libretro_core_options.h>
#include <streams/memory_stream.h>

#include "shared.h"
#include "smsplus.h"
#include "sound_output.h"

#ifdef HAVE_NTSC
#include "ntsc/sms_ntsc.h"
#endif

t_config option                                    = { 0 };
static gamedata_t gdata                            = { 0 };
static uint16_t *sms_bitmap                        = NULL;

static retro_log_printf_t log_cb                   = NULL;
static retro_video_refresh_t video_cb              = NULL;
static retro_audio_sample_t audio_cb               = NULL;
static retro_environment_t environ_cb              = NULL;
static retro_input_poll_t input_poll_cb            = NULL;
static retro_input_state_t input_state_cb          = NULL;
static retro_audio_sample_batch_t audio_batch_cb   = NULL;

static unsigned libretro_supports_bitmasks         = 0;
static unsigned libretro_serialize_size            = 0;
static unsigned geometry_changed                   = 0;
static unsigned hide_left_border                   = 0;

static unsigned system_width                       = VIDEO_WIDTH_SMS;
static unsigned system_height                      = VIDEO_HEIGHT_SMS;

#define RETRO_DEVICE_MSPAD_2B                      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_PADDLE                        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)

static unsigned input_type[2]                      = { DEVICE_PAD2B, DEVICE_PAD2B };

#ifdef HAVE_NTSC
static SMS_NTSC_IN_T *ntsc_screen                  = NULL;
static sms_ntsc_t *sms_ntsc                        = NULL;
static unsigned use_ntsc                           = 0;

enum {
   NTSC_NONE = 0,
   NTSC_MONOCHROME,
   NTSC_COMPOSITE,
   NTSC_SVIDEO,
   NTSC_RGB,
};
#endif /* HAVE_NTSC */

#ifdef _WIN32
#define path_default_slash_c() '\\'
#else
#define path_default_slash_c() '/'
#endif

#define MAX_PORTS 2

extern uint32_t load_rom_mem(const char *, size_t);

#ifndef MAXIM_PSG
extern sn76489_t psg_sn;
#endif

#define state_write(in_data, size) memstream_write(stream, in_data, size)
#define state_read(in_data, size)  memstream_read(stream, in_data, size)

static uint32_t system_save_state_mem(void *data, size_t size)
{
   uint32_t written;
   memstream_t *stream = memstream_open((uint8_t*)data, (uint64_t)size, 1);

   if (!stream)
      return 0;

   /* Save VDP context */
   state_write(&vdp, sizeof(vdp_t));

   /* Save SMS context */
   state_write(&sms, sizeof(sms_t));

   state_write(cart.fcr, 4);

   state_write(cart.sram, 0x8000);

   /* Save Z80 context */
   state_write(Z80_Context, sizeof(z80_t));

   /* Save YM2413 context */
   state_write(YM2413_GetContextPtr(), YM2413_GetContextSize());

   state_write(FM_GetContextPtr(), FM_GetContextSize());

   /* Save SN76489 context */
   #ifdef MAXIM_PSG
   state_write(SN76489_GetContextPtr(0), SN76489_GetContextSize());
   #else
   state_write(&psg_sn, sizeof(sn76489_t));
   #endif

   written = (uint32_t)memstream_pos(stream);
   memstream_close(stream);

   return written;
}

static void system_load_state_mem(const void *data, size_t size)
{
   unsigned i           = 0;
   uint8_t *buf         = NULL;
   memstream_t *stream  = memstream_open((uint8_t*)data, (uint64_t)size, 0);

   if (!stream)
      return;

   /* Initialize everything */
   system_reset();

   /* Load VDP context */
   state_read(&vdp, sizeof(vdp_t));

   /* Load SMS context */
   state_read(&sms, sizeof(sms_t));

   /* restore video & audio settings (needed if timing changed) */
   vdp_init();
   SMSPLUS_sound_init();

   state_read(cart.fcr, 4);

   state_read(cart.sram, 0x8000);

   /* Load Z80 context */
   state_read(Z80_Context, sizeof(z80_t));

   #ifdef USE_Z80
   Z80.irq_callback = sms_irq_callback;
   #endif

   /* Load YM2413 context */
   state_read(YM2413_GetContextPtr(), YM2413_GetContextSize());

   buf = malloc(FM_GetContextSize());
   state_read(buf, FM_GetContextSize());
   FM_SetContext(buf);
   free(buf);

   /* Load SN76489 context */
   #ifdef MAXIM_PSG
   buf = malloc(SN76489_GetContextSize());
   state_read(buf, SN76489_GetContextSize());
   SN76489_SetContext(0, buf);
   free(buf);
   #else
   buf = malloc(sizeof(sn76489_t));
   state_read(buf, sizeof(sn76489_t));
   memcpy(&psg_sn, buf, sizeof(sn76489_t));
   free(buf);
   #endif

   memstream_close(stream);

   if ((sms.console != CONSOLE_COLECO) && (sms.console != CONSOLE_SG1000) && (sms.console != CONSOLE_SORDM5))
   {
      /* Cartridge by default */
      slot.rom    = cart.rom;
      slot.pages  = cart.pages;
      slot.mapper = cart.mapper;
      slot.fcr    = &cart.fcr[0];

      /* Restore mapping */
      mapper_reset();
      cpu_readmap[0] = &slot.rom[0];
      if (slot.mapper != MAPPER_KOREA_MSX)
      {
         mapper_16k_w(0, slot.fcr[0]);
         mapper_16k_w(1, slot.fcr[1]);
         mapper_16k_w(2, slot.fcr[2]);
         mapper_16k_w(3, slot.fcr[3]);
      }
      else
      {
         mapper_8k_w(0, slot.fcr[0]);
         mapper_8k_w(1, slot.fcr[1]);
         mapper_8k_w(2, slot.fcr[2]);
         mapper_8k_w(3, slot.fcr[3]);
      }
   }

   /* Force full pattern cache update */
   bg_list_index = 0x200;
   for (i = 0; i < 0x200; i++)
   {
      bg_name_list[i]  = i;
      bg_name_dirty[i] = 255;
   }

   /* Restore palette */
   for (i = 0; i < PALETTE_SIZE; i++)
      palette_sync(i);
}

void system_manage_sram(uint8_t *sram, uint8_t slot_number, uint8_t mode)
{
   (void)sram;
   (void)slot_number;
   (void)mode;
}

static int bios_init(void)
{
   FILE *fd;
   char bios_path[1024];

   bios.rom     = malloc(0x100000);
   bios.enabled = 0;

   sprintf(bios_path, "%s%c%s", gdata.biosdir, path_default_slash_c(), "bios.sms");

   fd = fopen(bios_path, "rb");

   if(fd)
   {
      uint32_t size;
      /* Seek to end of file, and get size */
      fseek(fd, 0, SEEK_END);
      size = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      if (size < 0x4000)
         size = 0x4000;
      fread(bios.rom, size, 1, fd);
      bios.enabled = 2;
      bios.pages   = size / 0x4000;
      fclose(fd);

      log_cb(RETRO_LOG_INFO, "bios loaded:      %s\n", bios_path);
   }

   if (sms.console == CONSOLE_COLECO)
   {
      sprintf(bios_path, "%s%c%s", gdata.biosdir, path_default_slash_c(), "BIOS.col");
      fd = fopen(bios_path, "rb");

      if(!fd)
      {
         /* Coleco bios is required when running coleco roms */
         log_cb(RETRO_LOG_ERROR, "Cannot load required colero rom: %s\n", bios_path);
         return 0;
      }

      /* Seek to end of file, and get size */
      fread(coleco.rom, 0x2000, 1, fd);
      fclose(fd);

      log_cb(RETRO_LOG_INFO, "bios loaded:      %s\n", bios_path);
   }

   return 1;
}

/* Libretro implementation */

static void update_input(void)
{
#define JOYP(n)   (ret & (1 << (n)))
#define KEYP(key) (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key))

   unsigned port;
   unsigned startpressed = 0;

   input_poll_cb();

   input.pad[0]     = 0;
   input.pad[1]     = 0;

   coleco.keypad[0] = ~0;
   coleco.keypad[1] = ~0;

   input.system    &= (sms.console == CONSOLE_GG) ? ~INPUT_START : ~INPUT_PAUSE;

   for (port = 0; port < MAX_PORTS; port++)
   {
      int32_t ret = 0;
      if (libretro_supports_bitmasks)
      {
         ret = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      }
      else
      {
         unsigned i;
         for (i = 0; i < (1 + RETRO_DEVICE_ID_JOYPAD_R3); i++)
            ret |= (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0);
      }

      if (JOYP(RETRO_DEVICE_ID_JOYPAD_UP))
         input.pad[port] |= INPUT_UP;
      if (JOYP(RETRO_DEVICE_ID_JOYPAD_DOWN))
         input.pad[port] |= INPUT_DOWN;
      if (JOYP(RETRO_DEVICE_ID_JOYPAD_LEFT))
         input.pad[port] |= INPUT_LEFT;
      if (JOYP(RETRO_DEVICE_ID_JOYPAD_RIGHT))
         input.pad[port] |= INPUT_RIGHT;
      if (JOYP(RETRO_DEVICE_ID_JOYPAD_B))
         input.pad[port] |= INPUT_BUTTON1;
      if (JOYP(RETRO_DEVICE_ID_JOYPAD_A))
         input.pad[port] |= INPUT_BUTTON2;

      if (sms.console == CONSOLE_COLECO)
      {
         if (KEYP(RETROK_1) || (JOYP(RETRO_DEVICE_ID_JOYPAD_X)))
            coleco.keypad[port] = 1;
         else if (KEYP(RETROK_2) || (JOYP(RETRO_DEVICE_ID_JOYPAD_Y)))
            coleco.keypad[port] = 2;
         else if (KEYP(RETROK_3) || (JOYP(RETRO_DEVICE_ID_JOYPAD_R)))
            coleco.keypad[port] = 3;
         else if (KEYP(RETROK_4) || (JOYP(RETRO_DEVICE_ID_JOYPAD_L)))
            coleco.keypad[port] = 4;
         else if (KEYP(RETROK_5) || (JOYP(RETRO_DEVICE_ID_JOYPAD_R2)))
            coleco.keypad[port] = 5;
         else if (KEYP(RETROK_6) || (JOYP(RETRO_DEVICE_ID_JOYPAD_L2)))
            coleco.keypad[port] = 6;
         else if (KEYP(RETROK_7) || (JOYP(RETRO_DEVICE_ID_JOYPAD_R3)))
            coleco.keypad[port] = 7;
         else if (KEYP(RETROK_8) || (JOYP(RETRO_DEVICE_ID_JOYPAD_L3)))
            coleco.keypad[port] = 8;
         else if (KEYP(RETROK_9))
            coleco.keypad[port] = 9;
         else if (KEYP(RETROK_DOLLAR) || (JOYP(RETRO_DEVICE_ID_JOYPAD_START)))
            coleco.keypad[port] = 10;
         else if (KEYP(RETROK_ASTERISK) || (JOYP(RETRO_DEVICE_ID_JOYPAD_SELECT)))
            coleco.keypad[port] = 11;
      }
      else if (input_type[port] == DEVICE_PADDLE)
      {
         input.analog[port][0] = (input_state_cb(port, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X) + 0x8000) >> 8;

         if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            input.pad[port] |= INPUT_BUTTON1;
         if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            input.pad[port] |= INPUT_START;
      }

      if (!port && (JOYP(RETRO_DEVICE_ID_JOYPAD_START)))
         startpressed = 1;
   }

   if (startpressed)
      input.system |= (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;

   if (sms.console == CONSOLE_COLECO) input.system = 0;
}

static void check_system_specs(void)
{
   unsigned level = 0;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_init(void)
{
   struct retro_log_callback log;
   bool achievements = true;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &achievements);

   libretro_supports_bitmasks = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = 1;

   check_system_specs();
}

void retro_reset(void)
{
   system_reset();
}

bool retro_load_game_special(unsigned id, const struct retro_game_info *info, size_t size)
{
   return false;
}

static void check_variables(bool startup)
{
   struct retro_variable var = { 0 };
   unsigned old_border = hide_left_border;
   #ifdef HAVE_NTSC
   unsigned old_ntsc   = use_ntsc;
   #endif

   var.value = NULL;
   var.key   = "smsplus_sms_bios";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
   {
      if (strcmp(var.value, "auto") == 0)
         bios.enabled |= 1;
   }
   
   var.value = NULL;
   var.key   = "smsplus_hardware";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
   {
      if (strcmp(var.value, "master system") == 0)
         sms.console = CONSOLE_SMS;
      else if (strcmp(var.value, "master system II") == 0)
         sms.console = CONSOLE_SMS2;
      else if (strcmp(var.value, "game gear") == 0)
         sms.console = CONSOLE_GG;
      else if (strcmp(var.value, "game gear (sms compatibility)") == 0)
         sms.console = CONSOLE_GGMS;
      else if (strcmp(var.value, "coleco") == 0)
      {
         sms.console = CONSOLE_COLECO;
			cart.mapper = MAPPER_NONE;
      }
   }

   var.value = NULL;
   var.key   = "smsplus_region";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
   {
      if (strcmp(var.value, "ntsc-u") == 0)
      {
         sms.display   = DISPLAY_NTSC;
			sms.territory = TERRITORY_EXPORT;
      }
      else if (strcmp(var.value, "pal") == 0)
      {
         sms.display   = DISPLAY_PAL;
			sms.territory = TERRITORY_EXPORT;
      }
      else if (strcmp(var.value, "ntsc-j") == 0)
      {
         sms.display   = DISPLAY_NTSC;
			sms.territory = TERRITORY_DOMESTIC;
      }
   }

   var.value = NULL;
   var.key   = "smsplus_fm_sound";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
   {
      if (strcmp(var.value, "disabled") == 0)
         option.fm = 0;
      else
         option.fm = sms.use_fm;
   }

   var.value = NULL;
   var.key   = "smsplus_hide_left_border";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
         hide_left_border = 1;
      else
         hide_left_border = 0;
   }

   #ifdef HAVE_NTSC
   var.value = NULL;
   var.key   = "smsplus_ntsc_filter";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "monochrome") == 0)
         use_ntsc = NTSC_MONOCHROME;
      else if (strcmp(var.value, "composite") == 0)
         use_ntsc = NTSC_COMPOSITE;
      else if (strcmp(var.value, "svideo") == 0)
         use_ntsc = NTSC_SVIDEO;
      else if (strcmp(var.value, "rgb") == 0)
         use_ntsc = NTSC_RGB;
      else
         use_ntsc = NTSC_NONE;
   }

   if (old_ntsc != use_ntsc)
   {
      sms_ntsc_setup_t setup = { 0 };

      switch (use_ntsc)
      {
      case NTSC_MONOCHROME:
         setup = sms_ntsc_monochrome;
         break;
      case NTSC_COMPOSITE:
         setup = sms_ntsc_composite;
         break;
      case NTSC_SVIDEO:
         setup = sms_ntsc_svideo;
         break;
      case NTSC_RGB:
         setup = sms_ntsc_rgb;
         break;
      default:
         break;
      }

      if (use_ntsc)
         sms_ntsc_init(sms_ntsc, &setup);
      geometry_changed = 1;
   }
   #endif /* HAVE_NTSC */

   if (old_border != hide_left_border)
      bitmap.viewport.changed = 1;
}

static void retro_set_memory_map(void)
{
   struct retro_memory_map mmaps;
   struct retro_memory_descriptor descs[64];
   size_t i = 0, j = 0;

   memset(descs, 0, sizeof(descs));
   for (i = 0; i < 64; i++)
   {
      if (MMapPtrs[i] != NULL)
      {
         descs[j].ptr    = MMapPtrs[i];
         descs[j].start  = i * 1024;
         descs[j].len    = 1024;
         descs[j].select = 0;
         j++;
      }
   }

   mmaps.descriptors = descs;
   mmaps.num_descriptors = j;
   environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmaps);
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format rgb565   = RETRO_PIXEL_FORMAT_RGB565;
   const char *dir                  = NULL;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Button 1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Button 2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Coleco 2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Coleco 1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start/Pause / Coleco #" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,"Coleco *" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Coleco 4" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Coleco 3" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "coleco 6" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "coleco 5" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,    "coleco 8" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,    "coleco 7" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Button 1" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Button 2" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Coleco 2" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Coleco 1" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Coleco #" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,"Coleco *" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Coleco 4" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Coleco 3" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "coleco 6" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "coleco 5" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,    "coleco 8" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,    "coleco 7" },

      { 0, 0, 0, 0, NULL },
   };

   if (!info)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
      log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");

   /* Set up the bios directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      sprintf(gdata.biosdir, "%s", dir);

   memset(&option, 0, sizeof(option));

   option.fullscreen  = 1;
   option.fm          = 1;
   option.spritelimit = 1;
   option.tms_pal     = 2;
   option.nosound     = 0;
   option.soundlevel  = 2;
   option.sndrate     = SOUND_FREQUENCY;

   option.country     = 0;
   option.console     = 0;

   /* Force Colecovision mode if extension is .col */
	if (strcmp(strrchr(info->path, '.'), ".col") == 0)
      option.console = 6;
   /* Force SG-1000 */
   if (strcmp(strrchr(info->path, '.'), ".sg") == 0)
      option.console = 5;

   /* Load ROM */
   load_rom_mem((const char*)info->data, info->size);

   log_cb(RETRO_LOG_INFO, "CRC : 0x%08X\n", cart.crc);

   #ifdef HAVE_NTSC
   ntsc_screen = (SMS_NTSC_IN_T*)calloc(1, SMS_NTSC_OUT_WIDTH(VIDEO_WIDTH_SMS) * 240 * sizeof(SMS_NTSC_IN_T));
   sms_ntsc    = (sms_ntsc_t*)calloc(1, sizeof(sms_ntsc_t));
   #endif

   sms_bitmap  = (uint16_t*)malloc(VIDEO_WIDTH_SMS * 240 * sizeof(uint16_t));

   /* Set parameters for internal bitmap */
   bitmap.width       = VIDEO_WIDTH_SMS;
   bitmap.height      = 240;
   bitmap.depth       = 16;
   bitmap.data        = (uint8_t *)sms_bitmap;
   bitmap.pitch       = VIDEO_WIDTH_SMS * sizeof(uint16_t);
   bitmap.viewport.w  = VIDEO_WIDTH_SMS;
   bitmap.viewport.h  = VIDEO_HEIGHT_SMS;
   bitmap.viewport.x  = 0x00;
   bitmap.viewport.y  = 0x00;

   if (!bios_init()) /* This only fails when running coleco roms and coleco bios is not found */
      return false;

   check_variables(true);

   /* sms.territory = settings.misc_region; */
   if (sms.console == CONSOLE_SMS || sms.console == CONSOLE_SMS2)
      sms.use_fm = option.fm;

   /* Initialize all systems and power on */
   system_poweron();

   system_width  = bitmap.viewport.w;
   system_height = bitmap.viewport.h;

   libretro_serialize_size = 0;

   retro_set_memory_map();

   return true;
}

void retro_unload_game(void)
{
}

void retro_run(void)
{
   bool updated    = false;
   bool skip       = false;
   int32_t xoffset = 0;
   static unsigned last_width, last_height;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables(false);

   /* Read input */
   update_input();

   /* Execute frame(s) */
   system_frame(skip);

   /* Refresh video data */
   xoffset       = bitmap.viewport.x;
   system_width  = bitmap.viewport.w;
   system_height = bitmap.viewport.h;

   /* hide left border (sms) */
   if (hide_left_border && (IS_SMS || IS_MD))
   {
      if (vdp.reg[0] & 0x20)
      {
         xoffset      = 8;
         system_width = VIDEO_WIDTH_SMS - 8;
      }
   }

   if (system_width != last_width || system_height != last_height)
   {
      bitmap.viewport.changed = 1;
      last_width              = system_width;
      last_height             = system_height;
   }

   if (geometry_changed || bitmap.viewport.changed)
   {
      struct retro_system_av_info info = { 0 };

      retro_get_system_av_info(&info);

      /* hard audio-video reset */
      if (geometry_changed)
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
      else
         environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info);

      geometry_changed        = 0;
      bitmap.viewport.changed = 0;
   }

   #ifdef HAVE_NTSC
   if (use_ntsc)
   {
      int32_t ntsc_out_width  = SMS_NTSC_OUT_WIDTH(system_width);
      uint32_t ntsc_out_pitch = ntsc_out_width << 1;

      sms_ntsc_blit(sms_ntsc, sms_bitmap + xoffset, bitmap.pitch / sizeof(uint16_t),
            system_width, system_height, ntsc_screen, ntsc_out_pitch);

      video_cb(ntsc_screen, ntsc_out_width, system_height, ntsc_out_pitch);
   }
   else
   #endif
   {
      video_cb((const uint16_t *)sms_bitmap + xoffset, system_width, system_height, bitmap.pitch);
   }

   /* Output audio */
   audio_batch_cb((const int16_t*)snd.output, (size_t)snd.sample_count);
}

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

   memset(info, 0, sizeof(*info));

   info->library_name      = APP_NAME;
   info->library_version   = APP_VERSION GIT_VERSION;
   info->need_fullpath     = false;
   info->valid_extensions  = "sms|bin|rom|col|gg|sg";
   info->block_extract     = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
#define SMS_SCALED_4_3      (((double)system_width / (240.0 * ((double)VIDEO_WIDTH_SMS / 240.0))) * 4.0 / 3.0)

   memset(info, 0, sizeof(*info));

#ifdef HAVE_NTSC
   info->geometry.base_width   = !use_ntsc ? system_width : SMS_NTSC_OUT_WIDTH (system_width);
   info->geometry.max_width    = SMS_NTSC_OUT_WIDTH(VIDEO_WIDTH_SMS);
#else
   info->geometry.base_width   = system_width;
   info->geometry.max_width    = VIDEO_WIDTH_SMS;
#endif

   info->geometry.base_height  = system_height;
   info->geometry.max_height   = 240;

   if (sms.console != CONSOLE_GG)
      info->geometry.aspect_ratio = SMS_SCALED_4_3;
   else
      info->geometry.aspect_ratio = 4.0 / 3.0;

   info->timing.fps            = (double)((sms.display == DISPLAY_PAL) ? FPS_PAL : FPS_NTSC);
   info->timing.sample_rate    = (double)option.sndrate;
}

void retro_deinit()
{
   if (sms_bitmap)
      free(sms_bitmap);
   sms_bitmap = NULL;

   if (bios.rom)
      free(bios.rom);
   bios.rom = NULL;

   /* Shut down */
   system_poweroff();
   system_shutdown();

#ifdef HAVE_NTSC
   if (ntsc_screen)
      free(ntsc_screen);
   ntsc_screen = NULL;

   if (sms_ntsc)
      free(sms_ntsc);
   sms_ntsc = NULL;
#endif

   libretro_serialize_size    = 0;
   libretro_supports_bitmasks = 0;
}

unsigned retro_get_region(void)
{
   return (sms.display == DISPLAY_PAL) ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
   if (in_port > 1)
      return;
   
   switch (device)
   {
   case RETRO_DEVICE_NONE:
      input_type[in_port] = DEVICE_NONE;
      break;
   case RETRO_DEVICE_MSPAD_2B:
      sms.device[in_port] = DEVICE_PAD2B;
      input_type[in_port] = DEVICE_PAD2B;
      break;
   case RETRO_DEVICE_PADDLE:
      sms.device[in_port] = DEVICE_PADDLE;
      input_type[in_port] = DEVICE_PADDLE;
      break;
   case RETRO_DEVICE_JOYPAD:
   default:
      input_type[in_port] = sms.device[in_port];
      if (sms.device[in_port] > DEVICE_PADDLE)
         input_type[in_port] = DEVICE_PAD2B;
      break;
   }
}

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_controller_description port_1[] = {
      { "Joypad Auto", RETRO_DEVICE_JOYPAD },
      { "Joypad Port Empty", RETRO_DEVICE_NONE },
      { "MS Joypad 2 Button", RETRO_DEVICE_MSPAD_2B },
      { "MS Paddle Control", RETRO_DEVICE_PADDLE },
   };

   static const struct retro_controller_description port_2[] = {
      { "Joypad Auto", RETRO_DEVICE_JOYPAD },
      { "Joypad Port Empty", RETRO_DEVICE_NONE },
      { "MS Joypad 2 Button", RETRO_DEVICE_MSPAD_2B },
      { "MS Paddle Control", RETRO_DEVICE_PADDLE },
   };

   static const struct retro_controller_info ports[] = {
      { port_1, 4 },
      { port_2, 4 },
      { 0 },
   };
   environ_cb = cb;
   libretro_set_core_options(environ_cb);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

size_t retro_serialize_size(void)
{
   if (libretro_serialize_size == 0)
   {
      /* Something arbitrarily big.*/
      uint8_t *buffer = (uint8_t*)malloc(1000000);
      if (!buffer)
         return 0;

      libretro_serialize_size = system_save_state_mem(buffer, 1000000);
      free(buffer);
   }

   return libretro_serialize_size;
}

bool retro_serialize(void *data, size_t size)
{
   if (size != libretro_serialize_size)
      return 0;

   system_save_state_mem(data, size);
   return 1;
}

bool retro_unserialize(const void *data, size_t size)
{
   if (size != libretro_serialize_size)
      return 0;

   system_load_state_mem(data, size);
   return 1;
}

void *retro_get_memory_data(unsigned type)
{
   switch (type)
   {
      case RETRO_MEMORY_SYSTEM_RAM:
         return sms.wram;
      case RETRO_MEMORY_SAVE_RAM:
         return cart.sram;
   }

   return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   switch (type)
   {
      case RETRO_MEMORY_SYSTEM_RAM:
         return 0x2000;
      case RETRO_MEMORY_SAVE_RAM:
         return 0x8000;
   }

   return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
}
