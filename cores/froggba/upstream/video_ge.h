#ifndef VIDEO_GE_H
#define VIDEO_GE_H

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pspthreadman.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <stddef.h>

#define TO_UNCACHED_PTR(ptr)     ((void *)((u32)(ptr)|0x40000000))
#define TO_CACHED_PTR(ptr)       ((void *)((u32)(ptr)&~0x40000000))

#define FROM_GU_POINTER(ptr)     ((void *)((u32)(ptr)|0x44000000))
#define TO_GU_POINTER(ptr)       ((void *)((u32)(ptr)&~0x44000000))



#define PSP_VRAM_MAX             ((u16*)0x44200000)
#define GBA_VRAMTEXTURE_4bit     (PSP_VRAM_MAX           - (256 * 256) * 3)
#define GBA_VRAMTEXTURE_8bit     (GBA_VRAMTEXTURE_4bit   - (128 * 256) * 3)
#define GBA_DEPTH_BUFFER         (GBA_VRAMTEXTURE_8bit   - GBA_LINE_SIZE * GBA_SCREEN_HEIGHT)
extern u16 froggba_framebuffer[GBA_LINE_SIZE * 256];
#define GBA_FRAME_TEXTURE        (froggba_framebuffer)

#define GBA_VRAM_PALETTE_BUFFER  (GBA_FRAME_TEXTURE - 256 * 3)
#define GBA_VRAM_VBUFFER0         ((psp1_sprite_uv16bit_t*)GBA_VRAM_PALETTE_BUFFER - 32)
#define GBA_VRAM_VBUFFER1         ((psp1_sprite_t*)GBA_VRAM_VBUFFER0       - 32*32*4 - 4)


#define GBA_VRAMTEXTURE_8bit_GU  TO_GU_POINTER(GBA_VRAMTEXTURE_8bit)
#define GBA_VRAMTEXTURE_4bit_GU  TO_GU_POINTER(GBA_VRAMTEXTURE_4bit)
#define GBA_FRAME_TEXTURE_GU     TO_GU_POINTER(GBA_FRAME_TEXTURE)


typedef struct
{
   unsigned bg_mode                      : 3;
   unsigned reserved_cgb_mode            : 1;
   unsigned display_frame_select         : 1;
   unsigned h_blank_interval_free        : 1;
   unsigned obj_character_vram_mapping   : 1;
   unsigned forced_blank                 : 1;
   unsigned screen_display_BG0           : 1;
   unsigned screen_display_BG1           : 1;
   unsigned screen_display_BG2           : 1;
   unsigned screen_display_BG3           : 1;
   unsigned screen_display_OBJ           : 1;
   unsigned window0_display_flag         : 1;
   unsigned window1_display_flag         : 1;
   unsigned obj_window_display_flag      : 1;
} __attribute__((packed)) REG_DISPCNT_t;

typedef struct
{
   unsigned swap_on                      : 1;
   unsigned                              : 15;
} __attribute__((packed)) REG_GREENSWAP_t;

typedef struct
{
   unsigned v_blank_flag                 : 1;
   unsigned h_blank_flag                 : 1;
   unsigned v_counter_flag               : 1;
   unsigned v_blank_irq_enable           : 1;
   unsigned h_blank_irq_enable           : 1;
   unsigned v_counter_irq_enable         : 1;

   uint8_t v_count_setting;
} __attribute__((packed)) REG_DISPSTAT_t;

typedef struct
{
   uint8_t LY;
   unsigned :8;
} __attribute__((packed)) REG_VCOUNT_t;

typedef struct
{
   unsigned priority                     : 2;
   unsigned character_base_block         : 2;
   unsigned                              : 2;
   unsigned mosaic                       : 1;
   unsigned palette_mode_256             : 1;

   unsigned screen_base_block            : 5;
   unsigned display_area_overflow        : 1;
   unsigned screen_size                  : 2;
} __attribute__((packed)) REG_BGxCNT_t;

typedef struct
{
   unsigned X                       : 9;
   unsigned                         : 7;
   unsigned Y                       : 9;
   unsigned                         : 7;
} __attribute__((packed)) REG_BGxOFS_t;

typedef struct
{
   unsigned value_times_256                   : 27;
   unsigned sign                              : 1;
} __attribute__((packed)) REG_BGxROTSCLREF_t;

typedef struct
{
   unsigned value_times_256                   : 15;
   unsigned sign                              : 1;
} __attribute__((packed)) ROTSCLP_t;

typedef struct
{
   unsigned BG0_enable                   : 1;
   unsigned BG1_enable                   : 1;
   unsigned BG2_enable                   : 1;
   unsigned BG3_enable                   : 1;
   unsigned OBJ_enable                   : 1;
   unsigned color_effect_enable          : 1;
} __attribute__((packed)) REG_WINCNT_t;

typedef struct
{
   unsigned X                   : 4;
   unsigned Y                   : 4;
} __attribute__((packed)) REG_MOSAIC_t;

