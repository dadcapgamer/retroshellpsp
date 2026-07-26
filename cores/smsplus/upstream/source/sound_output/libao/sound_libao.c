/*
 * libao output sound code.
 * License : MIT
 * See docs/MIT_license.txt for more information.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ao/ao.h>
#include "smsplus.h"
#include "sound_output.h"
#include "shared.h"

static ao_device *aodevice;
static ao_sample_format aoformat;

void Sound_Init()
{
	ao_initialize();
	memset(&aoformat, 0, sizeof(aoformat));
	
	aoformat.bits = 16;
	aoformat.channels = 2;
	aoformat.rate = SOUND_FREQUENCY;
	aoformat.byte_format = AO_FMT_LITTLE;
	
	aodevice = ao_open_live(ao_default_driver_id(), &aoformat, NULL); // Live output
	if (!aodevice)
		aodevice = ao_open_live(ao_driver_id("null"), &aoformat, NULL);
}

void Sound_Update(int16_t* sound_buffer, unsigned long len)
{
	ao_play(aodevice, (char*)sound_buffer, len);
}

void Sound_Close()
{
	if (aodevice)
	{
		ao_close(aodevice);
		ao_shutdown();
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
