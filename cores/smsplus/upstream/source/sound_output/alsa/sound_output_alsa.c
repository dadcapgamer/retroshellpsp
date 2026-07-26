/*
 * Alsa output sound code.
 * License : MIT
 * See docs/MIT_license.txt for more information.
*/

#include <stdio.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include "smsplus.h"
#include "sound_output.h"
#include "shared.h"

static snd_pcm_t *handle;

void Sound_Init(void)
{
	snd_pcm_hw_params_t *params;
	uint32_t val;
	int32_t dir = -1;
	snd_pcm_uframes_t frames;
	
	/* Open PCM device for playback. */
	int32_t rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

#ifdef ZIPIT_PORT
	if (rc < 0)
		rc = snd_pcm_open(&handle, "plughw:Z2,0,0", SND_PCM_STREAM_PLAYBACK, 0);
#endif

	if (rc < 0)
		rc = snd_pcm_open(&handle, "plughw:0,0,0", SND_PCM_STREAM_PLAYBACK, 0);

	if (rc < 0)
		rc = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
		
	if (rc < 0)
		rc = snd_pcm_open(&handle, "plughw:1,0,0", SND_PCM_STREAM_PLAYBACK, 0);

	if (rc < 0)
		rc = snd_pcm_open(&handle, "plughw:1,0", SND_PCM_STREAM_PLAYBACK, 0);

	if (rc < 0)
	{
		fprintf(stderr, "unable to open PCM device: %s\n", snd_strerror(rc));
		return;
	}

#ifdef NONBLOCKING_AUDIO
	snd_pcm_nonblock(handle, 1);
#else
	snd_pcm_nonblock(handle, 0);
#endif

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	rc = snd_pcm_hw_params_any(handle, params);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_any %s\n", snd_strerror(rc));
		return;
	}

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	rc = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_access %s\n", snd_strerror(rc));
		return;
	}

	/* Signed 16-bit little-endian format */
	rc = snd_pcm_hw_params_set_format(handle, params,
	#ifdef LSB_FIRST
	SND_PCM_FORMAT_S16_LE
	#else
	SND_PCM_FORMAT_S16_BE
	#endif
	);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_format %s\n", snd_strerror(rc));
		return;
	}

	/* Two channels (stereo) */
	rc = snd_pcm_hw_params_set_channels(handle, params, 2);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_channels %s\n", snd_strerror(rc));
		return;
	}
	
	val = snd.sample_rate;
	rc=snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_rate_near %s\n", snd_strerror(rc));
		return;
	}

	/* Set period size to settings.aica.BufferSize frames. */
	frames = snd.buffer_size;
	rc = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_buffer_size_near %s\n", snd_strerror(rc));
		return;
	}
	frames *= 4;
	rc = snd_pcm_hw_params_set_buffer_size_near(handle, params, &frames);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_buffer_size_near %s\n", snd_strerror(rc));
		return;
	}

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0)
	{
		fprintf(stderr, "Unable to set hw parameters: %s\n", snd_strerror(rc));
		return;
	}

	return;
}

void Sound_Update(int16_t* sound_buffer, unsigned long len)
{
	long ret;
	ret = snd_pcm_writei(handle, sound_buffer, len);
	while(ret != len) 
	{
		if (ret < 0) snd_pcm_prepare( handle );
		else len -= ret;
		ret = snd_pcm_writei(handle, sound_buffer, len);
	}
}

void Sound_Close(void)
{
	if (handle)
	{
		snd_pcm_drop(handle);
		snd_pcm_close(handle);
		snd_config_update_free_global();
	}
}

void Sound_Pause()
{
	Sound_Close();
}

void Sound_Unpause()
{
	Sound_Init();
}