typedef struct
{
   unsigned BG0_src                   : 1;
   unsigned BG1_src                   : 1;
   unsigned BG2_src                   : 1;
   unsigned BG3_src                   : 1;
   unsigned OBJ_src                   : 1;
   unsigned BD_src                    : 1;
   unsigned effect                    : 2;

   unsigned BG0_dst                   : 1;
   unsigned BG1_dst                   : 1;
   unsigned BG2_dst                   : 1;
   unsigned BG3_dst                   : 1;
   unsigned OBJ_dst                   : 1;
   unsigned BD_dst                    : 1;
} __attribute__((packed)) REG_BLDCNT_t;

typedef struct
{
   unsigned src_coeff             : 5;
   unsigned                       : 3;
   unsigned dst_coeff             : 5;
   unsigned                       : 3;
} __attribute__((packed)) REG_BLDALPHA_t;

typedef struct
{
   unsigned coeff                  : 5;
   unsigned                        : 27;
} __attribute__((packed)) REG_BLDY_t;

static struct
{
//0x4000000
   REG_DISPCNT_t lcd_control;
   REG_GREENSWAP_t green_swap;
   REG_DISPSTAT_t lcd_status;
   REG_VCOUNT_t v_count_reg;
   REG_BGxCNT_t BG0_control;
   REG_BGxCNT_t BG1_control;
   REG_BGxCNT_t BG2_control;
   REG_BGxCNT_t BG3_control;
//0x4000010
   REG_BGxOFS_t BG0_scroll;
   REG_BGxOFS_t BG1_scroll;
   REG_BGxOFS_t BG2_scroll;
   REG_BGxOFS_t BG3_scroll;
//0x4000020
   ROTSCLP_t BG2_dx;
   ROTSCLP_t BG2_dmx;
   ROTSCLP_t BG2_dy;
   ROTSCLP_t BG2_dmy;
   REG_BGxROTSCLREF_t BG2_rotscl_ref_X;
   REG_BGxROTSCLREF_t BG2_rotscl_ref_Y;
//0x4000030
   ROTSCLP_t BG3_dx;
   ROTSCLP_t BG3_dmx;
   ROTSCLP_t BG3_dy;
   ROTSCLP_t BG3_dmy;
   REG_BGxROTSCLREF_t BG3_rotscl_ref_X;
   REG_BGxROTSCLREF_t BG3_rotscl_ref_Y;
//0x4000040
   uint8_t WIN0_right;
   uint8_t WIN0_left;
   uint8_t WIN1_right;
   uint8_t WIN1_left;
   uint8_t WIN0_bottom;
   uint8_t WIN0_top;
   uint8_t WIN1_bottom;
   uint8_t WIN1_top;
   REG_WINCNT_t WIN0_control;
   REG_WINCNT_t WIN1_control;
   REG_WINCNT_t WIN_OUT_control;
   REG_WINCNT_t WIN_OBJ_control;
   REG_MOSAIC_t BG_mosaic;
   REG_MOSAIC_t OBJ_mosaic;
   unsigned :16;
//0x4000050
   REG_BLDCNT_t blend_control;
   REG_BLDALPHA_t blend_alpha;
   REG_BLDY_t blend_luma;
} *const gba_video_registers = (void *const)io_registers;

//BG MAP
typedef struct
{
   unsigned tile_number          :10;
//   unsigned flip_X               :1;
//   unsigned flip_Y               :1;
   unsigned flip_XY              :2;
   unsigned palette_id           :4;
}__attribute__((packed)) BGMAP_TEXT_t;

typedef struct
{
   uint8_t tile_number;
}__attribute__((packed)) BGMAP_ROTSCL_t;

//OAM
typedef struct
{
   union
   {
      struct
      {
         uint8_t Y;
         unsigned rotscl_flag          :1;
         unsigned doublesize_flag      :1;
         unsigned mode                 :2;
         unsigned mosaic               :1;
         unsigned palette_mode_256     :1;
         unsigned shape                :2;
      }__attribute__((packed));
      struct
      {
         unsigned                      :9;
         unsigned obj_disable_flag     :1;
      }__attribute__((packed));
   };

   union
   {
      struct
      {
         unsigned X                    :9;
         unsigned rotscl_param_id      :5;
         unsigned size                 :2;
      }__attribute__((packed));
      struct
      {
         unsigned                      :12;
         unsigned flip_XY              :2;
      }__attribute__((packed));
   }__attribute__((packed));

   struct
   {
      unsigned id                   :10;
      unsigned prio                 :2;
      unsigned palette_id           :4;
   }__attribute__((packed));

   unsigned                         :16;
}__attribute__((packed)) OAM_t;

typedef struct
{
   unsigned :32;
   unsigned :16;
   ROTSCLP_t dx;

   unsigned :32;
   unsigned :16;
   ROTSCLP_t dmx;

   unsigned :32;
   unsigned :16;
   ROTSCLP_t dy;

   unsigned :32;
   unsigned :16;
   ROTSCLP_t dmy;
}__attribute__((packed)) OAM_ROTSCLP_t;

static OAM_t* const oam_attributes = (void *const)oam_ram;

extern int show_4bit_tilemap;
extern int tilemap_offset;
extern int gba_generic_counter;

#endif // VIDEO_GE_H
