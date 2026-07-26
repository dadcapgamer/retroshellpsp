/*
    CrabEmu Modifications to this code:
      - Added typedefs and such to unglue the code from MAME's core.
      - Added mono update function for the Dreamcast port.
      - Updated to mamedev current as of September 4, 2016, backporting to C
        from C++.

    Copyright (C) 2011, 2012, 2016 Lawrence Sebald

    Modifications distributed under the same license as the rest of the code in
    this file (GPLv2 or newer).
*/

/* license:GPL-2.0+
 * copyright-holders:Jarek Burczynski,Ernesto Corvi 
 */
#ifndef YM2413_H__
#define YM2413_H__


#include <stdint.h>
/* End MAME glue... */

struct OPLL_SLOT
{
    int32_t  ar;         /* attack rate: AR<<2           */
    int32_t  dr;         /* decay rate:  DR<<2           */
    int32_t  rr;         /* release rate:RR<<2           */
    int32_t   ksl;        /* keyscale level               */
    int32_t   ksr;        /* key scale rate: kcode>>KSR   */
    
    uint32_t   KSR;        /* key scale rate               */
    uint32_t   mul;        /* multiple: mul_tab[ML]        */

    /* Phase Generator */
    int32_t  phase;      /* frequency counter            */
    uint32_t  freq;       /* frequency counter step       */
	int32_t   fb_shift;   /* feedback shift value         */
    int32_t   op1_out[2]; /* slot1 output for feedback    */

    /* Envelope Generator */
    uint32_t   eg_type;    /* percussive/nonpercussive mode*/
    uint32_t   state;      /* phase type                   */
    int32_t  TL;         /* total level: TL << 2         */
    int32_t   TLL;        /* adjusted now TL              */
    int32_t   volume;     /* envelope counter             */
    int32_t  sl;         /* sustain level: sl_tab[SL]    */

    uint32_t   eg_sh_dp;   /* (dump state)                 */
    uint32_t   eg_sel_dp;  /* (dump state)                 */
    uint32_t   eg_sh_ar;   /* (attack state)               */
    uint32_t   eg_sel_ar;  /* (attack state)               */
    uint32_t   eg_sh_dr;   /* (decay state)                */
    uint32_t   eg_sel_dr;  /* (decay state)                */
    uint32_t   eg_sh_rr;   /* (release state for non-perc.)*/
    uint32_t   eg_sel_rr;  /* (release state for non-perc.)*/
    uint32_t   eg_sh_rs;   /* (release state for perc.mode)*/
    uint32_t   eg_sel_rs;  /* (release state for perc.mode)*/

    int32_t  key;        /* 0 = KEY OFF, >0 = KEY ON     */

    /* LFO */
    int32_t  AMmask;     /* LFO Amplitude Modulation enable mask */
    uint32_t   vib;        /* LFO Phase Modulation enable flag (active high)*/

    /* waveform select */
    int32_t wavetable;
};

struct OPLL_CH
{
    struct OPLL_SLOT SLOT[2];
    /* phase generator state */
    int32_t  block_fnum; /* block+fnum                   */
    uint32_t  fc;         /* Freq. freqement base         */
    int32_t  ksl_base;   /* KeyScaleLevel Base step      */
    uint8_t   kcode;      /* key code (for key scaling)   */
    uint8_t   sus;        /* sus on/off (release speed in percussive mode)*/
};

enum {
    RATE_STEPS = (8),

    /* sinwave entries */
    SIN_BITS =       10,
    SIN_LEN  =       (1<<SIN_BITS),
    SIN_MASK =       (SIN_LEN-1),

    TL_RES_LEN =     (256),   /* 8 bits addressing (real chip) */

    /*  TL_TAB_LEN is calculated as:
     *   11 - sinus amplitude bits     (Y axis)
     *   2  - sinus sign bit           (Y axis)
     *   TL_RES_LEN - sinus resolution (X axis)
     */
    TL_TAB_LEN = (11*2*TL_RES_LEN),

    LFO_AM_TAB_ELEMENTS = 210

};

typedef struct ym2413_s {
	int32_t tl_tab[TL_TAB_LEN];

	/* sin waveform table in 'decibel' scale */
	/* two waveforms on OPLL type chips */
	int32_t sin_tab[SIN_LEN * 2];

	int32_t   instvol_r[9];           /* instrument/volume (or volume/volume in percussive mode)*/

	int32_t  eg_cnt;                 /* global envelope generator counter    */
	int32_t  eg_timer;               /* global envelope generator counter works at frequency = chipclock/72 */
	int32_t  eg_timer_add;           /* step of eg_timer                     */
	int32_t  eg_timer_overflow;      /* envelope generator timer overlfows every 1 sample (on real chip) */

	uint32_t   rhythm;                 /* Rhythm mode                  */

	/* LFO */
	int32_t  LFO_AM;
	int32_t   LFO_PM;
	uint32_t  lfo_am_cnt;
	uint32_t  lfo_am_inc;
	int32_t  lfo_pm_cnt;
	int32_t  lfo_pm_inc;

	int32_t  noise_rng;              /* 23 bit noise shift register  */
	int32_t  noise_p;                /* current noise 'phase'        */
	int32_t  noise_f;                /* current noise period         */

/* instrument settings */
/*
    0-user instrument
    1-15 - fixed instruments
    16 -bass drum settings
    17,18 - other percussion instruments
*/
	uint32_t  fn_tab[1024];           /* fnumber->increment counter   */

	uint8_t address;                  /* address register             */
	
	uint8_t inst_tab[19][8];

	int16_t output[2];
	
	struct OPLL_CH P_CH[9];         /* OPLL chips have 9 channels */
} YM2413;

/* CrabEmu's public interface... */
YM2413 *ym2413_init(int32_t clock, int32_t rate);
void ym2413_shutdown(YM2413 *fm);
void ym2413_reset(YM2413 *fm);
void ym2413_write(YM2413 *fm, int32_t a, uint8_t v);
void ym2413_update(YM2413 *fm, int16_t **buf, int32_t samples);
unsigned char ym2413_read(YM2413 *fm, int32_t a);

#endif /*YM2413_H__*/
