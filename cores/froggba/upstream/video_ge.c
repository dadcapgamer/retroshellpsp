#include "video_ge.h"
#include "video_ge_utils.h"

#include <pspdebug.h>

int show_4bit_tilemap = 0;
int tilemap_offset = 0;
int gba_generic_counter = 0;

static unsigned int __attribute__((aligned(64))) d_list[4*1024*1024];

static PspGeContext main_context_buffer;
static int main_context_status;

static u16* const index_copy_clut = GBA_VRAM_PALETTE_BUFFER;

#define GBA_DISPLAY_LIST_ID   0x0F00

static psp1_sprite_uv16bit_t* const vram_cache_coords_8bit    = GBA_VRAM_VBUFFER0;
static psp1_sprite_uv16bit_t* const vram_cache_coords_4bit    = GBA_VRAM_VBUFFER0 + 1;
static psp1_sprite_uv16bit_t* const frame_128_256             = GBA_VRAM_VBUFFER0 + 2;
static psp1_sprite_uv16bit_t* const frame_256_256             = GBA_VRAM_VBUFFER0 + 3;
//static psp1_sprite_t*         const screen_tiles              = GBA_VRAM_VBUFFER1;
//static psp1_sprite_t*         const sprite_tiles              = GBA_VRAM_VBUFFER1 + 32*32*4;


static __attribute__((aligned(64))) psp1_sprite_t screen_tiles[32*32*4];
static __attribute__((aligned(64))) psp1_sprite_t sprite_tiles[4];
//static OAM_t oam_attributes_local[128];
u16 __attribute__((aligned(64))) palette_ram_8bit[0x200];
u16 __attribute__((aligned(64))) palette_ram_4bit[0x200];

static struct
{
   unsigned bg_mode;
   unsigned obj_character_vram_mapping;
   unsigned screen_display_BG0;
   unsigned screen_display_BG1;
   unsigned screen_display_BG2;
   unsigned screen_display_BG3;
   unsigned screen_display_OBJ;
   unsigned window0_display_flag;
   unsigned window1_display_flag;
   unsigned obj_window_display_flag;
   unsigned bg0_palette_mode_256;
   unsigned bg0_mosaic;
   unsigned bg0_screen_size;
   unsigned bg1_palette_mode_256;
   unsigned bg1_mosaic;
   unsigned bg1_screen_size;
   unsigned bg2_palette_mode_256;
   unsigned bg2_mosaic;
   unsigned bg2_screen_size;
   unsigned bg3_palette_mode_256;
   unsigned bg3_mosaic;
   unsigned bg3_screen_size;
   unsigned blend_status;
   unsigned blend_first;
   unsigned blend_second;

}previous_status,current_status;
static unsigned frame_count = 0;

