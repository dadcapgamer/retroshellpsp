#include "runtime/host_services.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "runtime/arena.h"
#include "runtime/config.h"
#include "runtime/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace rs::host {

namespace {

u32 s_gameHash = 0;
volatile u32 s_input = 0;

/* get_option returns borrowed pointers; keep a small rotation of buffers
 * so a core can hold a few results at once. */
char s_optBufs[4][64];
int  s_optClock = 0;

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
    const std::string v = cfg::gameOption(s_gameHash, key);
    if (v.empty()) return nullptr;
    char* buf = s_optBufs[s_optClock];
    s_optClock = (s_optClock + 1) % 4;
    std::snprintf(buf, sizeof s_optBufs[0], "%s", v.c_str());
    return buf;
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

void setActiveGame(u32 pathHash) { s_gameHash = pathHash; }
void setInputState(u32 rsButtons) { s_input = rsButtons; }

}  // namespace rs::host
