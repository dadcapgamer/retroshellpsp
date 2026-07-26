#ifndef SCALER_H
#define SCALER_H

#include <stdint.h>

/* Generic */
extern void bitmap_scale(uint32_t startx, uint32_t starty, uint32_t viswidth, uint32_t visheight, uint32_t newwidth, uint32_t newheight,uint32_t pitchsrc,uint32_t pitchdest, uint16_t* restrict src, uint16_t* restrict dst);

/* OpenDingux-like devices (320x240) */
extern void upscale_160x144_to_320x240(uint32_t* restrict dst, uint32_t* restrict src);
extern void upscale_SMS_to_320x240(uint32_t* restrict dst, uint32_t* restrict src, uint32_t height_scale);

/* Arcade Mini/PAP K3 Plus/PSP-Like scalers */
extern void upscale_160x144_to_320x272_for_480x272(uint32_t* restrict dst, uint32_t* restrict src);
extern void upscale_160x144_to_480x272(uint32_t* restrict dst, uint32_t* restrict src);
extern void upscale_256xXXX_to_384x272_for_480x272(uint32_t* restrict dst, uint32_t* restrict src, uint32_t height);
extern void upscale_256xXXX_to_480x272(uint32_t* restrict dst, uint32_t* restrict src, uint32_t height);

/* PAP Gameta II scalers */
void upscale_160x144_to_320x320_for_480x320(uint32_t* restrict dst, uint32_t* restrict src);
void upscale_160x144_to_480x320(uint32_t* restrict dst, uint32_t* restrict src);
void upscale_256xXXX_to_384x320_for_480x320(uint32_t* restrict dst, uint32_t* restrict src, uint32_t height);
void upscale_256xXXX_to_480x320(uint32_t* restrict dst, uint32_t* restrict src, uint32_t height);

// RS-90 Scaler
//for GG
void upscale_160x144_to_212x160(uint16_t* restrict src, uint16_t* restrict dst);
void upscale_160x144_to_212x144(uint16_t* restrict src, uint16_t* restrict dst);
void upscale_160x144_to_240x160(uint16_t* restrict src, uint16_t* restrict dst);
//for SMS
void downscale_240x192to240x160(uint32_t* restrict src, uint32_t* restrict dst);

#endif