void check_gba_video_status(void)
{



   current_status.bg_mode = gba_video_registers->lcd_control.bg_mode;
   current_status.obj_character_vram_mapping = gba_video_registers->lcd_control.obj_character_vram_mapping;
   current_status.screen_display_BG0 = gba_video_registers->lcd_control.screen_display_BG0;
   current_status.screen_display_BG1 = gba_video_registers->lcd_control.screen_display_BG1;
   current_status.screen_display_BG2 = gba_video_registers->lcd_control.screen_display_BG2;
   current_status.screen_display_BG3 = gba_video_registers->lcd_control.screen_display_BG3;
   current_status.screen_display_OBJ = gba_video_registers->lcd_control.screen_display_OBJ;
   current_status.window0_display_flag = gba_video_registers->lcd_control.window0_display_flag;
   current_status.window1_display_flag = gba_video_registers->lcd_control.window1_display_flag;
   current_status.obj_window_display_flag = gba_video_registers->lcd_control.obj_window_display_flag;
   current_status.bg0_palette_mode_256 = gba_video_registers->BG0_control.palette_mode_256;
   current_status.bg0_mosaic = gba_video_registers->BG0_control.mosaic;
   current_status.bg0_screen_size = gba_video_registers->BG0_control.screen_size;
   current_status.bg1_palette_mode_256 = gba_video_registers->BG1_control.palette_mode_256;
   current_status.bg1_mosaic = gba_video_registers->BG1_control.mosaic;
   current_status.bg1_screen_size = gba_video_registers->BG1_control.screen_size;
   current_status.bg2_palette_mode_256 = gba_video_registers->BG2_control.palette_mode_256;
   current_status.bg2_mosaic = gba_video_registers->BG2_control.mosaic;
   current_status.bg2_screen_size = gba_video_registers->BG2_control.screen_size;
   current_status.bg3_palette_mode_256 = gba_video_registers->BG3_control.palette_mode_256;
   current_status.bg3_mosaic = gba_video_registers->BG3_control.mosaic;
   current_status.bg3_screen_size = gba_video_registers->BG3_control.screen_size;
   current_status.blend_status = gba_video_registers->blend_control.effect;
   current_status.blend_first  = *((u8*)&gba_video_registers->blend_control)&0x3F;
   current_status.blend_second = *((u8*)&gba_video_registers->blend_control + 1)&0x3F;


   if (memcmp(&current_status,&previous_status,sizeof(current_status)))
   {
      printf("==================================================================\n");
      printf("status changed @frame :%u\n", frame_count);

      printf("bg_mode : %u\n", current_status.bg_mode);
      printf("obj_character_vram_mapping : %u\n", current_status.obj_character_vram_mapping);
      printf("screen_display_BG0 : %u\n", current_status.screen_display_BG0);
      printf("screen_display_BG1 : %u\n", current_status.screen_display_BG1);
      printf("screen_display_BG2 : %u\n", current_status.screen_display_BG2);
      printf("screen_display_BG3 : %u\n", current_status.screen_display_BG3);
      printf("screen_display_OBJ : %u\n", current_status.screen_display_OBJ);
      printf("window0_display_flag : %u\n", current_status.window0_display_flag);
      printf("window1_display_flag : %u\n", current_status.window1_display_flag);
      printf("obj_window_display_flag : %u\n", current_status.obj_window_display_flag);
      printf("bg0_palette_mode_256 : %u\n", current_status.bg0_palette_mode_256);
      printf("bg0_mosaic : %u\n", current_status.bg0_mosaic);
      printf("bg0_screen_size : %u\n", current_status.bg0_screen_size);
      printf("bg1_palette_mode_256 : %u\n", current_status.bg1_palette_mode_256);
      printf("bg1_mosaic : %u\n", current_status.bg1_mosaic);
      printf("bg1_screen_size : %u\n", current_status.bg1_screen_size);
      printf("bg2_palette_mode_256 : %u\n", current_status.bg2_palette_mode_256);
      printf("bg2_mosaic : %u\n", current_status.bg2_mosaic);
      printf("bg2_screen_size : %u\n", current_status.bg2_screen_size);
      printf("bg3_palette_mode_256 : %u\n", current_status.bg3_palette_mode_256);
      printf("bg3_mosaic : %u\n", current_status.bg3_mosaic);
      printf("blend_status : %u\n", current_status.blend_status);

      static char buffer[7];
      buffer[6] = '\0';
      unsigned i;

      for (i = 0; i < 6 ; i++)
          buffer[5 - i] = (current_status.blend_first & (1 << i))? '1' : '0';
      printf("blend_first  : %s\n",buffer);

      for (i = 0; i < 6 ; i++)
          buffer[5 - i] = (current_status.blend_second & (1 << i))? '1' : '0';
      printf("blend_second : %s\n",buffer);

      printf("==================================================================\n\n");


   }
   previous_status = current_status;
   frame_count++;

}

