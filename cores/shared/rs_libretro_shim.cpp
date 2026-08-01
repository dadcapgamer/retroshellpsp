/* RSCoreAPI ↔ libretro bridge. See rs_libretro_shim.h for the contract.
 *
 * One instance of this file is compiled into each libretro-based core
 * module; `RS_CORE_NAME` and `RS_CORE_SYSTEMS` identify the core. All
 * state is static because a module hosts exactly one core.
 */
#include "core_api/rs_core_api.h"

#include "libretro.h"

#include <csetjmp>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(RS_PSP_NATIVE_PIXELS) || defined(RS_PSP_NATIVE_RGB565)
#define RS_PSP_NATIVE_VIDEO
#include <pspkernel.h>
#endif

#ifndef RS_CORE_NAME
#error "core CMakeLists must define RS_CORE_NAME"
#endif
#ifndef RS_CORE_SYSTEMS
#error "core CMakeLists must define RS_CORE_SYSTEMS"
#endif

#ifdef RS_PSP_SECRET_OF_MANA_HIRES_RESOLVE
extern "C" bool rs_psp_hires_prefer_blue(void);
#endif

namespace {

const RSHostAPI* g_host;
uint32_t g_buttons;
bool g_videoEnabled = true;

/* Converted, PSP-native copy of the core's latest frame. Native-video cores
 * already render in the GE's channel ordering, so their ordinary frames are
 * borrowed directly and this allocation is omitted. */
uint16_t* g_frame;             /* also holds 8888 data (cast) */
uint32_t  g_frameCap;          /* capacity in bytes */
RSVideoFrame g_frameInfo;

retro_pixel_format g_srcFormat = RETRO_PIXEL_FORMAT_0RGB1555;
retro_audio_buffer_status_callback_t g_audioBufferStatus;

/* The active ROM, remembered so the shim can answer both the classic
 * retro_game_info path and the newer GET_GAME_INFO_EXT query — modern
 * cores (e.g. FCEUmm) fetch the ROM buffer only via the EXT interface and
 * fall back to broken file loading if it isn't answered. */
const void* g_romData = nullptr;
uint32_t    g_romSize = 0;
char        g_romPath[256];

char g_version[32] = "?";

uint32_t g_sramHash;

/* Core fault recovery. A libretro core that hits abort()/std::terminate
 * would otherwise take down the frontend's main thread (see abort() in the
 * PRX runtime section). Instead, calls into core code that can fault are
 * bracketed with setjmp; a fault longjmps back here, latches g_faulted so
 * the broken core stops running, and lets the frontend unload it and
 * return to the menu — the isolation the API promises. */
std::jmp_buf g_faultJmp;
bool         g_faultArmed;
bool         g_faulted;
bool         g_gameLoaded;

constexpr uint32_t MAX_GEOMETRY = 1024;
constexpr uint32_t MAX_STATE_BYTES = 16u * 1024u * 1024u;

struct RSVfsFile {
    char path[256];
    int64_t size;
    int64_t pos;
};

bool safeVfsPath(const char* path) {
    if (!path || !*path || std::strlen(path) >= 256 ||
        std::strstr(path, ".."))
        return false;
    return std::strncmp(path, "ms0:/ROMS/", 10) == 0 ||
           std::strncmp(path, "ms0:/RETROSHELL/system/", 23) == 0;
}

const char* vfsGetPath(retro_vfs_file_handle* stream) {
    auto* f = reinterpret_cast<RSVfsFile*>(stream);
    return f ? f->path : nullptr;
}

retro_vfs_file_handle* vfsOpen(const char* path, unsigned mode, unsigned) {
    if (!safeVfsPath(path) || !(mode & RETRO_VFS_FILE_ACCESS_READ) ||
        (mode & RETRO_VFS_FILE_ACCESS_WRITE))
        return nullptr;
    const int32_t size = g_host->file_size(path);
    if (size < 0) return nullptr;
    auto* f = static_cast<RSVfsFile*>(malloc(sizeof(RSVfsFile)));
    if (!f) return nullptr;
    std::snprintf(f->path, sizeof f->path, "%s", path);
    f->size = size;
    f->pos = 0;
    return reinterpret_cast<retro_vfs_file_handle*>(f);
}

int vfsClose(retro_vfs_file_handle* stream) {
    free(reinterpret_cast<RSVfsFile*>(stream));
    return 0;
}

int64_t vfsSize(retro_vfs_file_handle* stream) {
    auto* f = reinterpret_cast<RSVfsFile*>(stream);
    return f ? f->size : -1;
}

int64_t vfsTell(retro_vfs_file_handle* stream) {
    auto* f = reinterpret_cast<RSVfsFile*>(stream);
    return f ? f->pos : -1;
}

int64_t vfsSeek(retro_vfs_file_handle* stream, int64_t offset, int whence) {
    auto* f = reinterpret_cast<RSVfsFile*>(stream);
    if (!f) return -1;
    int64_t base = 0;
    if (whence == RETRO_VFS_SEEK_POSITION_CURRENT) base = f->pos;
    else if (whence == RETRO_VFS_SEEK_POSITION_END) base = f->size;
    else if (whence != RETRO_VFS_SEEK_POSITION_START) return -1;
    if ((offset > 0 && base > INT64_MAX - offset) ||
        offset == INT64_MIN ||
        (offset < 0 && base < -offset))
        return -1;
    const int64_t next = base + offset;
    if (next < 0 || next > f->size || next > UINT32_MAX) return -1;
    f->pos = next;
    return next;
}

int64_t vfsRead(retro_vfs_file_handle* stream, void* dst, uint64_t len) {
    auto* f = reinterpret_cast<RSVfsFile*>(stream);
    if (!f || !dst || f->pos < 0 || f->pos > f->size) return -1;
    const uint64_t left = uint64_t(f->size - f->pos);
    if (len > left) len = left;
    if (len > UINT32_MAX) len = UINT32_MAX;
    const int32_t got =
        g_host->file_read(f->path, dst, uint32_t(f->pos), uint32_t(len));
    if (got < 0) return -1;
    f->pos += got;
    return got;
}

int64_t vfsTruncate(retro_vfs_file_handle*, int64_t) { return -1; }
int64_t vfsWrite(retro_vfs_file_handle*, const void*, uint64_t) { return -1; }
int vfsFlush(retro_vfs_file_handle*) { return -1; }
int vfsRemove(const char*) { return -1; }
int vfsRename(const char*, const char*) { return -1; }

retro_vfs_interface g_vfs = {
    vfsGetPath, vfsOpen, vfsClose, vfsSize, vfsTell, vfsSeek, vfsRead,
    vfsWrite, vfsFlush, vfsRemove, vfsRename, vfsTruncate
};

#ifndef RS_PSP_NATIVE_VIDEO
/* Maps a libretro pixel format to the matching RS_PIXFMT_*. */
uint8_t rsPixFmt(retro_pixel_format f) {
    switch (f) {
        case RETRO_PIXEL_FORMAT_RGB565:   return RS_PIXFMT_RGB565;
        case RETRO_PIXEL_FORMAT_XRGB8888: return RS_PIXFMT_RGBA8888;
        default:                          return RS_PIXFMT_RGBA5551;
    }
}

/* ------------------------------------------------------------------ */
/* Pixel conversion: libretro packs red in the high bits, the PSP GE   */
/* wants it in the low bits, so every format needs an R/B swap.        */
/* ------------------------------------------------------------------ */

void convertRGB565(const uint8_t* src, unsigned w, unsigned h, size_t pitch) {
    for (unsigned y = 0; y < h; y++) {
        const uint16_t* in = reinterpret_cast<const uint16_t*>(src + y * pitch);
        uint16_t* out = g_frame + y * w;
        for (unsigned x = 0; x < w; x++) {
            const uint16_t v = in[x];
            out[x] = uint16_t((v >> 11) | (v & 0x07E0) | (v << 11));
        }
    }
    g_frameInfo.format = RS_PIXFMT_RGB565;
}

void convert1555(const uint8_t* src, unsigned w, unsigned h, size_t pitch) {
    for (unsigned y = 0; y < h; y++) {
        const uint16_t* in = reinterpret_cast<const uint16_t*>(src + y * pitch);
        uint16_t* out = g_frame + y * w;
        for (unsigned x = 0; x < w; x++) {
            const uint16_t v = in[x];
            out[x] = uint16_t(0x8000 | ((v >> 10) & 0x001F) | (v & 0x03E0) |
                              ((v & 0x001F) << 10));
        }
    }
    g_frameInfo.format = RS_PIXFMT_RGBA5551;
}

void convertXRGB8888(const uint8_t* src, unsigned w, unsigned h, size_t pitch) {
    for (unsigned y = 0; y < h; y++) {
        const uint32_t* in = reinterpret_cast<const uint32_t*>(src + y * pitch);
        uint32_t* out = reinterpret_cast<uint32_t*>(g_frame) + y * w;
        for (unsigned x = 0; x < w; x++) {
            const uint32_t v = in[x];
            out[x] = 0xFF000000u | (v & 0x0000FF00u) | ((v >> 16) & 0xFFu) |
                     ((v & 0xFFu) << 16);
        }
    }
    g_frameInfo.format = RS_PIXFMT_RGBA8888;
}
#endif

/* ------------------------------------------------------------------ */
/* libretro callbacks                                                  */
/* ------------------------------------------------------------------ */

void logPrintf(retro_log_level level, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    /* retro_log_level values match RS_LOG_* by design of both enums. */
    g_host->log(int(level), "%s", buf);
}

bool environment(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const auto fmt = *static_cast<const retro_pixel_format*>(data);
            if (fmt != RETRO_PIXEL_FORMAT_0RGB1555 &&
                fmt != RETRO_PIXEL_FORMAT_RGB565 &&
                fmt != RETRO_PIXEL_FORMAT_XRGB8888)
                return false;
            g_srcFormat = fmt;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            /* Audio always runs. The frontend may suppress video on a
             * recovery frame so a 30 Hz PSP presentation can carry 60 Hz
             * game logic without irregular automatic frameskip bursts. */
            *static_cast<int*>(data) = (g_videoEnabled ? 1 : 0) | 2;
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            static_cast<retro_log_callback*>(data)->log = logPrintf;
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *static_cast<const char**>(data) = "ms0:/RETROSHELL/system";
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            /* SRAM is persisted by the frontend through the API table, but
             * some cores insist on a directory existing. */
            *static_cast<const char**>(data) = "ms0:/RETROSHELL/saves";
            return true;
        case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
            if (!data) {
                g_audioBufferStatus = nullptr;
                return true;
            }
            const auto* status =
                static_cast<const retro_audio_buffer_status_callback*>(data);
            g_audioBufferStatus = status->callback;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
            /* The PSP output ring already provides about 186 ms at 44.1 kHz,
             * above the SNES cores' requested latency. */
            return true;
#ifdef RETRO_ENVIRONMENT_GET_VFS_INTERFACE
        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            auto* info = static_cast<retro_vfs_interface_info*>(data);
            if (!info || info->required_interface_version > 2) return false;
            info->required_interface_version = 2;
            info->iface = &g_vfs;
            return true;
        }
#endif
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            auto* var = static_cast<retro_variable*>(data);
            var->value = g_host->get_option(var->key);
#ifdef RS_GPSP_PSP_DEFAULTS
            /* gpSP's desktop-oriented defaults mix at 65.5 kHz and never
             * skip video when audio is close to starvation. On a 333 MHz
             * PSP, 32.8 kHz preserves the GBA's practical audio bandwidth
             * while halving mixer work. Auto frameskip is only activated
             * for an imminent underrun reported by the host, so ordinary
             * full-speed play still renders every frame. Per-game options
             * always take precedence over these platform defaults. */
            if (!var->value &&
                std::strcmp(var->key, "gpsp_sound_rate") == 0) {
                var->value = "32768";
                g_host->log(RS_LOG_INFO,
                            "PSP default: gpsp_sound_rate=32768");
            } else if (!var->value &&
                       std::strcmp(var->key, "gpsp_frameskip") == 0) {
                var->value = "auto";
                g_host->log(RS_LOG_INFO,
                            "PSP default: gpsp_frameskip=auto");
            }
#endif
#ifdef RS_PICODRIVE_PSP_DEFAULTS
            /* The upstream standalone PSP port mixes at 32 kHz-class rates.
             * Keeping the libretro desktop default of 44.1 kHz adds FM/PSG
             * work and host resampling while real PSP Genesis workloads are
             * already CPU-bound. This supported core option remains
             * overridable by a per-game setting. */
            if (!var->value &&
                std::strcmp(var->key, "picodrive_sound_rate") == 0) {
                var->value = "32000";
                g_host->log(RS_LOG_INFO,
                            "PSP default: picodrive_sound_rate=32000");
            }
#endif
            return var->value != nullptr;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = false;
            return true;
#ifdef RETRO_ENVIRONMENT_GET_GAME_INFO_EXT
        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT: {
            /* Hand the core the in-RAM ROM buffer via the extended-info
             * struct. Without this, modern cores never see `data` and try
             * to read the file themselves, which the bare PRX can't do. */
            static retro_game_info_ext ext;
            static char dirBuf[256], nameBuf[128], extBuf[16];
            const char* slash = std::strrchr(g_romPath, '/');
            const char* base = slash ? slash + 1 : g_romPath;
            const char* dot = std::strrchr(base, '.');
            std::snprintf(dirBuf, sizeof dirBuf, "%.*s",
                          slash ? int(slash - g_romPath) : 0, g_romPath);
            std::snprintf(nameBuf, sizeof nameBuf, "%.*s",
                          dot ? int(dot - base) : int(std::strlen(base)), base);
            std::snprintf(extBuf, sizeof extBuf, "%s", dot ? dot + 1 : "");
            ext = {};
            ext.full_path = g_romPath;
            ext.dir  = dirBuf;
            ext.name = nameBuf;
            ext.ext  = extBuf;
            ext.data = g_romData;
            ext.size = g_romSize;
            ext.file_in_archive = false;
            ext.persistent_data = g_romData != nullptr;
            *static_cast<const retro_game_info_ext**>(data) = &ext;
            return true;
        }
#endif
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            *static_cast<unsigned*>(data) = 2;
            return true;
        /* Option/descriptor registration is metadata we don't render yet;
         * acknowledging it keeps cores on their happy path. The V2 cases
         * are #ifdef'd because cores ship different libretro.h vintages —
         * an unknown value simply falls through to default and returns
         * false, which is a valid "not handled" answer. */
        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
#endif
#ifdef RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
#endif
            return true;
        default:
            return false;
    }
}

