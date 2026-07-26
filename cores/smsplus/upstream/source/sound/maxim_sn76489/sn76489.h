
#ifndef SN76489_H_
#define SN76489_H_

#define MAX_SN76489     1

/*
    More testing is needed to find and confirm feedback patterns for
    SN76489 variants and compatible chips.
*/
enum feedback_patterns {
    FB_BBCMICRO =   0x8005, /* Texas Instruments TMS SN76489N (original) from BBC Micro computer */
    FB_SC3000   =   0x0006, /* Texas Instruments TMS SN76489AN (rev. A) from SC-3000H computer */
    FB_SEGAVDP  =   0x0009, /* SN76489 clone in Sega's VDP chips (315-5124, 315-5246, 315-5313, Game Gear) */
};

enum volume_modes {
    VOL_TRUNC   =   0,      /* Volume levels 13-15 are identical */
    VOL_FULL    =   1,      /* Volume levels 13-15 are unique */
};

enum boost_modes {
    BOOST_OFF   =   0,      /* Regular noise channel volume */
    BOOST_ON    =   1,      /* Doubled noise channel volume */
};

enum mute_values {
    MUTE_ALLOFF =   0,      /* All channels muted */
    MUTE_TONE1  =   1,      /* Tone 1 mute control */
    MUTE_TONE2  =   2,      /* Tone 2 mute control */
    MUTE_TONE3  =   4,      /* Tone 3 mute control */
    MUTE_NOISE  =   8,      /* Noise mute control */
    MUTE_ALLON  =   15,     /* All channels enabled */
};

typedef struct
{
    /* expose this for inspection/modification for channel muting */
    int32_t Mute;
    int32_t BoostNoise;
    int32_t VolumeArray;
    
    /* Variables */
    float Clock;
    float dClock;
    int32_t PSGStereo;
    int32_t NumClocksForSample;
    int32_t WhiteNoiseFeedback;
    
    /* PSG registers: */
    int32_t Registers[8];        /* Tone, vol x4 */
    int32_t LatchedRegister;
    int32_t NoiseShiftRegister;
    int32_t NoiseFreq;            /* Noise channel signal generator frequency */
    
    /* Output calculation variables */
    int32_t ToneFreqVals[4];      /* Frequency register values (counters) */
    int8_t ToneFreqPos[4];        /* Frequency channel flip-flops */
    int32_t Channels[4];          /* Value of each channel, before stereo is applied */
    /* It was using LONG_MIN in sn76489.c before but that requires a 64-bits variable so i changed it to INT_MIN instead. So far, i haven't noticed any regressions yet. - Gameblabla */
    int32_t IntermediatePos[4];   /* intermediate values used at boundaries between + and - */

} SN76489_Context;

/* Function prototypes */
void SN76489_Init(int32_t which, int32_t PSGClockValue, int32_t SamplingRate);
void SN76489_Reset(int32_t which);
void SN76489_Shutdown(void);
void SN76489_Config(int32_t which, int32_t mute, int32_t boost, int32_t volume, int32_t feedback);
void SN76489_SetContext(int32_t which, uint8_t *data);
void SN76489_GetContext(int32_t which, uint8_t *data);
uint8_t *SN76489_GetContextPtr(int32_t which);
uint32_t SN76489_GetContextSize(void);
void SN76489_Write(int32_t which, int32_t data);
void SN76489_GGStereoWrite(int32_t which, int32_t data);
void SN76489_Update(int32_t which, int16_t **buffer, int32_t length);

#endif /* SN76489_H_ */

