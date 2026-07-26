/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additionnal code by Eke-Eke (SMS Plus GX)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   Sound emulation.
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * July 16th 2019 : Remove extra if condition.
 * June 6th 2019 : Correctly set the PSG parameters for SG-1000 and Colecovision.
 * March 13th 2019 : Partial revert due to CrabZ80. The switching to C99 datatypes had been done again.
 * March 7th 2019 : Minor changes to sound.c. (Some trimming down, more switching to C99 datatypes and more.
 * February 2nd 2019 : Change the names of some sound functions.
 * October 12th 2019 : Switching some variables in functions to c99 datatypes.
*/
#include "shared.h"
#include "config.h"

snd_t snd;
static int16_t **fm_buffer;
static int16_t **psg_buffer;
static int32_t *smptab;
static int32_t smptab_len;

#ifndef MAXIM_PSG
sn76489_t psg_sn;
#endif

#ifdef MAME_PSG
static uint8_t machine_psg = 0;
#endif

uint32_t SMSPLUS_sound_init(void)
{
	static uint8_t *fmbuf = NULL;
	static uint8_t *psgbuf = NULL;
	int32_t restore_sound = 0;
	int32_t i;

	snd.fm_which = option.fm;
	snd.fps = (sms.display == DISPLAY_NTSC) ? FPS_NTSC : FPS_PAL;
	snd.fm_clock = (sms.display == DISPLAY_NTSC) ? CLOCK_NTSC : CLOCK_PAL;
	snd.psg_clock = (sms.display == DISPLAY_NTSC) ? CLOCK_NTSC : CLOCK_PAL;
	snd.sample_rate = SOUND_FREQUENCY;
	snd.mixer_callback = NULL;
	
	/* Save register settings */
	if(snd.enabled)
	{
		restore_sound = 1;
		#if defined(MAME_PSG)
		psgbuf = malloc(sizeof(PSG));
		if (!psgbuf) return 0;
		memcpy(psgbuf, &PSG, sizeof(PSG));
		#elif defined(MAXIM_PSG)
		psgbuf = malloc(SN76489_GetContextSize ());
		if (!psgbuf) return 0;
		memcpy (psgbuf, SN76489_GetContextPtr (0),SN76489_GetContextSize ());
		#else
		psgbuf = malloc(sizeof(sn76489_t));
		if (!psgbuf) return 0;
		memcpy (&psg_sn, psgbuf, sizeof(sn76489_t));
		#endif
		fmbuf = malloc(FM_GetContextSize());
		if (!fmbuf) return 0;
		FM_GetContext(fmbuf);
		
		/* If we are reinitializing, shut down sound emulation */
		SMSPLUS_sound_shutdown();
	}
	
	/* Disable sound until initialization is complete */
	snd.enabled = 0;

	/* Check if sample rate is invalid */
	if(snd.sample_rate < 8000 || snd.sample_rate > 48000)
		return 0;

	/* Assign stream mixing callback if none provided */
	if(!snd.mixer_callback)
		snd.mixer_callback = SMSPLUS_sound_mixer_callback;

	/* Calculate number of samples generated per frame */
	snd.sample_count = (snd.sample_rate / snd.fps);

	/* Calculate size of sample buffer */
	snd.buffer_size = snd.sample_count * 2;
	
	/* Free sample buffer position table if previously allocated */
	if(smptab)
	{
		free(smptab);
		smptab = NULL;
	}

	/* Prepare incremental info */
	snd.done_so_far = 0;
	smptab_len = (sms.display == DISPLAY_NTSC) ? 262 : 313;
	smptab = malloc(smptab_len * sizeof(int32_t));
	
	if(!smptab)
	{
		printf("Failed to malloc smptab\n");
		return 0;
	}
	
	for (i = 0; i < smptab_len; i++)
	{
		float calc = (snd.sample_count * i);
		calc = calc / (float)smptab_len;
		smptab[i] = (int32_t)calc;
	}

	/* Allocate emulated sound streams */
	for(i = 0; i < STREAM_MAX; i++)
	{
		snd.stream[i] = malloc(snd.buffer_size);
		if(!snd.stream[i]) return 0;
		memset(snd.stream[i], 0, snd.buffer_size);
	}

	/* Allocate sound output streams */
	snd.output = malloc(snd.buffer_size*2);
	
	if(!snd.output)
		return 0;

	/* Set up buffer pointers */
	fm_buffer = (int16_t **)&snd.stream[STREAM_FM_MO];
	psg_buffer = (int16_t **)&snd.stream[STREAM_PSG_L];

	/* Set up SN76489 emulation */
	#ifdef MAME_PSG
	switch(sms.console)
	{
		default:
			machine_psg = 0;
		break;
		case CONSOLE_GG:
		case CONSOLE_GGMS:
			machine_psg = 1;
		break;
		case CONSOLE_COLECO:
			machine_psg = 2;
		break;
		case CONSOLE_SG1000:
		case CONSOLE_SC3000:
		case CONSOLE_SF7000:
			machine_psg = 3;
		break;
	}
	SN76489_Init(machine_psg, snd.psg_clock, snd.sample_rate);
	#elif defined(MAXIM_PSG)
    SN76489_Init(0, snd.psg_clock, snd.sample_rate);
    SN76489_Config(0, MUTE_ALLON, BOOST_ON, VOL_FULL, (sms.console < CONSOLE_SMS) ? FB_SC3000 : FB_SEGAVDP);
    #else
	sn76489_init(&psg_sn, (float)snd.psg_clock, (float)snd.sample_rate, 
	(sms.console < CONSOLE_SMS) ? SN76489_NOISE_BITS_SG1000 : SN76489_NOISE_BITS_SMS, 
	(sms.console < CONSOLE_SMS) ? SN76489_NOISE_TAPPED_SG1000 : SN76489_NOISE_TAPPED_SMS);
	#endif

	/* Set up YM2413 emulation */
	FM_Init();

	/* Restore YM2413 register settings */
	if(restore_sound)
	{
		#if defined(MAME_PSG)
		memcpy (&PSG, psgbuf, sizeof(PSG));
		#elif defined(MAXIM_PSG)
		memcpy (SN76489_GetContextPtr (0),psgbuf,SN76489_GetContextSize ());
		#else
		memcpy (&psg_sn, psgbuf, sizeof(sn76489_t));
		#endif
		FM_SetContext(fmbuf);
		free(fmbuf);
		free(psgbuf);
	}

	/* Inform other functions that we can use sound */
	snd.enabled = 1;
	
	return 1;
}


void SMSPLUS_sound_shutdown(void)
{
	uint32_t i;
	
	if(!snd.enabled)
		return;

	/* Free emulated sound streams */
	for(i = 0; i < STREAM_MAX; i++)
	{
		if(snd.stream[i])
		{
			free(snd.stream[i]);
			snd.stream[i] = NULL;
		}
	}

	/* Free sound output buffers */
	if(snd.output)
	{
		free(snd.output);
		snd.output = NULL;
	}
	
	/* Free sample buffer position table if previously allocated */
	if (smptab)
	{
		free(smptab);
		smptab = NULL;
	}
	
	/* Shut down SN76489 emulation */
    #ifdef MAXIM_PSG
    SN76489_Shutdown();
    #endif
	/* Shut down YM2413 emulation */
	FM_Shutdown();
}


void SMSPLUS_sound_reset(void)
{
	if(!snd.enabled)
		return;

	/* Reset SN76489 emulator */
	#if defined(MAME_PSG)
	SN76489_Init(machine_psg, snd.psg_clock, snd.sample_rate);
	#elif defined(MAXIM_PSG)
	SN76489_Reset(0);
	#else
	sn76489_reset(&psg_sn, (float)snd.psg_clock, (float)snd.sample_rate,
	(sms.console < CONSOLE_SMS) ? SN76489_NOISE_BITS_SG1000 : SN76489_NOISE_BITS_SMS, 
	(sms.console < CONSOLE_SMS) ? SN76489_NOISE_TAPPED_SG1000 : SN76489_NOISE_TAPPED_SMS);
	#endif
	
	/* Reset YM2413 emulator */
	FM_Reset();
}


void SMSPLUS_sound_update(int32_t line)
{
	int16_t *fm[2], *psg[2];

	/*if(!snd.enabled)
		return;*/

	/* Finish buffers at end of frame */
	if(line == smptab_len - 1)
	{
		psg[0] = psg_buffer[0] + snd.done_so_far;
		psg[1] = psg_buffer[1] + snd.done_so_far;
		fm[0]  = fm_buffer[0] + snd.done_so_far;
		fm[1]  = fm_buffer[1] + snd.done_so_far;

		/* Generate SN76489 sample data */
		#if defined(MAME_PSG)
		SN76489_Update(psg, snd.sample_count - snd.done_so_far);
		#elif defined(MAXIM_PSG)
		SN76489_Update(0, psg, snd.sample_count - snd.done_so_far);
		#else
		sn76489_execute_samples(&psg_sn, psg[0], psg[1], snd.sample_count - snd.done_so_far);
		#endif

		/* Generate YM2413 sample data */
		FM_Update(fm, snd.sample_count - snd.done_so_far);

		/* Mix streams into output buffer */
		snd.mixer_callback(snd.output, snd.sample_count);
		/* Reset */
		snd.done_so_far = 0;
	}
	else
	{
		int32_t tinybit;
		tinybit = smptab[line] - snd.done_so_far;

		/* Do a tiny bit */
		psg[0] = psg_buffer[0] + snd.done_so_far;
		psg[1] = psg_buffer[1] + snd.done_so_far;
		fm[0]  = fm_buffer[0] + snd.done_so_far;
		fm[1]  = fm_buffer[1] + snd.done_so_far;

		/* Generate SN76489 sample data */
		#if defined(MAME_PSG)
		SN76489_Update(psg, tinybit);
		#elif defined(MAXIM_PSG)
		SN76489_Update(0, psg, tinybit);
		#else
		sn76489_execute_samples(&psg_sn, psg[0], psg[1], tinybit);
		#endif

		/* Generate YM2413 sample data */
		FM_Update(fm, tinybit);

		/* Sum total */
		snd.done_so_far += tinybit;
	}
}

/* Generic FM+PSG stereo mixer callback */
void SMSPLUS_sound_mixer_callback(int16_t *output, int32_t length)
{
	int32_t i;
	for(i = 0; i < length; i++)
	{
		int16_t temp = (fm_buffer[0][i] + fm_buffer[1][i]) / 2;
		output[i * 2] = (temp + psg_buffer[0][i]) * option.soundlevel;
		output[i * 2 + 1] = (temp + psg_buffer[1][i]) * option.soundlevel;
	}
}

/*--------------------------------------------------------------------------*/
/* Sound chip access handlers                                               */
/*--------------------------------------------------------------------------*/

void psg_stereo_w(int32_t data)
{
	/*if(!snd.enabled) return;*/
	#if defined(MAME_PSG)
	SN76489_GGStereoWrite(data);
	#elif defined(MAXIM_PSG)
	SN76489_GGStereoWrite(0, data);
	#else
	sn76489_set_output_channels(&psg_sn, data);
	#endif
}


void psg_write(int32_t data)
{
	/*if(!snd.enabled) return;*/
	#if defined(MAME_PSG)
	SN76489_Write(data);
	#elif defined(MAXIM_PSG)
	SN76489_Write(0, data);
	#else
	sn76489_write(&psg_sn, data);
	#endif
}

/*--------------------------------------------------------------------------*/
/* Mark III FM Unit / Master System (J) built-in FM handlers                */
/*--------------------------------------------------------------------------*/

uint32_t fmunit_detect_r(void)
{
	return sms.fm_detect;
}

void fmunit_detect_w(uint32_t data)
{
	if(/* !snd.enabled || */ !sms.use_fm) return;
	sms.fm_detect = data;
}

void fmunit_write(uint32_t offset, uint8_t data)
{
	if(/* !snd.enabled || */ !sms.use_fm) return;
	FM_Write(offset, data);
}
