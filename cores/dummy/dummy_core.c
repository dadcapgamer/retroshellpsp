/**
 * RetroSuite dummy core — validates the entire Core API pipeline before any
 * real emulator is integrated.
 *
 * "Emulates" a 240x160 machine whose game is a scrolling test pattern:
 *   - video: animated color bands + a bouncing block (exercises RGB565
 *     frames and per-frame texture upload)
 *   - audio: a triangle-wave tone at 32768 Hz (exercises the resampler);
 *     pitch rises while any face button is held (exercises input)
 *   - input: pressed buttons light up indicator blocks along the bottom
 *   - state: scroll phase / block position serialize (exercises save states)
 *   - SRAM: a boot counter incremented on every load_rom (exercises battery
 *     saves surviving relaunches)
 *
 * Pure C, no libc beyond string.h/stdint — everything else via RSHostAPI.
 */
#include "core_api/rs_core_api.h"

#include <string.h>

#ifndef RS_STATIC_BUILD
#include <pspkernel.h>
PSP_MODULE_INFO("rs_core_dummy", 0, 1, 0);
#endif

#define FB_W 240
#define FB_H 160
#define AUDIO_RATE 32768u

static const RSHostAPI* g_host;
static uint16_t* g_fb;
static uint8_t*  g_sram;
static uint32_t  g_frame;
static int  g_blockX = 40, g_blockY = 30, g_blockDX = 2, g_blockDY = 1;
static uint32_t g_buttons;
static int32_t  g_tonePhase;
static uint32_t g_sramDirty;

#define SRAM_SIZE 8192u

/* ------------------------------------------------------------------ */

static uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3));
}

static int core_init(const RSHostAPI* host) {
    g_host = host;
    g_host->log(RS_LOG_INFO, "dummy core init, %u KB arena available",
                (unsigned)(g_host->mem_available() / 1024));
    return 0;
}

static void core_shutdown(void) {
    g_host->log(RS_LOG_INFO, "dummy core shutdown");
    g_fb = 0;
    g_sram = 0;
}

static int core_load_rom(const char* path, const void* data, uint32_t size) {
    g_fb = (uint16_t*)g_host->mem_alloc(FB_W * FB_H * 2, 16);
    g_sram = (uint8_t*)g_host->mem_alloc(SRAM_SIZE, 16);
    if (!g_fb || !g_sram) return -1;
    memset(g_sram, 0, SRAM_SIZE);

    /* Boot counter lives in SRAM; the frontend restores the previous SRAM
     * after load_rom, so bumping here marks it dirty for the next flush. */
    g_sram[0]++;
    g_sramDirty = 1;

    g_frame = 0;
    g_host->audio_set_rate(AUDIO_RATE);
    g_host->log(RS_LOG_INFO, "dummy: rom '%s' (%u bytes, %s)",
                path ? path : "?", (unsigned)size,
                data ? "in memory" : "streamed");
    return 0;
}

static void core_unload_rom(void) {
    g_fb = 0;
    g_sram = 0;
}

static void core_reset(void) {
    g_frame = 0;
    g_blockX = 40;
    g_blockY = 30;
    g_host->log(RS_LOG_INFO, "dummy: reset");
}

static void draw_pattern(void) {
    const uint32_t t = g_frame;
    for (int y = 0; y < FB_H; y++) {
        uint16_t* row = g_fb + y * FB_W;
        for (int x = 0; x < FB_W; x++) {
            const int band = ((x + (int)t) >> 4) & 7;
            const int base = 40 + ((y * 120) / FB_H);
            switch (band) {
                case 0: row[x] = rgb565(base + 60, 40, 60); break;
                case 1: row[x] = rgb565(60, base + 60, 60); break;
                case 2: row[x] = rgb565(60, 60, base + 80); break;
                case 3: row[x] = rgb565(base + 60, base + 60, 40); break;
                case 4: row[x] = rgb565(40, base + 50, base + 70); break;
                case 5: row[x] = rgb565(base + 70, 40, base + 70); break;
                case 6: row[x] = rgb565(base + 40, base + 40, base + 40); break;
                default: row[x] = rgb565(30, 34, 44); break;
            }
        }
    }

    /* Bouncing block. */
    g_blockX += g_blockDX;
    g_blockY += g_blockDY;
    if (g_blockX < 0 || g_blockX > FB_W - 24) g_blockDX = -g_blockDX;
    if (g_blockY < 8 || g_blockY > FB_H - 40) g_blockDY = -g_blockDY;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 24; x++) {
            const int px = g_blockX + x, py = g_blockY + y;
            if (px >= 0 && px < FB_W && py >= 0 && py < FB_H)
                g_fb[py * FB_W + px] = rgb565(255, 255, 255);
        }

    /* Button indicator blocks along the bottom. */
    static const uint32_t BTNS[8] = {
        RS_BTN_UP, RS_BTN_DOWN, RS_BTN_LEFT, RS_BTN_RIGHT,
        RS_BTN_A, RS_BTN_B, RS_BTN_START, RS_BTN_SELECT
    };
    for (int i = 0; i < 8; i++) {
        const int on = (g_buttons & BTNS[i]) != 0;
        const uint16_t c = on ? rgb565(120, 255, 140) : rgb565(30, 40, 50);
        for (int y = FB_H - 14; y < FB_H - 4; y++)
            for (int x = 0; x < 20; x++)
                g_fb[y * FB_W + (8 + i * 28 + x)] = c;
    }
}