void videoRefresh(const void* data, unsigned w, unsigned h, size_t pitch) {
#ifdef RS_SHIM_DIAG
    /* One-shot video diagnostic: report the first ~90 frames' worth of
     * calls so we can see whether a "black/blank" core is actually
     * delivering frames, duping (NULL), or sending odd geometry. */
    static int s_diagCalls = 0, s_diagNull = 0;
    s_diagCalls++;
    if (!data) s_diagNull++;
    if (s_diagCalls == 300) {   /* ~5s in, past a game's black boot */
        uint32_t nonzero = 0;
        if (data) {
            const uint8_t* p = static_cast<const uint8_t*>(data);
            for (unsigned y = 0; y < h; y++) {
                const uint16_t* row = reinterpret_cast<const uint16_t*>(p + y * pitch);
                for (unsigned x = 0; x < w; x++)
                    if (row[x]) nonzero++;
            }
        }
        g_host->log(RS_LOG_INFO,
                    "shim DIAG: %d calls, %d null | %ux%u pitch=%u fmt=%d | "
                    "nonblack px this frame: %u / %u",
                    s_diagCalls, s_diagNull, w, h, unsigned(pitch),
                    int(g_srcFormat), nonzero, w * h);
    }
#endif
    if (!data) return;   /* NULL = duped frame, keep the last */

    const unsigned bpp = (g_srcFormat == RETRO_PIXEL_FORMAT_XRGB8888) ? 4 : 2;
    const uint64_t bytes = uint64_t(w) * uint64_t(h) * bpp;
    if (!w || !h || w > MAX_GEOMETRY || h > MAX_GEOMETRY ||
        pitch < size_t(w) * bpp || bytes > g_frameCap ||
        uint64_t(w) * bpp > UINT16_MAX || pitch > UINT16_MAX) {
        /* Larger than the load-time allocation — a core that grew its
         * geometry at runtime (which the shim doesn't support). Skip the
         * frame loudly rather than overrun the buffer. */
        g_host->log(RS_LOG_WARN, "libretro shim: %ux%u frame exceeds capacity",
                    w, h);
        return;
    }

#ifdef RS_PSP_NATIVE_PIXELS
    /* Snes9x's PSP renderer writes BGR555, the native channel ordering for
     * GU_PSM_5551. Keep the core isolated from sceGu while avoiding the old
     * full-frame R/B swap. The core owns this storage and guarantees it
     * through the next run.
     *
     * The GE does not snoop Allegrex's data cache. Upstream's native PSP
     * path explicitly writes this range back before binding it as a texture;
     * the isolated callback must do the same. Without it, cache-line eviction
     * determines which freshly rendered tiles reach the display, producing
     * partially stale effects such as corrupted Secret of Mana window fills. */
    if (w == 512 && g_frame) {
        /* SNES pseudo-hires transparency interleaves main- and sub-screen
         * pixels. Sending all 512 columns to the 480-wide PSP display creates
         * severe vertical moire instead of the intended blended window.
         * Resolve each adjacent BGR555 pair into one format-correct pixel.
         * This is also the appropriate PSP tradeoff for genuine hi-res modes:
         * their native width exceeds the physical display. */
        const bool preferBlue =
#ifdef RS_PSP_SECRET_OF_MANA_HIRES_RESOLVE
            rs_psp_hires_prefer_blue();
#else
            false;
#endif
        for (unsigned y = 0; y < h; y++) {
            const uint16_t* src = reinterpret_cast<const uint16_t*>(
                static_cast<const uint8_t*>(data) + size_t(y) * pitch);
            uint16_t* dst = g_frame + size_t(y) * 256u;
            for (unsigned x = 0; x < 256; x++) {
                const uint16_t a = src[x * 2u];
                const uint16_t b = src[x * 2u + 1u];
                /* Secret of Mana's hi-res dialogue alternates its intended
                 * blue/text sample with a sub-screen sample. TYL resolves
                 * those screens in separate GU passes. On the clean US ROM,
                 * choose the member with the larger BGR555 blue component:
                 * dark-blue fill and white glyphs win over the room/black
                 * sample. Other titles retain the format-correct average. */
                if (preferBlue && ((a ^ b) & 0x7FFFu))
                    dst[x] = ((a >> 10) & 31u) >= ((b >> 10) & 31u) ? a : b;
                else
                    dst[x] = uint16_t(
                        (((a & 0x7BDEu) + (b & 0x7BDEu)) >> 1) +
                        (a & b & 0x0421u));
            }
        }
        const size_t resolvedBytes = size_t(256u) * h * sizeof(uint16_t);
        sceKernelDcacheWritebackRange(g_frame, resolvedBytes);
        g_frameInfo.pixels = g_frame;
        g_frameInfo.width  = 256;
        g_frameInfo.pitch  = 256 * sizeof(uint16_t);
    } else {
        sceKernelDcacheWritebackRange(
            const_cast<void*>(data), size_t(pitch) * size_t(h));
        g_frameInfo.pixels = data;
        g_frameInfo.width  = uint16_t(w);
        g_frameInfo.pitch  = uint16_t(pitch);
    }
    g_frameInfo.height = uint16_t(h);
    g_frameInfo.format = RS_PIXFMT_RGBA5551;
    g_frameInfo.storage_height = RS_PSP_NATIVE_STORAGE_HEIGHT;
    g_frameInfo.sequence++;
    return;
#elif defined(RS_PSP_NATIVE_RGB565)
    /* PicoDrive's PSP renderer can emit the GE's native BGR565 ordering.
     * Borrow it through drawFrame(), avoiding a 320x240 per-pixel R/B swap.
     * Its 320-pixel stride is not a power of two, so the frontend performs
     * one bounded texture upload before the GE samples the frame. */
    g_frameInfo.pixels = data;
    g_frameInfo.width = uint16_t(w);
    g_frameInfo.height = uint16_t(h);
    g_frameInfo.pitch = uint16_t(pitch);
    g_frameInfo.storage_height = uint16_t(h);
    g_frameInfo.format = RS_PIXFMT_RGB565;
    g_frameInfo.sequence++;
    return;
#else
    if (!g_frame) return;
    const auto* src = static_cast<const uint8_t*>(data);
    switch (g_srcFormat) {
        case RETRO_PIXEL_FORMAT_RGB565:   convertRGB565(src, w, h, pitch); break;
        case RETRO_PIXEL_FORMAT_XRGB8888: convertXRGB8888(src, w, h, pitch); break;
        default:                          convert1555(src, w, h, pitch); break;
    }
    g_frameInfo.pixels = g_frame;
    g_frameInfo.width  = uint16_t(w);
    g_frameInfo.height = uint16_t(h);
    g_frameInfo.pitch  = uint16_t(w * bpp);
    g_frameInfo.storage_height = uint16_t(h);
    g_frameInfo.sequence++;
#endif
}

