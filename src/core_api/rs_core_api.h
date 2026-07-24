/**
 * RetroSuite Core API — the emulator core contract.
 *
 * Every emulator core (PRX module or statically linked) exposes exactly one
 * entry point:
 *
 *     const RSCoreAPI* rs_get_core_api(void);
 *
 * returning a table that must stay valid for the lifetime of the module.
 * The boundary is pure C: PRX modules are built independently of the
 * frontend, and C++ vtables are not ABI-stable across separately compiled
 * modules. Frontend code wraps this table in the C++ `EmulatorCore` adapter
 * (src/frontend/emulator_core.h) so UI code never sees function pointers.
 *
 * Threading: all calls arrive on the frontend's main thread.
 */
#pragma once

#include "rs_host_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RS_CORE_API_VERSION 1u

/* Pixel formats a core may emit. Values match what the PSP GE consumes so
 * frames upload without conversion. */
enum {
    RS_PIXFMT_RGB565   = 0,   /* GU_PSM_5650 — preferred                 */
    RS_PIXFMT_RGBA5551 = 1,   /* GU_PSM_5551                             */
    RS_PIXFMT_RGBA8888 = 2    /* GU_PSM_8888 — costs 2x bandwidth        */
};

/* One video frame, borrowed from the core. Valid until the next run_frame
 * or unload_rom call. `pitch` is in BYTES. */
typedef struct RSVideoFrame {
    const void* pixels;
    uint16_t    width;
    uint16_t    height;
    uint16_t    pitch;
    uint8_t     format;      /* RS_PIXFMT_* */
    uint8_t     _pad;
} RSVideoFrame;

typedef struct RSCoreAPI {
    uint32_t api_version;    /* RS_CORE_API_VERSION — checked by the host */

    const char* name;        /* e.g. "gambatte"                           */
    const char* version;     /* upstream version string                   */
    const char* systems;     /* pipe-separated ids, e.g. "gb|gbc"         */
    const char* extensions;  /* pipe-separated, e.g. "gb|gbc|dmg"         */

    /* Target timing. The frontend paces emulation with these; 0 means
     * 60.0 fps / filled in after load_rom. */
    double fps;
    uint32_t audio_rate;     /* native audio rate, also set via host api  */

    /* --- Lifecycle ------------------------------------------------------ */
    /* `host` outlives the core. Returns 0 on success. */
    int  (*init)(const RSHostAPI* host);
    void (*shutdown)(void);

    /* `data` is the whole ROM image when the frontend could load it into
     * RAM, else NULL and the core streams via host file_read using `path`.
     * Returns 0 on success. */
    int  (*load_rom)(const char* path, const void* data, uint32_t size);
    void (*unload_rom)(void);
    void (*reset)(void);

    /* Advance exactly one video frame. `buttons` is an RS_BTN_* bitmask.
     * Audio for the frame is pushed through host->audio_push. */
    void (*run_frame)(uint32_t buttons);

    /* Frame produced by the last run_frame. */
    RSVideoFrame (*get_frame)(void);

    /* --- Save states ----------------------------------------------------- */
    uint32_t (*state_size)(void);                       /* upper bound      */
    int      (*state_save)(void* buf, uint32_t size);   /* ret bytes or <0  */
    int      (*state_load)(const void* buf, uint32_t size); /* 0 on success */

    /* --- Battery-backed SRAM --------------------------------------------- */
    /* Live pointer into the core's SRAM (or NULL). The frontend persists it
     * on pause/exit and restores it before reset. */
    uint32_t (*sram_size)(void);
    void*    (*sram_data)(void);
    /* Core must call this notification-free; the frontend polls a dirty
     * flag instead: returns nonzero if SRAM changed since the last call. */
    int      (*sram_dirty)(void);

    /* --- Options ---------------------------------------------------------- */
    /* Returns 0 if the option was recognized. */
    int (*set_option)(const char* key, const char* value);
} RSCoreAPI;

/* The single symbol a core module exports. */
typedef const RSCoreAPI* (*RSGetCoreApiFn)(void);

#define RS_CORE_ENTRY_SYMBOL "rs_get_core_api"

#ifdef __cplusplus
}
#endif
