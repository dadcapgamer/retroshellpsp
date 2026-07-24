/** Memory Stick filesystem access — the only place sceIo* is called.
 *
 * All RetroSuite data lives under ms0:/RETROSUITE/ except ROMs, which the
 * user keeps in ms0:/ROMS/<System>/ per the documented layout.
 */
#pragma once

#include "rs_common.h"

#include <functional>
#include <string>
#include <vector>

namespace rs::fs {

constexpr const char* ROOT     = "ms0:/RETROSUITE";
constexpr const char* ROM_ROOT = "ms0:/ROMS";

struct DirEntry {
    std::string name;      /* file name only, no path */
    u32  size   = 0;
    u32  mtime  = 0;       /* packed local time, monotonic per file change */
    bool isDir  = false;
};

bool exists(const char* path);
bool mkdirs(const char* path);              /* creates parents as needed  */
s32  fileSize(const char* path);

/* Lists a directory non-recursively; returns false if it can't be opened. */
bool listDir(const char* path, std::vector<DirEntry>& out);

/* Whole-file helpers. read appends to `out` and returns success. */
bool readFile(const char* path, std::vector<u8>& out);
bool writeFile(const char* path, const void* data, u32 size);

/* Random-access read for streaming; returns bytes read or <0. */
s32 readRange(const char* path, void* buf, u32 offset, u32 size);

}  // namespace rs::fs
