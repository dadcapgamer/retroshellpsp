#ifndef VIDEO_GE_UTILS_H
#define VIDEO_GE_UTILS_H

#include "video_ge.h"

typedef struct __attribute__((packed)) psp1_vertex
{
   s8 u,v;
   s16 x,y,z;
} psp1_vertex_t;
typedef struct __attribute__((packed)) psp1_sprite
{
   psp1_vertex_t v0;
   psp1_vertex_t v1;

} psp1_sprite_t;

typedef struct __attribute__((packed)) psp1_vertex_uv16bit
{
   s16 u,v;
   s16 x,y,z;
} psp1_vertex_uv16bit_t;
typedef struct __attribute__((packed)) psp1_sprite_uv16bit
{
   psp1_vertex_uv16bit_t v0;
   psp1_vertex_uv16bit_t v1;

} psp1_sprite_uv16bit_t;
static inline void sceGuTexScale_8bit(float u, float v)
{
   sceGuTexScale(128.0 / u,128.0 / v);
}

static inline void sceGuTexScale_16bit(float u, float v)
{
   sceGuTexScale(32768.0 * u,32768.0 * v);
}

#define sceGuOffset_(X,Y) sceGuOffset(2048+(X),2048+(Y))

static inline void init_3Dprojection(void)
{
   ScePspFMatrix4 m_identity =
   {
      {1.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
   };

   sceGuOffset(2048,2048);
   sceGuViewport(2048,2048,4096,-4096);
   sceGuSetMatrix(GU_VIEW, &m_identity);
   sceGuSetMatrix(GU_MODEL, &m_identity);

}



static inline void setup_3Dprojection_8bit (void)
{
   static ScePspFMatrix4 m_proj8bit =
   {
      {128.0f/2048.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 128.0f/2048.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 128.0f/65536.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
   };

   sceGuSetMatrix(GU_PROJECTION, &m_proj8bit);
}

static inline void setup_3Dprojection_16bit (void)
{
   static ScePspFMatrix4 m_proj16bit =
   {
      {32768.0f/2048.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 32768.0f/2048.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 32768.0f/65536.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
   };

   sceGuSetMatrix(GU_PROJECTION, &m_proj16bit);
}

static inline void setup_3Dprojection_32bit (void)
{
   static ScePspFMatrix4 m_proj32bit =
   {
      {1.0f/2048.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 1.0f/2048.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f/65536.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
   };

   sceGuSetMatrix(GU_PROJECTION, &m_proj32bit);
}


typedef struct
{
   unsigned int* start;
   unsigned int* current;
   int parent_context;
} GuDisplayList;
extern GuDisplayList* gu_list;

#if 1
#define sendCommandi sendCommandi_
#define sendCommandf sendCommandf_
#else
void sendCommandi(int cmd, int argument);
void sendCommandiStall(int cmd, int argument);
void sendCommandf(int cmd, float argument);
#endif

#define sceGuDrawArray  sceGuDrawArray_
#define sceGuClutLoad   sceGuClutLoad_
#define sceGuClutMode   sceGuClutMode_
#define sceGuTexImage   sceGuTexImage_

static inline void sendCommandi_(int cmd, int argument)
{
   *(gu_list->current++) = (cmd << 24) | (argument & 0xffffff);
}

static inline void sendCommandf_(int cmd, float argument)
{
   union
   {
      float f;
      unsigned int i;
   } t;
   t.f = argument;

   sendCommandi_(cmd,t.i >> 8);
}

static inline void sceGuDrawArray_(int prim, int vtype, int count, const void* indices, const void* vertices)
{
   sendCommandi(18,vtype);
   sendCommandi(16,(((unsigned int)indices) >> 8) & 0xf0000);
   sendCommandi(2,((unsigned int)indices) & 0xffffff);
   sendCommandi(16,(((unsigned int)vertices) >> 8) & 0xf0000);
   sendCommandi(1,((unsigned int)vertices) & 0xffffff);
   sendCommandi(4,(prim << 16)|count);
}

static inline void sceGuClutLoad_(int num_blocks, const void* cbp)
{
   sendCommandi(176,((unsigned int)cbp) & 0xffffff);
   sendCommandi(177,(((unsigned int)cbp) >> 8) & 0xf0000);
   sendCommandi(196,num_blocks);
}
static inline void sceGuClutMode_(unsigned int cpsm, unsigned int shift, unsigned int mask, unsigned int a3)
{
   unsigned int argument = (cpsm) | (shift << 2) | (mask << 8) | (a3 << 16);
   sendCommandi(197,argument);
}

static inline int getExp_(int val)
{
   unsigned int i;
   asm("clz %0, %1\n":"=r"(i):"r"(val&0x3FF));
   return 31-i;
}

void inline sceGuTexImage_(int mipmap, int width, int height, int tbw, const void* tbp)
{
   sendCommandi(0xa0,((unsigned int)tbp) & 0xffffff);
   sendCommandi(0xa8,((((unsigned int)tbp) >> 8) & 0x0f0000)|tbw);
   sendCommandi(0xb8,(getExp_(height) << 8)|(getExp_(width)));

   //sceGuTexFlush
   sendCommandf(203,0.0f);
}

extern u8 msx[];

static void debug_put_char(int x, int y, u8 ch)
{
   int 	i,j, l;
   u8	*font;
   u32 *vram_ptr;
   u32 *vram;

   vram = GBA_FRAME_TEXTURE;
   vram += x;
   vram += (y * GBA_LINE_SIZE);

   font = &msx[ (int)ch * 8];
   for (i=l=0; i < 8; i++, l+= 8, font++)
   {
      vram_ptr  = vram;
      for (j=0; j < 8; j++)
      {
          if ((*font & (128 >> j)))
            *vram_ptr = 0xFFFFFFFF;
          else
             *vram_ptr = 0x00000000;

        vram_ptr++;
      }
      vram += GBA_LINE_SIZE;
   }
}

#include <stdarg.h>

static int debug_X=0;
static int debug_Y=0;

void debug_printf(const char *format, ...)
{
   va_list	opt;
   char     buff[2048], c;
   int		size, i, j;

   va_start(opt, format);
   size = vsnprintf( buff, (size_t) sizeof(buff), format, opt);

   for (i = 0; i < size; i++)
   {
      c = buff[i];
      switch (c)
      {
         case '\r':
                  debug_X = 0;
                  break;
         case '\n':
                  debug_X = 0;
                  debug_Y += 8;
                  if (debug_Y >= GBA_SCREEN_HEIGHT)
                     debug_Y = 0;
                  break;
         case '\t':
                  for (j = 0; j < 5; j++) {
                     debug_put_char( debug_X , debug_Y, ' ');
                     debug_X+=7;
                  }
                  break;
         default:
                  debug_put_char( debug_X, debug_Y, c);
                  debug_X+=7;
                  if (debug_X >= GBA_SCREEN_WIDTH)
                  {
                     debug_X = 0;
                     debug_Y+=8;
                     if (debug_Y >= GBA_SCREEN_HEIGHT)
                        debug_Y = 0;
                  }
      }
   }
}

void debug_setpos(int X, int Y)
{
   debug_X = X;
   debug_Y = Y;
}
#include <pspctrl.h>

unsigned int read_system_buttons(void);


#endif //VIDEO_GE_UTILS_H
