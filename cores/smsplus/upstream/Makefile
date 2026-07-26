PRGNAME     = sms.elf
CC			= gcc

# Possible choices : rs97, k3s (PAP K3S), sdl, amini, fbdev
PORT = sdl
# Possible choices : alsa, pulse (pulseaudio), oss, sdl12 (SDL 1.2 sound output), portaudio, libao
SOUND_OUTPUT = pulse
# Possible choices : crabemu_sn76489 (less accurate, GPLv2), maxim_sn76489 (somewhat problematic license but good accuracy)
SOUND_ENGINE = maxim_sn76489
# Possible choices : z80 (accurate but proprietary), eighty (EightyZ80's core, GPLv2)
Z80_CORE = z80
SCALE2X_UPSCALER = 1
PROFILE = 0
ZIP_SUPPORT = 1

SRCDIR		= ./source ./source/cpu_cores/$(Z80_CORE) ./source/sound ./source/unzip
SRCDIR		+= ./source/scalers ./source/ports/$(PORT) ./source/sound/$(SOUND_ENGINE) ./source/sound_output/$(SOUND_OUTPUT)
VPATH		= $(SRCDIR)
SRC_C		= $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
SRC_CP		= $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.cpp))
OBJ_C		= $(notdir $(patsubst %.c, %.o, $(SRC_C)))
OBJ_CP		= $(notdir $(patsubst %.cpp, %.o, $(SRC_CP)))
OBJS		= $(OBJ_C) $(OBJ_CP)

CFLAGS		= -O0 -g3 -Wall -Wextra
CFLAGS		+= -DLSB_FIRST -std=gnu99 -DALIGN_DWORD -DNOYUV
CFLAGS		+= -Isource -Isource/cpu_cores/$(Z80_CORE) -Isource/scalers -Isource/ports/$(PORT) -I./source/sound -Isource/unzip -Isource/sdl -Isource/sound/$(SOUND_ENGINE) -Isource/sound_output

SRCDIR		+= ./source/text/fb
CFLAGS		+= -Isource/text/fb

ifeq ($(PROFILE), YES)
CFLAGS 		+= -fprofile-generate=./
else ifeq ($(PROFILE), APPLY)
CFLAGS		+= -fprofile-use -fbranch-probabilities
endif

ifeq ($(SOUND_ENGINE), maxim_sn76489)
CFLAGS 		+= -DMAXIM_PSG
endif

ifeq ($(ZIP_SUPPORT), 0)
CFLAGS 		+= -DNOZIP_SUPPORT
endif

ifeq ($(SCALE2X_UPSCALER), 1)
CFLAGS 		+= -DSCALE2X_UPSCALER
CFLAGS		+= -Isource/scale2x
SRCDIR		+= ./source/scale2x
endif

LDFLAGS     = -nodefaultlibs -lc -lgcc -lm -lSDL -Wl,--as-needed -Wl,--gc-sections -flto

ifeq ($(SOUND_OUTPUT), portaudio)
LDFLAGS		+= -lportaudio
endif
ifeq ($(SOUND_OUTPUT), libao)
LDFLAGS		+= -lao
endif
ifeq ($(SOUND_OUTPUT), alsa)
LDFLAGS		+= -lasound
endif
ifeq ($(SOUND_OUTPUT), pulse)
LDFLAGS		+= -lpulse -lpulse-simple
endif

# Rules to make executable
$(PRGNAME): $(OBJS)  
	$(CC) $(CFLAGS) -o $(PRGNAME) $^ $(LDFLAGS)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PRGNAME) *.o
