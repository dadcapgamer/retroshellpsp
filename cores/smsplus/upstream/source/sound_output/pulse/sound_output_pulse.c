/*
 * Pulseaudio output sound code.
 * License : MIT
 * See docs/MIT_license.txt for more information.
*/

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <pulse/simple.h>
#include "smsplus.h"
#include "sound_output.h"
#include "shared.h"

static pa_simple *pulse_stream;

void Sound_Init()
{
	pa_sample_spec ss;
	pa_buffer_attr paattr;
	
	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = SOUND_FREQUENCY;

	paattr.tlength = snd.buffer_size * 4;
	paattr.prebuf = -1;
	paattr.maxlength = -1;
	paattr.minreq = snd.buffer_size;

	/* Create a new playback stream */
	pulse_stream = pa_simple_new(NULL, "SMS Plus GX", PA_STREAM_PLAYBACK, NULL, "SMS Plus GX", &ss, NULL, &paattr, NULL);
	if (!pulse_stream)
	{
		fprintf(stderr, "PulseAudio: pa_simple_new() failed!\n");
	}
	return;
}

void Sound_Update(int16_t* sound_buffer, unsigned long len)
{
	if (pa_simple_write(pulse_stream, sound_buffer, len * 4, NULL) < 0)
	{
		fprintf(stderr, "PulseAudio: pa_simple_write() failed!\n");
	}
}

void Sound_Close()
{
	if(pulse_stream != NULL)
	{
		if (pa_simple_drain(pulse_stream, NULL) < 0) 
		{
			fprintf(stderr, "PulseAudio: pa_simple_drain() failed!\n");
		}
		pa_simple_free(pulse_stream);
	}
}

void Sound_Pause()
{
}

void Sound_Unpause()
{
}

