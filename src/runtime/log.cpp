#include "runtime/log.h"
#include "platform/psp/fs_psp.h"
#include "rs_common.h"

#include <pspiofilemgr.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace rs::log {

namespace {
SceUID s_fd = -1;
const char* LEVEL_TAG[4] = {"D", "I", "W", "E"};
bool s_deferred = false;
constexpr u32 DEFER_BYTES = 16u * 1024u;
char s_pending[DEFER_BYTES];
u32 s_pendingLen = 0;
}  // namespace

void flush() {
    if (s_fd >= 0 && s_pendingLen) sceIoWrite(s_fd, s_pending, s_pendingLen);
    s_pendingLen = 0;
}

void setDeferred(bool deferred) {
    if (!deferred) flush();
    s_deferred = deferred;
}

void init(bool toFile) {
    if (!toFile) return;
    sceIoMkdir(fs::ROOT, 0777);
    s_fd = sceIoOpen("ms0:/RETROSHELL/retroshell.log",
                     PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
}

void shutdown() {
    flush();
    if (s_fd >= 0) sceIoClose(s_fd);
    s_fd = -1;
}

void write(int level, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int prefix = std::snprintf(buf, sizeof buf, "[%s] ",
                                     LEVEL_TAG[level & 3]);
    int n = std::vsnprintf(buf + prefix, sizeof buf - prefix - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    n += prefix;
    if (n > int(sizeof buf) - 2) n = int(sizeof buf) - 2;
    buf[n++] = '\n';
    buf[n] = 0;

    std::fputs(buf, stdout);
    if (s_fd < 0) return;
    if (!s_deferred) {
        sceIoWrite(s_fd, buf, SceSize(n));
        return;
    }
    if (s_pendingLen + u32(n) > DEFER_BYTES) flush();
    std::memcpy(s_pending + s_pendingLen, buf, size_t(n));
    s_pendingLen += u32(n);
    if ((level & 3) == Error) flush();   /* an error may precede a crash */
}

}  // namespace rs::log