size_t audioBatch(const int16_t* data, size_t frames) {
    if (!data || frames > UINT32_MAX) return 0;
    g_host->audio_push(data, uint32_t(frames));
    return frames;
}

void audioSample(int16_t left, int16_t right) {
    const int16_t pair[2] = {left, right};
    g_host->audio_push(pair, 1);
}

void inputPoll() {}

int16_t inputState(unsigned port, unsigned device, unsigned, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;

    /* RS_BTN_* deliberately mirrors the SNES-style retro pad. */
    static const struct { unsigned retro; uint32_t rs; } MAP[] = {
        {RETRO_DEVICE_ID_JOYPAD_UP, RS_BTN_UP},
        {RETRO_DEVICE_ID_JOYPAD_DOWN, RS_BTN_DOWN},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, RS_BTN_LEFT},
        {RETRO_DEVICE_ID_JOYPAD_RIGHT, RS_BTN_RIGHT},
        {RETRO_DEVICE_ID_JOYPAD_A, RS_BTN_A},
        {RETRO_DEVICE_ID_JOYPAD_B, RS_BTN_B},
        {RETRO_DEVICE_ID_JOYPAD_X, RS_BTN_X},
        {RETRO_DEVICE_ID_JOYPAD_Y, RS_BTN_Y},
        {RETRO_DEVICE_ID_JOYPAD_L, RS_BTN_L},
        {RETRO_DEVICE_ID_JOYPAD_R, RS_BTN_R},
        {RETRO_DEVICE_ID_JOYPAD_START, RS_BTN_START},
        {RETRO_DEVICE_ID_JOYPAD_SELECT, RS_BTN_SELECT},
    };

    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        int16_t mask = 0;
        for (const auto& m : MAP)
            if (g_buttons & m.rs) mask |= int16_t(1u << m.retro);
        return mask;
    }
    for (const auto& m : MAP)
        if (m.retro == id) return (g_buttons & m.rs) ? 1 : 0;
    return 0;
}

