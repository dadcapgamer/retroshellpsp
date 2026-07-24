/**
 * RetroSuite Core API — host services table.
 *
 * The frontend hands this table to a core at init time. It is the ONLY way a
 * core may talk to the outside world: cores never call sce* functions, never
 * touch the display, and never allocate from the system heap directly. This
 * keeps cores portable, testable, and lets the frontend own the memory map.
 *
 * Pure C, no dependencies — this header (with rs_core_api.h) is the entire
 * contract between the frontend and an emulator core.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RS_HOST_API_VERSION 1u

/* Log levels accepted by RSHostAPI::log. */
enum {
    RS_LOG_DEBUG = 0,
    RS_LOG_INFO  = 1,
    RS_LOG_WARN  = 2,
    RS_LOG_ERROR = 3
};

/* Abstract gamepad bits passed to RSCoreAPI::run_frame and returned by
 * input_state. The frontend maps physical PSP buttons onto these according
 * to the user's per-core / per-game remapping. Modeled on a SNES-style pad,
 * which is a superset of every system RetroSuite targets. */
enum {
    RS_BTN_UP     = 1u << 0,
    RS_BTN_DOWN   = 1u << 1,
    RS_BTN_LEFT   = 1u << 2,
    RS_BTN_RIGHT  = 1u << 3,
    RS_BTN_A      = 1u << 4,   /* east  (NES/GB A, Genesis C)  */
    RS_BTN_B      = 1u << 5,   /* south (NES/GB B, Genesis B)  */
    RS_BTN_X      = 1u << 6,   /* north                        */
    RS_BTN_Y      = 1u << 7,   /* west  (Genesis A)            */
    RS_BTN_L      = 1u << 8,
    RS_BTN_R      = 1u << 9,
    RS_BTN_START  = 1u << 10,
    RS_BTN_SELECT = 1u << 11
};

typedef struct RSHostAPI {
    uint32_t api_version;              /* RS_HOST_API_VERSION */

    /* --- Logging ------------------------------------------------------- */
    void (*log)(int level, const char* fmt, ...);

    /* --- Memory -------------------------------------------------------- */
    /* Allocates from the arena the frontend released before launching the
     * core. `align` must be a power of two (0 means 16). Returns NULL on
     * exhaustion. All core memory is reclaimed wholesale on core unload;
     * mem_free exists for cores that recycle large buffers mid-session. */
    void* (*mem_alloc)(uint32_t size, uint32_t align);
    void  (*mem_free)(void* ptr);
    uint32_t (*mem_available)(void);

    /* --- Audio --------------------------------------------------------- */
    /* Declare the core's native output rate once (init or after load_rom),
     * then push interleaved stereo s16 frames during run_frame. The host
     * resamples to the PSP's 44100 Hz output. */
    void (*audio_set_rate)(uint32_t hz);
    void (*audio_push)(const int16_t* stereo, uint32_t frames);

    /* --- Input --------------------------------------------------------- */
    /* Current RS_BTN_* state; identical to the last run_frame argument.
     * Provided for cores that poll mid-frame. */
    uint32_t (*input_state)(void);

    /* --- Files --------------------------------------------------------- */
    /* Random-access reads for cores that stream data (e.g. large GBA ROMs
     * paged from the Memory Stick). Returns bytes read or < 0 on error. */
    int32_t (*file_size)(const char* path);
    int32_t (*file_read)(const char* path, void* buf, uint32_t offset,
                         uint32_t size);

    /* --- Options ------------------------------------------------------- */
    /* Resolved per-game/per-core setting, or NULL if unset. */
    const char* (*get_option)(const char* key);
} RSHostAPI;

#ifdef __cplusplus
}
#endif