void list_finish_callback(int id)
{
   if (id != GBA_DISPLAY_LIST_ID)
      return;

//   if (read_system_buttons()&PSP_CTRL_VOLUP)
   SceCtrlData pad;
   sceCtrlPeekBufferPositive(&pad,1);
   debug_setpos(0,0);
   debug_printf("palette_ram[0] :0x%04X\n", palette_ram[0]);
   if (pad.Ly > 200)
   {
      debug_setpos(0,0);

      debug_printf("frame :%u\n", frame_count);
      debug_printf("bg_mode : %u\n", current_status.bg_mode);
      debug_printf("obj_character_vram_mapping : %u\n", current_status.obj_character_vram_mapping);
      debug_printf("screen_display_BG0 : %u\n", current_status.screen_display_BG0);
      debug_printf("screen_display_BG1 : %u\n", current_status.screen_display_BG1);
      debug_printf("screen_display_BG2 : %u\n", current_status.screen_display_BG2);
      debug_printf("screen_display_BG3 : %u\n", current_status.screen_display_BG3);
      debug_printf("screen_display_OBJ : %u\n", current_status.screen_display_OBJ);
      debug_printf("window0_display_flag : %u\n", current_status.window0_display_flag);
      debug_printf("window1_display_flag : %u\n", current_status.window1_display_flag);
      debug_printf("obj_window_display_flag : %u\n", current_status.obj_window_display_flag);
      debug_printf("bg0_palette_mode_256 : %u\n", current_status.bg0_palette_mode_256);
      debug_printf("bg0_mosaic : %u\n", current_status.bg0_mosaic);
      debug_printf("bg0_screen_size : %u\n", current_status.bg0_screen_size);
      debug_printf("bg1_palette_mode_256 : %u\n", current_status.bg1_palette_mode_256);
      debug_printf("bg1_mosaic : %u\n", current_status.bg1_mosaic);
      debug_printf("bg1_screen_size : %u\n", current_status.bg1_screen_size);
      debug_printf("bg2_palette_mode_256 : %u\n", current_status.bg2_palette_mode_256);
      debug_printf("bg2_mosaic : %u\n", current_status.bg2_mosaic);
      debug_printf("bg2_screen_size : %u\n", current_status.bg2_screen_size);
      debug_printf("bg3_palette_mode_256 : %u\n", current_status.bg3_palette_mode_256);
      debug_printf("bg3_mosaic : %u\n", current_status.bg3_mosaic);
      debug_printf("blend_status : %u\n", current_status.blend_status);

      static char buffer[7];
      buffer[6] = '\0';
      unsigned i;

      for (i = 0; i < 6 ; i++)
          buffer[5 - i] = (current_status.blend_first & (1 << i))? '1' : '0';
      debug_printf("blend_first  : %s\n",buffer);

      for (i = 0; i < 6 ; i++)
          buffer[5 - i] = (current_status.blend_second & (1 << i))? '1' : '0';
      debug_printf("blend_second : %s\n",buffer);
   }

//   sceGuSetAllStatus(main_context_status);
   sceGeRestoreContext(&main_context_buffer);
//   sceGuClearColor(0x00000000);


}

