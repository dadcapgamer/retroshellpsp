/** Memory Stick filesystem access — the only place sceIo* is called.
 *
 * All RetroShell data lives under ms0:/RETROSHELL/ except ROMs, which the
 * user keeps anywhere below ms0:/ROMS/. Older ms0:/RETROSUITE installs are
 * migrated before any persistent frontend service starts.
 */
#pragma once

#include "rs_common.h"

#include <functional>
#include <string>
#include <vector>

namespace rs::fs {

constexpr const char* ROOT        = "ms0:/RETROSHELL";
constexpr const char* LEGACY_ROOT = "ms0:/RETROSUITE";
constexpr const char* ROM_ROOT = "ms0:/ROMS";
constexpr u32 DEFAULT_MAX_FILE = 4u * 1024u * 1024u;

struct DirEntry {
    std::string name;      /* file name only, no path */
    u32  size   = 0;
    u32  mtime  = 0;       /* packed local time, monotonic per file change */
    bool isDir  = false;
};

enum class RootMigration { None, Renamed, Merged, Failed };

/* Moves a legacy RETROSUITE tree into RETROSHELL. If a freshly installed
 * release already created RETROSHELL, user data is merged without replacing
 * the new package; collisions are retained with a .legacy suffix. */
RootMigration migrateLegacyRoot();

bool exists(const char* path);
bool mkdirs(const char* path);              /* creates parents as needed  */
s32  fileSize(const char* path);

/* Lists a directory non-recursively; returns false if it can't be opened. */
bool listDir(const char* path, std::vector<DirEntry>& out);

/* Whole-file helpers. read appends to `out`, rejects oversized files, and
 * falls back to the last atomic-write backup if the main file is absent. */
bool readFile(const char* path, std::vector<u8>& out,
              u32 maxBytes = DEFAULT_MAX_FILE);
bool writeFile(const char* path, const void* data, u32 size);
bool writeFileAtomic(const char* path, const void* data, u32 size);

/* Random-access read for streaming; returns bytes read or <0. */
s32 readRange(const char* path, void* buf, u32 offset, u32 size);

}  // namespace rs::fs
