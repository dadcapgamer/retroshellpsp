#ifndef SOUND_OUTPUT_H
#define SOUND_OUTPUT_H

#include <stdint.h>

extern void Sound_Init(void);
/* extern void Sound_Update(void); */
extern void Sound_Update(int16_t* sound_buffer, unsigned long len);
extern void Sound_Close(void);
extern void Sound_Pause(void);
extern void Sound_Unpause(void);

#endif