uint32_t adler32(const uint8_t* p, uint32_t n) {
    uint32_t a = 1, b = 0;
    for (uint32_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

}  // namespace

/* ------------------------------------------------------------------ */
/* RSCoreAPI implementation                                            */
/* ------------------------------------------------------------------ */

#ifndef RS_STATIC_BUILD
static void runStaticCtors();
#endif

static int coreInit(const RSHostAPI* host) {
    /* api_version is the first field of RSHostAPI and never moves, so it is
     * safe to read even if the rest of the table was reordered by a newer
     * host. Refuse a mismatch instead of calling through wrong slots. */
    if (!host || host->api_version != RS_HOST_API_VERSION) return -1;
    g_host = host;
    g_audioBufferStatus = nullptr;

    /* Guard construction: static ctors and retro_init may fault, and a
     * fault here must unwind to the host, not kill its main thread. */
    g_faultArmed = true;
    if (setjmp(g_faultJmp) != 0) {
        g_faultArmed = false;
        g_host->log(RS_LOG_ERROR, "libretro shim: core faulted during init");
        return -1;
    }
#ifndef RS_STATIC_BUILD
    /* Deferred from module_start: constructors may allocate, and the
     * arena-backed heap below needs g_host first. */
    runStaticCtors();
#endif
    retro_set_environment(environment);
    retro_set_video_refresh(videoRefresh);
    retro_set_audio_sample(audioSample);
    retro_set_audio_sample_batch(audioBatch);
    retro_set_input_poll(inputPoll);
    retro_set_input_state(inputState);
    retro_init();
    g_faultArmed = false;

    retro_system_info info = {};
    retro_get_system_info(&info);
    std::snprintf(g_version, sizeof g_version, "%s",
                  info.library_version ? info.library_version : "?");
    g_host->log(RS_LOG_INFO, "libretro shim: %s %s",
                info.library_name ? info.library_name : RS_CORE_NAME,
                g_version);
    return 0;
}

static void coreShutdown(void) {
    /* Guarded: deinit on an already-corrupt core (e.g. after a fault) can
     * fault again; unwind rather than kill the host thread. */
    g_faultArmed = true;
    if (setjmp(g_faultJmp) == 0) retro_deinit();
    g_faultArmed = false;
    g_frame = nullptr;
    g_frameCap = 0;
    g_gameLoaded = false;
    g_audioBufferStatus = nullptr;
    g_host = nullptr;
}

static void coreReset(void) {
    /* Reset is a recovery action, so clear the fault latch: a successful
     * reset un-freezes a core that faulted earlier. */
    g_faulted = false;
    g_faultArmed = true;
    if (setjmp(g_faultJmp) != 0) g_faulted = true;   /* reset itself faulted */
    else retro_reset();
    g_faultArmed = false;
}

static void coreRunFrame(uint32_t buttons) {
    if (g_faulted) return;   /* frozen after a fault; user exits via menu */
    g_videoEnabled = (buttons & RS_RUN_SKIP_VIDEO) == 0;
    g_buttons = buttons & ~uint32_t(RS_RUN_SKIP_VIDEO);
    if (g_audioBufferStatus) {
        const uint32_t capacity = g_host->audio_capacity();
        const uint32_t buffered = g_host->audio_buffered();
        const unsigned occupancy =
            capacity ? unsigned((uint64_t(buffered) * 100u) / capacity) : 0;
        /* One PSP hardware block is the immediate starvation boundary.
         * Auto frameskip suppresses rendering until emulation has rebuilt
         * enough audio, then resumes full video automatically. */
        g_audioBufferStatus(true, occupancy, buffered < 512u);
    }
    g_faultArmed = true;
    if (setjmp(g_faultJmp) == 0) retro_run();
    g_faultArmed = false;
}

static RSVideoFrame coreGetFrame(void) { return g_frameInfo; }

static uint32_t coreStateSize(void) {
    const size_t n = retro_serialize_size();
    return n <= MAX_STATE_BYTES ? uint32_t(n) : 0;
}

static int coreStateSave(void* buf, uint32_t size) {
    if (!buf || !size || size > MAX_STATE_BYTES) return -1;
    const size_t need = retro_serialize_size();
    if (!need || need > size || need > MAX_STATE_BYTES) return -1;
    g_faultArmed = true;
    const bool ok = setjmp(g_faultJmp) == 0 && retro_serialize(buf, size);
    g_faultArmed = false;
    return ok ? int(need) : -1;
}

static int coreStateLoad(const void* buf, uint32_t size) {
    if (!buf || !size || size > MAX_STATE_BYTES) return -1;
    g_faultArmed = true;
    const bool ok = setjmp(g_faultJmp) == 0 && retro_unserialize(buf, size);
    g_faultArmed = false;
    return ok ? 0 : -1;
}

static uint32_t coreSramSize(void) {
    const size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    return n <= UINT32_MAX ? uint32_t(n) : 0;
}

static void* coreSramData(void) {
    return retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
}

static int coreSramDirty(void) {
    const auto* p = static_cast<const uint8_t*>(
        retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    const size_t raw = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (raw > UINT32_MAX) return 0;
    const uint32_t n = uint32_t(raw);
    if (!p || !n) return 0;
    const uint32_t hash = adler32(p, n);
    if (hash == g_sramHash) return 0;
    g_sramHash = hash;
    return 1;
}

static int coreSetOption(const char*, const char*) {
    /* Options are pulled by the core via GET_VARIABLE. */
    return 0;
}

static int coreLoadRom(const char* path, const void* data, uint32_t size);
static void coreUnloadRom(void);

static RSCoreAPI g_api = {
    RS_CORE_API_VERSION,
    RS_CORE_NAME,
    g_version,
    RS_CORE_SYSTEMS,
    "",                 /* extensions: frontend maps by directory instead */
    60.0,               /* corrected after load_rom */
    0,
    coreInit,
    coreShutdown,
    coreLoadRom,
    coreUnloadRom,
    coreReset,
    coreRunFrame,
    coreGetFrame,
    coreStateSize,
    coreStateSave,
    coreStateLoad,
    coreSramSize,
    coreSramData,
    coreSramDirty,
    coreSetOption,
};

static int coreLoadRom(const char* path, const void* data, uint32_t size) {
    if ((!data || size == 0) && (!path || g_host->file_size(path) <= 0))
        return -1;

    g_romData = data;
    g_romSize = data ? size : uint32_t(g_host->file_size(path));
    std::snprintf(g_romPath, sizeof g_romPath, "%s", path ? path : "");

    retro_game_info game = {};
    game.path = path;
    game.data = data;
    game.size = size;

    /* A core that faults while parsing a malformed ROM is recoverable —
     * report failure and let the frontend surface it. */
    g_faultArmed = true;
    if (setjmp(g_faultJmp) != 0) {
        g_faultArmed = false;
        g_host->log(RS_LOG_ERROR, "libretro shim: core faulted loading ROM");
        return -1;
    }
    if (!retro_load_game(&game)) {
        g_faultArmed = false;
        g_host->log(RS_LOG_ERROR, "libretro shim: retro_load_game failed");
        return -1;
    }
    g_faultArmed = false;

    retro_system_av_info av = {};
    retro_get_system_av_info(&av);

    /* Frame buffer sized for the worst case the core declares. */
    const uint32_t bpp =
        (g_srcFormat == RETRO_PIXEL_FORMAT_XRGB8888) ? 4 : 2;
    const uint64_t frameBytes = uint64_t(av.geometry.max_width) *
                                uint64_t(av.geometry.max_height) * bpp;
    if (!av.geometry.base_width || !av.geometry.base_height ||
        av.geometry.base_width > av.geometry.max_width ||
        av.geometry.base_height > av.geometry.max_height ||
        av.geometry.max_width > MAX_GEOMETRY ||
        av.geometry.max_height > MAX_GEOMETRY ||
        uint64_t(av.geometry.base_width) * bpp > UINT16_MAX ||
        frameBytes == 0 || frameBytes > UINT32_MAX ||
        frameBytes > g_host->mem_available()) {
        g_host->log(RS_LOG_ERROR, "libretro shim: invalid geometry %ux%u/%ux%u",
                    av.geometry.base_width, av.geometry.base_height,
                    av.geometry.max_width, av.geometry.max_height);
        retro_unload_game();
        return -1;
    }
    if (!(av.timing.fps >= 10.0 && av.timing.fps <= 1000.0) ||
        !(av.timing.sample_rate >= 8000.0 &&
          av.timing.sample_rate <= 192000.0)) {
        g_host->log(RS_LOG_ERROR, "libretro shim: invalid timing");
        retro_unload_game();
        return -1;
    }
    g_frameCap = uint32_t(frameBytes);
#if !defined(RS_PSP_NATIVE_VIDEO)
    g_frame = static_cast<uint16_t*>(g_host->mem_alloc(g_frameCap, 64));
    if (!g_frame) {
        g_host->log(RS_LOG_ERROR, "libretro shim: no memory for frame buffer");
        retro_unload_game();
        return -1;
    }
    std::memset(g_frame, 0, g_frameCap);
#elif defined(RS_PSP_NATIVE_PIXELS)
    /* Small PSP-native resolve surface for 512-wide pseudo-hires/hires
     * frames. Ordinary 256-wide output remains zero-copy. */
    const uint64_t resolveBytes =
        uint64_t(256u) * av.geometry.max_height * sizeof(uint16_t);
    g_frame = resolveBytes <= UINT32_MAX
        ? static_cast<uint16_t*>(
              g_host->mem_alloc(uint32_t(resolveBytes), 64))
        : nullptr;
    if (!g_frame) {
        g_host->log(RS_LOG_ERROR,
                    "libretro shim: no memory for PSP hires resolve");
        retro_unload_game();
        return -1;
    }
    std::memset(g_frame, 0, size_t(resolveBytes));
#else
    g_frame = nullptr;
#endif
    g_frameInfo = {};
#ifndef RS_PSP_NATIVE_VIDEO
    g_frameInfo.pixels = g_frame;
#endif
    g_frameInfo.width  = uint16_t(av.geometry.base_width);
    g_frameInfo.height = uint16_t(av.geometry.base_height);
    g_frameInfo.pitch  = uint16_t(av.geometry.base_width * bpp);
    g_frameInfo.storage_height = uint16_t(av.geometry.base_height);
#ifdef RS_PSP_NATIVE_PIXELS
    g_frameInfo.format = RS_PIXFMT_RGBA5551;
#elif defined(RS_PSP_NATIVE_RGB565)
    g_frameInfo.format = RS_PIXFMT_RGB565;
#else
    g_frameInfo.format = rsPixFmt(g_srcFormat);
#endif

    g_api.fps = av.timing.fps;
    g_api.audio_rate = uint32_t(av.timing.sample_rate);
    g_host->audio_set_rate(g_api.audio_rate);
    g_gameLoaded = true;

    /* Baseline so an untouched SRAM doesn't read as dirty. */
    g_sramHash = 0;
    coreSramDirty();

    g_host->log(RS_LOG_INFO, "libretro shim: %ux%u @%.2ffps, audio %u Hz",
                av.geometry.base_width, av.geometry.base_height,
                av.timing.fps, g_api.audio_rate);
#ifdef RS_PSP_NATIVE_VIDEO
    g_host->log(RS_LOG_INFO,
                "libretro shim: PSP-native video active (zero conversion)");
#endif
    return 0;
}

static void coreUnloadRom(void) {
    if (g_gameLoaded) retro_unload_game();
    g_gameLoaded = false;
    if (g_frame) g_host->mem_free(g_frame);
    g_frame = nullptr;
    g_frameCap = 0;
    g_frameInfo = {};
}

extern "C" const RSCoreAPI* rs_get_core_api(void) { return &g_api; }

/* ------------------------------------------------------------------ */
/* PRX runtime support                                                  */
/*                                                                      */
/* Core modules link with -nostartfiles (no crt0 — crt0_prx wants to    */
/* own module_start and spawn a main thread, which is wrong for a       */
/* plugin), so the shim supplies the pieces of the C/C++ runtime an     */
/* emulator actually needs:                                             */
/*                                                                      */
/*   - static constructors: rs_add_core links crti/crtbegin/crtend/     */
/*     crtn, whose .init machinery (reached via _init below) walks      */
/*     .ctors; destructors are skipped — the module is discarded        */
/*     wholesale on unload;                                             */
/*   - a libc heap: malloc & friends forward to the host arena, so      */
/*     every allocation a core makes lives in the memory the frontend   */
/*     released for it, and unloading the core reclaims everything —    */
/*     free() returns blocks to the host's recyclable core heap.         */
/* ------------------------------------------------------------------ */

#ifndef RS_STATIC_BUILD
#include <pspkernel.h>

PSP_MODULE_INFO(RS_CORE_MODULE_ID, 0, 1, 0);

extern "C" {

void _init(void);   /* crti.o — runs the .ctors list */

/* C++ function-local statics (e.g. gambatte's lazily-built saver_list) go
 * through __cxa_guard_* for thread-safe one-time init. libstdc++ here is
 * built --enable-threads=posix, so the stock guards spin on a pthread
 * mutex that a bare PRX never initializes. Cores run single-threaded on
 * the frontend's main thread, so a plain "did it run yet" flag on the
 * guard's first byte is both correct and lock-free. */
int __cxa_guard_acquire(void* g) {
    return *static_cast<volatile char*>(g) == 0;
}
void __cxa_guard_release(void* g) {
    *static_cast<volatile char*>(g) = 1;
}
void __cxa_guard_abort(void*) {}

/* A faulting core must not take the frontend's main thread with it. When a
 * fault region is armed (inside run_frame / load_rom) unwind back to it;
 * otherwise — a fault during init or shutdown, where there is nothing to
 * unwind to — stop this thread rather than let a corrupt core run on. */
void abort(void) {
    if (g_host) g_host->log(RS_LOG_ERROR, "core aborted");
    g_faulted = true;
    if (g_faultArmed) {
        g_faultArmed = false;
        std::longjmp(g_faultJmp, 1);
    }
    sceKernelExitDeleteThread(0);
    for (;;) {}   /* unreachable; silences -Wreturn */
}

void _exit(int) { abort(); }

/* Each block is prefixed by a 16-byte header holding its requested size and
 * the arena pointer that must be returned to mem_free. Keeping the original
 * pointer lets posix_memalign satisfy cores that require 32/64-byte buffers. */
static void* alignedHeapAlloc(size_t size, size_t alignment) {
    if (alignment < 16) alignment = 16;
    if ((alignment & (alignment - 1)) != 0 || alignment > 4096 ||
        size > UINT32_MAX - alignment - 16)
        return nullptr;
    auto* base = static_cast<uint8_t*>(
        g_host->mem_alloc(uint32_t(size + alignment + 16), 16));
    if (!base) return nullptr;
    const uintptr_t raw = reinterpret_cast<uintptr_t>(base + 16);
    const uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
    auto* p = reinterpret_cast<uint32_t*>(aligned);
    p[-4] = uint32_t(size);
    p[-3] = uint32_t(reinterpret_cast<uintptr_t>(base));
    return p;
}

void* malloc(size_t size) {
    return alignedHeapAlloc(size, 16);
}

void free(void* ptr) {
    if (ptr) {
        const auto base = reinterpret_cast<void*>(
            uintptr_t(static_cast<uint32_t*>(ptr)[-3]));
        g_host->mem_free(base);
    }
}

void* calloc(size_t n, size_t size) {
    if (size && n > SIZE_MAX / size) return nullptr;   /* multiply overflow */
    void* p = malloc(n * size);
    if (p) std::memset(p, 0, n * size);
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    const uint32_t oldSize = static_cast<uint32_t*>(ptr)[-4];
    void* p = malloc(size);
    if (p) {
        std::memcpy(p, ptr, oldSize < size ? oldSize : size);
        free(ptr);
    }
    return p;
}

int posix_memalign(void** out, size_t alignment, size_t size) {
    if (alignment < sizeof(void*) ||
        (alignment & (alignment - 1)) != 0 || alignment > 4096)
        return 22;  /* EINVAL */
    void* p = alignedHeapAlloc(size, alignment);
    if (!p) return 12;
    *out = p;
    return 0;
}

/* newlib's stdio reaches for the reentrant entry points directly. */
void* _malloc_r(struct _reent*, size_t n) { return malloc(n); }
void  _free_r(struct _reent*, void* p) { free(p); }
void* _calloc_r(struct _reent*, size_t n, size_t s) { return calloc(n, s); }
void* _realloc_r(struct _reent*, void* p, size_t n) { return realloc(p, n); }

int module_start(SceSize args, void* argp) {
    /* Constructors are NOT run here — they may allocate, and the heap
     * needs the host table which only arrives at coreInit. Until then
     * the module just publishes its API table. */
    if (args >= sizeof(void*)) {
        const RSCoreAPI** slot = *reinterpret_cast<const RSCoreAPI***>(argp);
        if (slot) *slot = rs_get_core_api();
    }
    return 0;
}

int module_stop(SceSize, void*) { return 0; }

}  /* extern "C" */

static void runStaticCtors() {
    static bool done = false;
    if (done) return;
    done = true;
    _init();
}
#endif  /* !RS_STATIC_BUILD */
