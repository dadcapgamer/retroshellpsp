TARGET      := tempgba_libretro_psp1.a

CC  = psp-gcc
AR  = psp-ar

HW_RENDER_TEST = 0
PERF_TEST      = 0

ifeq ($(DEBUG), 1)
OPTIMIZE	      := -O0 -g
OPTIMIZE_SAFE  := -O0 -g
else
OPTIMIZE	      := -O3
OPTIMIZE_SAFE  := -O2
endif

CFLAGS   += -G0

CFLAGS   += -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi
CFLAGS   += -fomit-frame-pointer -fstrict-aliasing
CFLAGS   += -falign-functions=32 -falign-loops -falign-labels -falign-jumps
CFLAGS   += -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Wsign-compare
#CFLAGS   += -Werror

ASFLAGS = $(CFLAGS)

CFLAGS  += -D__LIBRETRO__ -DPSP

CFLAGS  += -D_PSP_FW_VERSION=371

OBJS := mips_stub.o
OBJS += cpu.o
OBJS += video.o

ifeq ($(HW_RENDER_TEST), 1)
CFLAGS  += -DHW_RENDER_TEST
OBJS += video_ge.o
endif

ifeq ($(PERF_TEST), 1)
CFLAGS  += -DPERF_TEST
endif

#OBJS += griffin.o
OBJS += libretro.o
OBJS += input.o
OBJS += main.o
OBJS += memory.o
##OBJS += sound.o
OBJS += sound_alt.o
OBJS += zip.o



INCDIRS := -I.
INCDIRS += -I$(shell psp-config --pspsdk-path)/include
#INCDIRS += -I$(shell psp-config --psp-prefix)/include

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(OPTIMIZE) $(INCDIRS)

cpu.o: cpu.c
	$(CC) -c -o $@ $< $(CFLAGS) $(OPTIMIZE_SAFE) $(INCDIRS)

%.o: %.S
	$(CC) -c -o $@ $< $(ASFLAGS) $(OPTIMIZE)

video_ge.o: video_ge_utils.h

clean:
	rm -f libretro.o input.o main.o memory.o sound.o sound_alt.o video_ge.o griffin.o zip.o
#	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: $(TARGET) griffin.c clean
