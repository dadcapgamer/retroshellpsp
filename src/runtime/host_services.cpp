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

void hostLog(int level, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log::write(level, "[core] %s", buf);
}

void* hostAlloc(uint32_t size, uint32_t align) {
    return mem::alloc(size, align ? align : 16);
}

void hostFree(void*) {
    /* Arena memory is reclaimed wholesale on core unload. */
}

uint32_t hostAvailable() { return mem::available(); }

void hostSetRate(uint32_t hz) { audio::setSourceRate(hz); }

void hostPush(const int16_t* stereo, uint32_t frames) {
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

void setActiveGame(u32 pathHash) {
    s_gameHash = pathHash;
    s_optCache.clear();   /* options are per-game */
}
void setInputState(u32 rsButtons) { s_input = rsButtons; }

}  // namespace rs::host
