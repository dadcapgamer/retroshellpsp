#ifndef SMSPLUS_H
#define SMSPLUS_H

#define SOUND_FREQUENCY 44100

#define VIDEO_WIDTH_SMS 256
#define VIDEO_HEIGHT_SMS 192
#define VIDEO_WIDTH_GG 160
#define VIDEO_HEIGHT_GG 144

#define LOCK_VIDEO
#define UNLOCK_VIDEO

typedef struct {
	char gamename[256];
	char biosdir[512];
} gamedata_t;

#endif
