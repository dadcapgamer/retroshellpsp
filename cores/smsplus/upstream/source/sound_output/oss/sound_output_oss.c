/*
 * OSSv3/OSSv4 output sound code.
 * License : MIT
 * See docs/MIT_license.txt for more information.
*/

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include "smsplus.h"
#include "sound_output.h"
#include "shared.h"

static int32_t oss_audio_fd = -1;

void Sound_Init()
{
	uint32_t channels = 2;
#ifdef LSB_FIRST
	uint32_t format = AFMT_S16_LE;
#else
	uint32_t format = AFMT_S16_BE;
#endif
	int32_t err_ret;
	int32_t tmp;
	
	tmp = SOUND_FREQUENCY;
	
	oss_audio_fd = open("/dev/dsp", O_WRONLY 
#ifdef NONBLOCKING_AUDIO
	| O_NONBLOCK
#endif
	);
	if (oss_audio_fd < 0)
	{
		printf("Couldn't open /dev/dsp.\n");
		return;
	}
	
	err_ret = ioctl(oss_audio_fd, SNDCTL_DSP_SPEED,&tmp);
	if (err_ret == -1)
	{
		printf("Could not set sound frequency\n");
		return;
	}
	err_ret = ioctl(oss_audio_fd, SNDCTL_DSP_CHANNELS, &channels);
	if (err_ret == -1)
	{
		printf("Could not set channels\n");
		return;
	}
	err_ret = ioctl(oss_audio_fd, SNDCTL_DSP_SETFMT, &format);
	if (err_ret == -1)
	{
		printf("Could not set sound format\n");
		return;
	}
	
	return;
}

void Sound_Update(int16_t* sound_buffer, unsigned long len)
{
	if (!oss_audio_fd) return;
	write(oss_audio_fd, sound_buffer, len * 4);
}

void Sound_Close()
{
	if (oss_audio_fd >= 0)
	{
		close(oss_audio_fd);
		oss_audio_fd = -1;
	}
}


void Sound_Pause()
{
}

void Sound_Unpause()
{
}
