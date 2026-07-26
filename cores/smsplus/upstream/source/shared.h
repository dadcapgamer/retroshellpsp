#ifndef SHARED_H_
#define SHARED_H_

/* Convenience stuff... */
#undef INLINE
#if __STDC_VERSION__ >= 199901L
#    define INLINE static inline
#elif defined(__GNUC__) || defined(__GNUG__)
#    define INLINE static __inline__
#else
#    define INLINE static
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#ifndef NGC
#ifndef PATH_MAX
#ifdef  MAX_PATH
#define PATH_MAX    MAX_PATH
#else
#define PATH_MAX    1024
#endif
#endif
#endif

#include "z80.h"
#include "sms.h"
#include "pio.h"
#include "memz80.h"
#include "vdp.h"
#include "render.h"
#include "tms.h"
#include "sn76489.h"
#include "ym2413.h"
#include "fmintf.h"
#include "sound.h"
#include "system.h"
#include "loadrom.h"
#include "config.h"
#include "state.h"
#include "z80_wrap.h"
#include "sound_output.h"

/* For lock and unlocking screen surface */
#include "smsplus.h"

#ifndef NOZIP_SUPPORT
#include "miniz.h"
#include "fileio.h"
#include "unzip.h"
#endif

#ifdef SCALE2X_UPSCALER
#include "scale2x.h"
#endif

#endif /* _SHARED_H_ */
