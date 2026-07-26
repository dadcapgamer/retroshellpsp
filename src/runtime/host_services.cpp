#include "runtime/host_services.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "runtime/arena.h"
#include "runtime/config.h"
#include "runtime/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace rs::host {

namespace {

u32 s_gameHash = 0;
volatile u32 s_input = 0;

/* Options for the active game, resolved once each and cached in memory: a
 * core may call get_option every frame, and cfg::gameOption reads+parses a
 * Memory Stick file per call, which would stall real hardware. std::map
 * gives stable storage so the borrowed const char* stays valid. Cleared
 * when the active game changes. */
std::map<std::string, std::string> s_optCache;

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

uint32_t hostInput() { return s_input; }

int32_t hostFileSize(const char* path) { return fs::fileSize(path); }

int32_t hostFileRead(const char* path, void* buf, uint32_t offset,
                     uint32_t size) {
    return fs::readRange(path, buf, offset, size);
}

const char* hostGetOption(const char* key) {
    auto it = s_optCache.find(key);
    if (it == s_optCache.end())   /* first query for this key: read once */
        it = s_optCache.emplace(key, cfg::gameOption(s_gameHash, key)).first;
    return it->second.empty() ? nullptr : it->second.c_str();
}

const RSHostAPI s_table = {
    RS_HOST_API_VERSION,
    &hostLog,
    &hostAlloc,
    &hostFree,
    &hostAvailable,
    &hostSetRate,
    &hostPush,
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
    s_optCache.clear();   /* options are per-game */
}
void setInputState(u32 rsButtons) { s_input = rsButtons; }

}  // namespace rs::host