void init_video_ge(void)
{
   int i;

   for (i = 0; i < 256; i++)
      index_copy_clut[i]=i;

   psp1_sprite_t* tile = screen_tiles;

   for (i=0; i<32*32; i++)
   {
      tile->v0.x=((i &  0x1F) << 3);
      tile->v0.y=((i >>    5) << 3);
      tile->v0.z=0;
      tile->v0.u=0;
      tile->v0.v=0;

      tile->v1.x=((i &  0x1F) << 3) + 8;
      tile->v1.y=((i >>    5) << 3) + 8;
      tile->v1.z=0;
      tile->v1.u=1;
      tile->v1.v=1;
      tile++;
   }
   for (i=0; i<32*32; i++)
   {
      tile->v0.x=((i &  0x1F) << 3);
      tile->v0.y=((i >>    5) << 3);
      tile->v0.z=0;
      tile->v0.u=1;
      tile->v0.v=0;

      tile->v1.x=((i &  0x1F) << 3) + 8;
      tile->v1.y=((i >>    5) << 3) + 8;
      tile->v1.z=0;
      tile->v1.u=0;
      tile->v1.v=1;
      tile++;
   }
   for (i=0; i<32*32; i++)
   {
      tile->v0.x=((i &  0x1F) << 3);
      tile->v0.y=((i >>    5) << 3);
      tile->v0.z=0;
      tile->v0.u=0;
      tile->v0.v=1;

      tile->v1.x=((i &  0x1F) << 3) + 8;
      tile->v1.y=((i >>    5) << 3) + 8;
      tile->v1.z=0;
      tile->v1.u=1;
      tile->v1.v=0;
      tile++;
   }
   for (i=0; i<32*32; i++)
   {
      tile->v0.x=((i &  0x1F) << 3);
      tile->v0.y=((i >>    5) << 3);
      tile->v0.z=0;
      tile->v0.u=1;
      tile->v0.v=1;

      tile->v1.x=((i &  0x1F) << 3) + 8;
      tile->v1.y=((i >>    5) << 3) + 8;
      tile->v1.z=0;
      tile->v1.u=0;
      tile->v1.v=0;
      tile++;
   }







   frame_128_256->v0.x = 0;
   frame_128_256->v0.y = 0;
   frame_128_256->v0.u = 0;
   frame_128_256->v0.v = 0;
   frame_128_256->v1.x = 256;
   frame_128_256->v1.y = 256;
   frame_128_256->v1.u = 128;
   frame_128_256->v1.v = 256;

   vram_cache_coords_8bit->v0.x = 0;
   vram_cache_coords_8bit->v0.y = 0;
   vram_cache_coords_8bit->v0.u = 0;
   vram_cache_coords_8bit->v0.v = 0;
   vram_cache_coords_8bit->v1.x = 128;
   vram_cache_coords_8bit->v1.y = 256;
   vram_cache_coords_8bit->v1.u = 128;
   vram_cache_coords_8bit->v1.v = 256;

   frame_256_256->v0.x = 0;
   frame_256_256->v0.y = 0;
   frame_256_256->v0.u = 0;
   frame_256_256->v0.v = 0;
   frame_256_256->v1.x = 256;
   frame_256_256->v1.y = 256;
   frame_256_256->v1.u = 256;
   frame_256_256->v1.v = 256;

   vram_cache_coords_4bit->v0.x = 0;
   vram_cache_coords_4bit->v0.y = 0;
   vram_cache_coords_4bit->v0.u = 0;
   vram_cache_coords_4bit->v0.v = 0;
   vram_cache_coords_4bit->v1.x = 256;
   vram_cache_coords_4bit->v1.y = 256;
   vram_cache_coords_4bit->v1.u = 256;
   vram_cache_coords_4bit->v1.v = 256;

   sprite_tiles->v0.x = 0;
   sprite_tiles->v0.y = 0;
   sprite_tiles->v0.z = 200;
   sprite_tiles->v0.u = 0;
   sprite_tiles->v0.v = 0;

   sprite_tiles->v1.x = 8;
   sprite_tiles->v1.y = 8;
   sprite_tiles->v1.z = 200;
   sprite_tiles->v1.u = 1;
   sprite_tiles->v1.v = 1;

   sprite_tiles[1] = *sprite_tiles;
   sprite_tiles[2] = *sprite_tiles;
   sprite_tiles[3] = *sprite_tiles;

   sprite_tiles[1].v0.u = 1;
   sprite_tiles[1].v1.u = 0;

   sprite_tiles[2].v0.v = 1;
   sprite_tiles[2].v1.v = 0;

   sprite_tiles[3].v0.u = 1;
   sprite_tiles[3].v1.u = 0;
   sprite_tiles[3].v0.v = 1;
   sprite_tiles[3].v1.v = 0;

}

