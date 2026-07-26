#include "platform/psp/fs_psp.h"

#include <pspiofilemgr.h>

#include <cstring>
#include <cstdio>

namespace rs::fs {

namespace {
u32 packTime(const ScePspDateTime& t) {
    /* Not a real timestamp — a monotonic-per-change packing used only to
     * detect modified files between scans. */
    return (u32(t.year - 2000) << 26) | (u32(t.month) << 22) |
           (u32(t.day) << 17) | (u32(t.hour) << 12) | (u32(t.minute) << 6) |
           u32(t.second);
}

SceUID openReadWithBackup(const char* path) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) return fd;
    char backup[320];
    const int n = std::snprintf(backup, sizeof backup, "%s.bak", path);
    if (n <= 0 || n >= int(sizeof backup)) return fd;
    return sceIoOpen(backup, PSP_O_RDONLY, 0);
}
}  // namespace

bool exists(const char* path) {
    SceIoStat st{};
    return sceIoGetstat(path, &st) >= 0;
}

bool mkdirs(const char* path) {
    char buf[256];
    const int length = std::snprintf(buf, sizeof buf, "%s", path);
    if (length <= 0 || length >= int(sizeof buf)) return false;
    /* Walk past "ms0:/", creating each component. */
    char* p = std::strchr(buf, '/');
    while (p) {
        char* next = std::strchr(p + 1, '/');
        if (next) *next = 0;
        else p = nullptr;
        if (!exists(buf)) sceIoMkdir(buf, 0777);
        if (next) {
            *next = '/';
            p = next;
        }
    }
    if (!exists(path)) sceIoMkdir(path, 0777);
    return exists(path);
}

s32 fileSize(const char* path) {
    SceUID fd = openReadWithBackup(path);
    if (fd < 0) return -1;
    const s32 size = s32(sceIoLseek32(fd, 0, PSP_SEEK_END));
    sceIoClose(fd);
    return size;
}

bool listDir(const char* path, std::vector<DirEntry>& out) {
    SceUID d = sceIoDopen(path);
    if (d < 0) return false;
    SceIoDirent ent;
    std::memset(&ent, 0, sizeof ent);
    while (sceIoDread(d, &ent) > 0) {
        if (ent.d_name[0] == '.' &&
            (ent.d_name[1] == 0 || (ent.d_name[1] == '.' && ent.d_name[2] == 0))) {
            std::memset(&ent, 0, sizeof ent);
            continue;
        }
        DirEntry e;
        e.name  = ent.d_name;
        e.size  = u32(ent.d_stat.st_size);
        e.mtime = packTime(ent.d_stat.sce_st_mtime);
        e.isDir = FIO_S_ISDIR(ent.d_stat.st_mode);
        out.push_back(std::move(e));
        std::memset(&ent, 0, sizeof ent);
    }
    sceIoDclose(d);
    return true;
}

namespace {
bool writeAll(const char* path, const void* data, u32 size) {
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (fd < 0) return false;
    const u8* p = static_cast<const u8*>(data);
    u32 left = size;
    bool ok = true;
    while (left) {
        const int put = sceIoWrite(fd, p, left);
        if (put <= 0) {
            ok = false;
            break;
        }
        p += put;
        left -= u32(put);
    }
    if (sceIoClose(fd) < 0) ok = false;
    return ok;
}
}  // namespace

bool readFile(const char* path, std::vector<u8>& out, u32 maxBytes) {
    SceUID fd = openReadWithBackup(path);
    if (fd < 0) return false;
    const s32 size = s32(sceIoLseek32(fd, 0, PSP_SEEK_END));
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    if (size < 0 || u32(size) > maxBytes ||
        out.size() > size_t(maxBytes) - size_t(size)) {
        sceIoClose(fd);
        return false;
    }
    const size_t base = out.size();
    out.resize(base + size_t(size));
    u8* dst = out.data() + base;
    s32 total = 0;
    while (total < size) {
        const int got = sceIoRead(fd, dst + total, SceSize(size - total));
        if (got <= 0) break;
        total += got;
    }
    sceIoClose(fd);
    if (total != size) {
        out.resize(base);
        return false;
    }
    return true;
}

bool writeFile(const char* path, const void* data, u32 size) {
    return writeAll(path, data, size);
}

bool writeFileAtomic(const char* path, const void* data, u32 size) {
    char tmp[320], bak[320];
    const int tn = std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    const int bn = std::snprintf(bak, sizeof bak, "%s.bak", path);
    if (tn <= 0 || tn >= int(sizeof tmp) ||
        bn <= 0 || bn >= int(sizeof bak))
        return false;
    sceIoRemove(tmp);
    if (!writeAll(tmp, data, size)) {
        sceIoRemove(tmp);
        return false;
    }
    sceIoSync("ms0:", 0);
    sceIoRemove(bak);
    const bool hadOld = exists(path);
    if (hadOld && sceIoRename(path, bak) < 0) {
        sceIoRemove(tmp);
        return false;
    }
    if (sceIoRename(tmp, path) < 0) {
        if (hadOld) sceIoRename(bak, path);
        sceIoRemove(tmp);
        return false;
    }
    sceIoSync("ms0:", 0);
    sceIoRemove(bak);
    return true;
}

s32 readRange(const char* path, void* buf, u32 offset, u32 size) {
    SceUID fd = openReadWithBackup(path);
    if (fd < 0) return -1;
    if (offset > 0x7FFFFFFFu ||
        sceIoLseek32(fd, s32(offset), PSP_SEEK_SET) < 0) {
        sceIoClose(fd);
        return -1;
    }
    u8* dst = static_cast<u8*>(buf);
    u32 total = 0;
    while (total < size) {
        const int got = sceIoRead(fd, dst + total, size - total);
        if (got < 0) {
            sceIoClose(fd);
            return total ? s32(total) : s32(got);
        }
        if (got == 0) break;
        total += u32(got);
    }
    sceIoClose(fd);
    return s32(total);
}

}  // namespace rs::fs
