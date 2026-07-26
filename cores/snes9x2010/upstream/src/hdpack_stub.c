#include "hdpack.h"

int S9xHdPackInit(const char *rom_path) {
   (void)rom_path;
   return 0;
}

void S9xHdPackDeinit(void) {}
int S9xHdPackActive(void) { return 0; }
int S9xHdPackRecording(void) { return 0; }
uint32_t S9xHdPackScale(void) { return 1; }
void S9xHdPackFrameBegin(void) {}
void S9xHdPackWrapRenderers(int normal1x1) { (void)normal1x1; }

uint16_t *S9xHdPackComposite(int width, int height, int *out_width,
      int *out_height, int *out_pitch) {
   (void)width;
   (void)height;
   (void)out_width;
   (void)out_height;
   (void)out_pitch;
   return 0;
}