static void gba_upload_vram (void)
{
   sceGuDisable(GU_SCISSOR_TEST);
   sceGuDisable(GU_DEPTH_TEST);

   sceGuEnable(GU_TEXTURE_2D);

   sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
   sceGuClutMode(GU_PSM_5551,0,0xFF,0);
   sceGuClutLoad(32, index_copy_clut);


   sceGuDrawBufferList(GU_PSM_5551, GBA_VRAMTEXTURE_8bit_GU, 128);
   sceGuTexImage(0, 128, 256, 128, vram);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_8bit));

   sceGuDrawBufferList(GU_PSM_5551, (u16*)GBA_VRAMTEXTURE_8bit_GU + (128*256), 128);
   sceGuTexImage(0, 128, 256, 128, vram + (128*256));
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_8bit));

   sceGuDrawBufferList(GU_PSM_5551, (u16*)GBA_VRAMTEXTURE_8bit_GU + (128*256*2), 128);
   sceGuTexImage(0, 128, 256, 128, vram + (128*256) * 2);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_8bit));

   sceGuTexMode(GU_PSM_T4, 0, 0, GU_FALSE);
   sceGuClutMode(GU_PSM_5551,0,0xFF,0);
   sceGuClutLoad(32, index_copy_clut);

   sceGuDrawBufferList(GU_PSM_5551, GBA_VRAMTEXTURE_4bit_GU, 256);
   sceGuTexImage(0, 256, 256, 256, vram);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_4bit));

   sceGuDrawBufferList(GU_PSM_5551, (u16*)GBA_VRAMTEXTURE_4bit_GU + (256*256) , 256);
   sceGuTexImage(0, 256, 256, 256, vram + (256 * 256 / 2));
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_4bit));

   sceGuDrawBufferList(GU_PSM_5551, (u16*)GBA_VRAMTEXTURE_4bit_GU + (256*256) * 2, 256);
   sceGuTexImage(0, 256, 256, 256, vram + (256 * 256 / 2)*2);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 2, NULL,(void*)(vram_cache_coords_4bit));

}


static void draw_bg_map_256(BGMAP_TEXT_t* map_data, unsigned character_base_block, unsigned offsetX, unsigned offsetY)
{
   return;
   psp1_sprite_t* tile_coords = sceGuGetMemory(32*32*sizeof(psp1_sprite_t));
   unsigned int i;

   sceGuClutLoad(32, palette_ram_8bit);
   sceGuClutMode(GU_PSM_5551,0,0xFF,0);

   sceGuTexImage(0, 128, 256, 128, GBA_VRAMTEXTURE_8bit + character_base_block * 16 * 1024);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
         GU_TRANSFORM_2D, 32 * 32 * 2, NULL,(void*)tile_coords);

}


static void draw_bg_map_16(BGMAP_TEXT_t* map_data, unsigned character_base_block)
{

   unsigned int i;
   u16* tile_data = GBA_VRAMTEXTURE_4bit +  character_base_block * 32 * 1024 ;

   for (i = 0; i < 32*32; i++)
   {
      sceGuTexImage(0, 8, 8, 8,
                    tile_data + (map_data[i].tile_number << 6));

      sceGuClutMode(GU_PSM_5551,0,0x0F,map_data[i].palette_id);

      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_8BIT | GU_VERTEX_16BIT |
                     GU_TRANSFORM_3D, 2, NULL,(void*)(screen_tiles + (map_data[i].flip_XY << 10) + i));

   }
}



