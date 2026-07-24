#include "platform/psp/fs_psp.h"

#include <pspiofilemgr.h>

#include <cstring>

namespace rs::fs {

namespace {
u32 packTime(const ScePspDateTime& t) {
    /* Not a real timestamp — a monotonic-per-change packing used only to
     * detect modified files between scans. */
    return (u32(t.year - 2000) << 26) | (u32(t.month) << 22) |
           (u32(t.day) << 17) | (u32(t.hour) << 12) | (u32(t.minute) << 6) |
           u32(t.second);
}
}  // namespace

bool exists(const char* path) {
    SceIoStat st{};
    return sceIoGetstat(path, &st) >= 0;
}

bool mkdirs(const char* path) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s", path);
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
    SceIoStat st{};
    if (sceIoGetstat(path, &st) < 0) return -1;
    return s32(st.st_size);
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

bool readFile(const char* path, std::vector<u8>& out) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return false;
    const s32 size = s32(sceIoLseek32(fd, 0, PSP_SEEK_END));
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    if (size < 0) {
        sceIoClose(fd);
        return false;
    }
    const size_t base = out.size();
    out.resize(base + size_t(size));
    const int got = sceIoRead(fd, out.data() + base, SceSize(size));
    sceIoClose(fd);
    if (got != size) {
        out.resize(base);
        return false;
    }
    return true;
}

bool writeFile(const char* path, const void* data, u32 size) {
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return false;
    const int put = sceIoWrite(fd, data, size);
    sceIoClose(fd);
    return put == int(size);
}

s32 readRange(const char* path, void* buf, u32 offset, u32 size) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return -1;
    sceIoLseek32(fd, int(offset), PSP_SEEK_SET);
    const int got = sceIoRead(fd, buf, size);
    sceIoClose(fd);
    return got;
}

}  // namespace rs::fs