static void run_audio_frame(void) {
    /* One video frame of triangle wave. Held face buttons raise pitch. */
    int16_t buf[(AUDIO_RATE / 60 + 4) * 2];
    const uint32_t frames = AUDIO_RATE / 60;
    int32_t step = 440 * 4;   /* triangle period units */
    if (g_buttons & (RS_BTN_A | RS_BTN_B)) step = 660 * 4;
    for (uint32_t i = 0; i < frames; i++) {
        g_tonePhase += step;
        int32_t v = (g_tonePhase >> 3) & 0x1FFF;
        if (v > 0x0FFF) v = 0x1FFF - v;
        const int16_t s = (int16_t)((v - 0x800) << 2);
        buf[i * 2 + 0] = s;
        buf[i * 2 + 1] = s;
    }
    g_host->audio_push(buf, frames);
}

static void core_run_frame(uint32_t buttons) {
    g_buttons = buttons;
    if (!g_fb) return;
    g_frame++;
    draw_pattern();
    run_audio_frame();
}

static RSVideoFrame core_get_frame(void) {
    RSVideoFrame f;
    f.pixels = g_fb;
    f.width  = FB_W;
    f.height = FB_H;
    f.pitch  = FB_W * 2;
    f.format = RS_PIXFMT_RGB565;
    f._pad   = 0;
    return f;
}

/* --- state ---------------------------------------------------------- */

typedef struct {
    uint32_t frame;
    int32_t blockX, blockY, blockDX, blockDY;
    int32_t tonePhase;
} DummyState;

static uint32_t core_state_size(void) { return sizeof(DummyState); }

static int core_state_save(void* buf, uint32_t size) {
    if (size < sizeof(DummyState)) return -1;
    DummyState st;
    st.frame = g_frame;
    st.blockX = g_blockX;
    st.blockY = g_blockY;
    st.blockDX = g_blockDX;
    st.blockDY = g_blockDY;
    st.tonePhase = g_tonePhase;
    memcpy(buf, &st, sizeof st);
    return (int)sizeof st;
}

static int core_state_load(const void* buf, uint32_t size) {
    if (size < sizeof(DummyState)) return -1;
    DummyState st;
    memcpy(&st, buf, sizeof st);
    g_frame = st.frame;
    g_blockX = st.blockX;
    g_blockY = st.blockY;
    g_blockDX = st.blockDX;
    g_blockDY = st.blockDY;
    g_tonePhase = st.tonePhase;
    return 0;
}

static uint32_t core_sram_size(void) { return g_sram ? SRAM_SIZE : 0; }
static void*    core_sram_data(void) { return g_sram; }

static int core_sram_dirty(void) {
    const int d = (int)g_sramDirty;
    g_sramDirty = 0;
    return d;
}

static int core_set_option(const char* key, const char* value) {
    g_host->log(RS_LOG_DEBUG, "dummy: option %s=%s", key, value);
    return 0;
}

/* ------------------------------------------------------------------ */

static const RSCoreAPI g_api = {
    RS_CORE_API_VERSION,
    "dummy",
    "1.0",
    "gb|gbc|gba|nes|sfc|md|sms|gg|pce",
    "*",
    60.0,
    AUDIO_RATE,
    core_init,
    core_shutdown,
    core_load_rom,
    core_unload_rom,
    core_reset,
    core_run_frame,
    core_get_frame,
    core_state_size,
    core_state_save,
    core_state_load,
    core_sram_size,
    core_sram_data,
    core_sram_dirty,
    core_set_option,
};

const RSCoreAPI* rs_get_core_api(void) { return &g_api; }

#ifndef RS_STATIC_BUILD
/* PRX entry: the frontend passes the ADDRESS of a pointer slot as the
 * start argument; we publish our API table there (no export lookup). */
int module_start(SceSize args, void* argp) {
    if (args >= sizeof(void*) && argp) {
        const RSCoreAPI** slot = *(const RSCoreAPI***)argp;
        if (slot) *slot = &g_api;
    }
    return 0;
}

int module_stop(SceSize args, void* argp) {
    (void)args;
    (void)argp;
    return 0;
}
#endif
