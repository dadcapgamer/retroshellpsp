/** Leveled logging: stdout (visible in PPSSPP's console) plus an optional
 * file on the Memory Stick for debugging on real hardware. */
#pragma once

namespace rs::log {

enum Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void init(bool toFile);
void shutdown();
void write(int level, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

}  // namespace rs::log

#define RS_LOGD(...) ::rs::log::write(::rs::log::Debug, __VA_ARGS__)
#define RS_LOGI(...) ::rs::log::write(::rs::log::Info, __VA_ARGS__)
#define RS_LOGW(...) ::rs::log::write(::rs::log::Warn, __VA_ARGS__)
#define RS_LOGE(...) ::rs::log::write(::rs::log::Error, __VA_ARGS__)
