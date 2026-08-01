#include "runtime/log.h"
#include "platform/psp/fs_psp.h"

#include <pspiofilemgr.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace rs::log {

namespace {
SceUID s_fd = -1;
const char* LEVEL_TAG[4] = {"D", "I", "W", "E"};
}  // namespace

void init(bool toFile) {
    if (!toFile) return;
    sceIoMkdir(fs::ROOT, 0777);
    s_fd = sceIoOpen("ms0:/RETROSHELL/retroshell.log",
                     PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
}

void shutdown() {
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
    if (s_fd >= 0) sceIoWrite(s_fd, buf, SceSize(n));
}

}  // namespace rs::log