static void draw_bg(REG_BGxCNT_t* BG_control, REG_BGxOFS_t* BG_scroll)
{
   BGMAP_TEXT_t* map_data;
   unsigned offsetX, offsetY;

   map_data = (BGMAP_TEXT_t*)(vram + BG_control->screen_base_block * 0x800);


   offsetX = BG_scroll->X;
   offsetY = BG_scroll->Y;


   sceGuTexMode(GU_PSM_T16, 0, 0, GU_TRUE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
   sceGuTexScale_8bit(1.0,1.0);
   sceGuEnable(GU_TEXTURE_2D);
   sceGuDepthOffset(100 + BG_control->priority);



   if (BG_control->palette_mode_256)
   {
      sceGuClutLoad(32, palette_ram_8bit);

      draw_bg_map_256(map_data,BG_control->character_base_block, offsetX, offsetY);

      switch (BG_control->screen_size)
      {
      case 1:
//         printf("1!!!\n");
         draw_bg_map_256(map_data + 0x800, BG_control->character_base_block, offsetX + 256, offsetY);
         break;

      case 2:
         printf("2!!!\n");
         draw_bg_map_256(map_data + 0x800, BG_control->character_base_block, offsetX, offsetY + 256);
         break;

      case 3:
         printf("3!!!\n");
         draw_bg_map_256(map_data + 0x800, BG_control->character_base_block, offsetX + 256, offsetY);
         draw_bg_map_256(map_data + 0x800 * 2, BG_control->character_base_block, offsetX, offsetY + 256);
         draw_bg_map_256(map_data + 0x800 * 3, BG_control->character_base_block, offsetX + 256, offsetY + 256);
      }
   }
   else
   {
      sceGuClutLoad(32, palette_ram_4bit);

      sceGuOffset_(offsetX, offsetY);
      draw_bg_map_16(map_data,BG_control->character_base_block);

      switch (BG_control->screen_size)
      {
      case 1:
         sceGuOffset_(offsetX + 256, offsetY);
         draw_bg_map_16(map_data + 0x800, BG_control->character_base_block);
         break;

      case 2:
         sceGuOffset_(offsetX, offsetY + 256);
         draw_bg_map_16(map_data + 0x800, BG_control->character_base_block);
         break;

      case 3:
         sceGuOffset_(offsetX + 256, offsetY);
         draw_bg_map_16(map_data + 0x800, BG_control->character_base_block);
         sceGuOffset_(offsetX, offsetY + 256);
         draw_bg_map_16(map_data + 0x800 * 2, BG_control->character_base_block);
         sceGuOffset_(offsetX + 256, offsetY + 256);
         draw_bg_map_16(map_data + 0x800 * 3, BG_control->character_base_block);
      }
   }


}

static void draw_sprites(void)
{
   int i;

   psp1_sprite_t* tile_coords = sceGuGetMemory(128*sizeof(psp1_sprite_t));
//   sceGuClutLoad(32, palette_ram + 256);


   int mapping_mode_1D = gba_video_registers->lcd_control.obj_character_vram_mapping;

   typedef struct {
      int w;
      int h;
   }obj_size_t;

   static const obj_size_t obj_size[4][4]=
   {
      {{8,8},{16,16},{32,32},{64,64}},
      {{16,8},{32,8},{32,16},{64,32}},
      {{8,16},{8,32},{16,32},{32,64}},
      {{0,0},{0,0},{0,0},{0,0}}

   };

   sceGuTexMode(GU_PSM_T16, 0, 0, GU_TRUE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
   sceGuTexScale_8bit(1.0,1.0);

   sceGuClutLoad(32, palette_ram_4bit+256);

//   for (i=0; i < 128; i++)
   for (i=127; i >= 0; i--)
   {
      if (oam_attributes[i].obj_disable_flag)
         continue;

      if (oam_attributes[i].mode > 1)
         continue;

      if (oam_attributes[i].palette_mode_256)
      {
         sceGuClutLoad(32, palette_ram_8bit+256);
         printf("256!\n");
         sceGuClutLoad(32, palette_ram_4bit+256);

      }
      else
      {
//         printf("16!\n");


         int size_id = oam_attributes[i].size;
         int shape_id = oam_attributes[i].shape;

         if (shape_id>3)
            continue;

         int width = obj_size[shape_id][size_id].w;
         int height = obj_size[shape_id][size_id].h;



         psp1_sprite_t temp;

         temp = sprite_tiles[oam_attributes[i].flip_XY];
//         temp = sprite_tiles[0];

         temp.v1.x = width;
         temp.v1.y = height;

         *tile_coords = temp;

         sceGuOffset_(-oam_attributes[i].X, -oam_attributes[i].Y);

         if (mapping_mode_1D)
         {
            sceGuTexImage(0, width, height, width,
                          GBA_VRAMTEXTURE_4bit +  256*256*2 + oam_attributes[i].id * 64);
         }
         else
         {
//            printf("2d!!\n");
            sceGuTexImage(0, width, height, 256,
                          GBA_VRAMTEXTURE_4bit +  256*256*2 + oam_attributes[i].id * 64);
         }

         sceGuClutMode(GU_PSM_5551,0,0x0F,oam_attributes[i].palette_id);
         sceGuDrawArray(GU_SPRITES, GU_TEXTURE_8BIT | GU_VERTEX_16BIT |
               GU_TRANSFORM_3D, 2, NULL,(void*)(tile_coords++));



      }
   }




}

static void gba_draw_bg_mode0 (void)
{

//   goto show_tiles;

   RETRO_PERFORMANCE_INIT(draw_bgs_procs);
   RETRO_PERFORMANCE_START(draw_bgs_procs);

   sceGuEnable(GU_BLEND);
   sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0xFFFFFFFF,0xFFFFFFFF);
//   sceGuEnable(GU_DEPTH_TEST);
   sceGuDepthMask(GU_FALSE);
   sceGuDepthFunc(GU_GEQUAL);
//   sceGuDisable(GU_BLEND);
   //BG0


   if (gba_video_registers->lcd_control.screen_display_BG3)
      draw_bg(&gba_video_registers->BG3_control, &gba_video_registers->BG3_scroll);
   if (gba_video_registers->lcd_control.screen_display_BG2)
      draw_bg(&gba_video_registers->BG2_control, &gba_video_registers->BG2_scroll);
   if (gba_video_registers->lcd_control.screen_display_BG1)
      draw_bg(&gba_video_registers->BG1_control, &gba_video_registers->BG1_scroll);
   if (gba_video_registers->lcd_control.screen_display_BG0)
      draw_bg(&gba_video_registers->BG0_control, &gba_video_registers->BG0_scroll);

   RETRO_PERFORMANCE_STOP(draw_bgs_procs);

   RETRO_PERFORMANCE_INIT(draw_sprites_proc);
   RETRO_PERFORMANCE_START(draw_sprites_proc);
//   draw_sprites_old();
//   sceGuClutMode(GU_PSM_5551,0,0xFF,32);
   draw_sprites();
   RETRO_PERFORMANCE_STOP(draw_sprites_proc);




   return;

show_tiles:

   sceGuTexMode(GU_PSM_T16, 0, 0, GU_TRUE);
   sceGuClutMode(GU_PSM_5551,0,0xFF,0);
   sceGuClutLoad(32, palette_ram);
   sceGuDisable(GU_BLEND);

   sceGuEnable(GU_TEXTURE_2D);
   sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
//   sceGuClutMode(GU_PSM_4444,0,0xFF,0);
//   sceGuClutLoad(32, index_copy_clut);

   if (show_4bit_tilemap)
   {
      sceGuTexImage(0, 256, 256, 256, ((u16*)GBA_VRAMTEXTURE_4bit)
            + 256 * 256 * tilemap_offset);

      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
            GU_TRANSFORM_2D, 2, NULL,(void*)frame_256_256);
   }
   else
   {
      sceGuTexImage(0, 128, 256, 128, ((u16*)GBA_VRAMTEXTURE_8bit)
            + 128 * 256 * tilemap_offset);

      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
            GU_TRANSFORM_2D, 2, NULL,(void*)frame_128_256);
   }




}
typedef void(*gba_draw_bg_t)(void);
gba_draw_bg_t gba_draw_bg[8] =
{
   gba_draw_bg_mode0,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};



