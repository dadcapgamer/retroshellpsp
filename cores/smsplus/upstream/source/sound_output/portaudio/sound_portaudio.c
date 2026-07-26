/*
 * Portaudio output sound code.
 * License : MIT
 * See docs/MIT_license.txt for more information.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <portaudio.h>
#include "smsplus.h"
#include "sound_output.h"
#include "shared.h"

PaStream *apu_stream;

#ifdef NONBLOCKING_AUDIO
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    /* Cast data passed through stream to our structure. */
    snd.output = (int16_t*)outputBuffer;
    int32_t i;
    (void) inputBuffer; /* Prevent unused variable warning. */
	
    return 0;
}
#endif

void Sound_Init()
{
	int32_t err;
	err = Pa_Initialize();
	
	PaStreamParameters outputParameters;

	outputParameters.device = Pa_GetDefaultOutputDevice();
	
	if (outputParameters.device == paNoDevice) 
	{
		printf("No sound output\n");
		return;
	}

	outputParameters.channelCount = 2;
	outputParameters.sampleFormat = paInt16;
	//outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	
	err = Pa_OpenStream( &apu_stream, NULL, &outputParameters, SOUND_FREQUENCY, snd.buffer_size, paNoFlag,
	#ifdef NONBLOCKING_AUDIO
	patestCallback, NULL);
	#else
	NULL, NULL);
	#endif
	err = Pa_StartStream( apu_stream );
}

void Sound_Update(int16_t* sound_buffer, unsigned long len)
{
	#ifndef NONBLOCKING_AUDIO
	Pa_WriteStream( apu_stream, sound_buffer, len);
	#endif
}

void Sound_Close()
{
	int32_t err;
	err = Pa_CloseStream( apu_stream );
	err = Pa_Terminate();
}

void Sound_Pause()
{
}

void Sound_Unpause()
{
}
