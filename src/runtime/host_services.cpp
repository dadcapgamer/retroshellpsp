#include "runtime/host_services.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "runtime/arena.h"
#include "runtime/config.h"
#include "runtime/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace rs::host {

namespace {

u32 s_gameHash = 0;
volatile u32 s_input = 0;

/* Options for the active game, resolved once each and cached in fixed
 * storage. A std::map used here previously retained one heap allocation per
 * option queried by a feature-rich core. Large option sets can exhaust or
 * fragment the PSP-1000's small newlib heap immediately after loading.
 * The libretro callback needs stable borrowed strings, not a dynamic map. */
struct OptionEntry {
    char key[65] = {};
    char value[257] = {};
    bool used = false;
};
constexpr int MAX_OPTION_ENTRIES = 64;
OptionEntry s_optCache[MAX_OPTION_ENTRIES];
bool s_optionCacheFullLogged = false;

/* The arena remains a bump allocator, but libretro cores expect free() to
 * make transient blocks reusable. Track core allocations without using the
 * core heap itself and recycle whole freed blocks. */
struct CoreBlock {
    void* ptr = nullptr;
    u32 size = 0;
    u32 align = 0;
    bool live = false;
};
constexpr int MAX_CORE_BLOCKS = 1024;
CoreBlock s_blocks[MAX_CORE_BLOCKS];
u32 s_allocFailures = 0;

void hostLog(int level, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log::write(level, "[core] %s", buf);
}

void* hostAlloc(uint32_t size, uint32_t align) {
    if (!size) size = 1;
    if (!align) align = 16;
    if ((align & (align - 1)) != 0 || align > 4096) {
        s_allocFailures++;
        return nullptr;
    }
    for (auto& b : s_blocks) {
        if (!b.live && b.ptr && b.size >= size &&
            (reinterpret_cast<uintptr_t>(b.ptr) & (align - 1)) == 0) {
            b.live = true;
            return b.ptr;
        }
    }
    CoreBlock* record = nullptr;
    for (auto& b : s_blocks) {
        if (!b.ptr) {
            record = &b;
            break;
        }
    }
    if (!record) {
        s_allocFailures++;
        RS_LOGW("core heap: allocation table exhausted");
        return nullptr;
    }
    void* p = mem::alloc(size, align);
    if (!p) {
        s_allocFailures++;
        return nullptr;
    }
    *record = {p, size, align, true};
    return p;
}

void hostFree(void* ptr) {
    if (!ptr) return;
    for (auto& b : s_blocks) {
        if (b.ptr == ptr && b.live) {
            b.live = false;
            return;
        }
    }
    RS_LOGW("core heap: ignored invalid/double free %p", ptr);
}

uint32_t hostAvailable() { return mem::available(); }

void hostSetRate(uint32_t hz) {
    if (hz < 8000 || hz > 192000) {
        RS_LOGW("core: rejected audio rate %u", unsigned(hz));
        hz = audio::OUTPUT_RATE;
    }
    audio::setSourceRate(hz);
}

void hostPush(const int16_t* stereo, uint32_t frames) {
    if (!stereo || frames > 16384) {
        RS_LOGW("core: rejected audio batch %u", unsigned(frames));
        return;
    }
    audio::push(stereo, frames);
}

uint32_t hostAudioBuffered() { return audio::buffered(); }
uint32_t hostAudioCapacity() { return audio::capacity(); }

uint32_t hostInput() { return s_input; }

int32_t hostFileSize(const char* path) { return fs::fileSize(path); }

int32_t hostFileRead(const char* path, void* buf, uint32_t offset,
                     uint32_t size) {
    return fs::readRange(path, buf, offset, size);
}

const char* hostGetOption(const char* key) {
    if (!key || !*key || std::strlen(key) >= sizeof(OptionEntry::key))
        return nullptr;
    OptionEntry* empty = nullptr;
    for (auto& entry : s_optCache) {
        if (entry.used && std::strcmp(entry.key, key) == 0)
            return entry.value[0] ? entry.value : nullptr;
        if (!entry.used && !empty) empty = &entry;
    }
    if (!empty) {
        if (!s_optionCacheFullLogged) {
            RS_LOGW("core options: fixed cache full");
            s_optionCacheFullLogged = true;
        }
        return nullptr;
    }
    std::string value = cfg::gameOption(s_gameHash, key);
    std::snprintf(empty->key, sizeof empty->key, "%s", key);
    std::snprintf(empty->value, sizeof empty->value, "%s", value.c_str());
    empty->used = true;
    return empty->value[0] ? empty->value : nullptr;
}

const RSHostAPI s_table = {
    RS_HOST_API_VERSION,
    &hostLog,
    &hostAlloc,
    &hostFree,
    &hostAvailable,
    &hostSetRate,
    &hostPush,
    &hostAudioBuffered,
    &hostAudioCapacity,
    &hostInput,
    &hostFileSize,
    &hostFileRead,
    &hostGetOption,
};

}  // namespace

const RSHostAPI* table() { return &s_table; }

void beginCoreSession() {
    for (auto& b : s_blocks) b = CoreBlock{};
    s_allocFailures = 0;
}

void endCoreSession() {
    u32 live = 0;
    for (const auto& b : s_blocks)
        if (b.live) live += b.size;
    RS_LOGI("core heap: high-water %u KB, live at unload %u KB, failures %u",
            unsigned(mem::highWater() / 1024), unsigned(live / 1024),
            unsigned(s_allocFailures + mem::allocationFailures()));
    for (auto& b : s_blocks) b = CoreBlock{};
}

u32 allocationFailures() {
    return s_allocFailures + mem::allocationFailures();
}

void setActiveGame(u32 pathHash) {
    s_gameHash = pathHash;
    for (auto& entry : s_optCache) entry = OptionEntry{};
    s_optionCacheFullLogged = false;
}
void setInputState(u32 rsButtons) { s_input = rsButtons; }

}  // namespace rs::host