void update_scanline_ge(void)
{
//   int i;

//   if (gba_video_registers->v_count_reg.LY != 100)
//      return;
//   for (i=0; i<128; i++)
//   {
//      oam_attributes_local[i]=oam_attributes[i];
//   }



}


void update_frame_ge(void)
{
   check_gba_video_status();


//   return;

//   printf("video mode: %u\n", gba_video_registers->lcd_control.bg_mode);

   int i;

   palette_ram_8bit[0]=palette_ram[0]&0x7FFF;

   for (i=1; i<256;i++)
      palette_ram_8bit[i]=palette_ram[i]|0x8000;

   palette_ram_8bit[256]=palette_ram[256]&0x7FFF;

   for (i=257; i<512;i++)
      palette_ram_8bit[i]=palette_ram[i]|0x8000;

   for (i=0; i<512;i++)
   {
      if (i&0xF)
         palette_ram_4bit[i]=palette_ram_8bit[i]|0x8000;

      else
         palette_ram_4bit[i]=palette_ram_8bit[i]&0x7FFF;

   }


//   sceKernelDcacheWritebackAll();
//   sceKernelDcacheWritebackInvalidateAll();
   RETRO_PERFORMANCE_INIT(gu_sync_time);
   RETRO_PERFORMANCE_START(gu_sync_time);
   sceGuSync(0,0);
   RETRO_PERFORMANCE_STOP(gu_sync_time);

   main_context_status= sceGuGetAllStatus();
   sceGeSaveContext(&main_context_buffer);
   sceGuSetCallback(GU_CALLBACK_FINISH, list_finish_callback);


   sceGuStart(GU_DIRECT, d_list);

   init_3Dprojection();
   setup_3Dprojection_16bit();
   sceGuDisable(GU_DEPTH_TEST);

//   sceGuDepthBuffer((void*)VRAM_DEPTH,512);
   sceGuDepthMask(GU_TRUE);
   sceGuDepthRange(0,65535);
   sceGuDisable(GU_CULL_FACE);
   sceGuDisable(GU_CLIP_PLANES);

//   sceGuTexFilter(GU_NEAREST,GU_NEAREST);

   RETRO_PERFORMANCE_INIT(gba_upload_vram_proc);
   RETRO_PERFORMANCE_START(gba_upload_vram_proc);
   gba_upload_vram();
   RETRO_PERFORMANCE_STOP(gba_upload_vram_proc);

//   sceGuDrawBufferList(GU_PSM_5551, GBA_FRAME_TEXTURE_GU, GBA_LINE_SIZE);
   sceGuDrawBufferList(GU_PSM_8888, GBA_FRAME_TEXTURE_GU, GBA_LINE_SIZE);
   sceGuDepthBuffer(GBA_DEPTH_BUFFER, GBA_LINE_SIZE);

   sceGuScissor(0,0,GBA_SCREEN_WIDTH,GBA_SCREEN_HEIGHT);
   sceGuScissor(0,0,240,160);
   sceGuEnable(GU_SCISSOR_TEST);


//   sceGuClearColor(0x000000FF);
//   sceGuClearColor((palette_ram[0]&0x1F<<3)|(palette_ram[0]&0x3E0<<6)|(palette_ram[0]&0x7C00<<9)|0xFF000000);
   sceGuClearColor(GU_COLOR(((palette_ram[0]>> 0) & 31) / 31.0f,
                            ((palette_ram[0]>> 5) & 31) / 31.0f,
                            ((palette_ram[0]>>10) & 31) / 31.0f,
                            1.0));
   sceGuClear(GU_COLOR_BUFFER_BIT);

//   if (gba_draw_bg[gba_video_registers->lcd_control.bg_mode])
//      gba_draw_bg[gba_video_registers->lcd_control.bg_mode]();


   RETRO_PERFORMANCE_INIT(gba_draw_bg_mode0_proc);
   RETRO_PERFORMANCE_START(gba_draw_bg_mode0_proc);
   gba_draw_bg_mode0();
   RETRO_PERFORMANCE_STOP(gba_draw_bg_mode0_proc);

//   sceGuFinish();
   sceGuFinishId(GBA_DISPLAY_LIST_ID);

//   RETRO_PERFORMANCE_INIT(gu_sync_time);
//   RETRO_PERFORMANCE_START(gu_sync_time);
//   sceGuSync(0,0);
//   RETRO_PERFORMANCE_STOP(gu_sync_time);

}
