#include "runtime/save_manager.h"
#include "frontend/emulator_core.h"
#include "platform/psp/fs_psp.h"
#include "runtime/host_services.h"
#include "runtime/log.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace rs::save {

namespace {

constexpr u32 STATE_MAGIC   = 0x54535352u;  /* "RSST" */
constexpr u32 STATE_VERSION = 1;
constexpr u32 MAX_STATE_BYTES = 2u * 1024u * 1024u;
constexpr u32 MAX_SRAM_BYTES = 1024u * 1024u;

struct __attribute__((packed)) StateHeader {
    u32  magic, version;
    char coreName[16];
    char coreVersion[16];
    u32  payloadSize;
    u16  thumbW, thumbH;
};

class ScopedHostBuffer {
public:
    ScopedHostBuffer(const RSHostAPI* services, u32 size)
        : m_services(services), m_data(services->mem_alloc(size, 16)) {}
    ~ScopedHostBuffer() {
        if (m_data) m_services->mem_free(m_data);
    }
    ScopedHostBuffer(const ScopedHostBuffer&) = delete;
    ScopedHostBuffer& operator=(const ScopedHostBuffer&) = delete;
    void* data() const { return m_data; }
    u8* bytes() const { return static_cast<u8*>(m_data); }

private:
    const RSHostAPI* m_services;
    void* m_data;
};

void gameDir(char* buf, size_t n, const db::GameEntry& g) {
    std::snprintf(buf, n, "%s/saves/%s/%08x", fs::ROOT,
                  db::systemInfo(g.system).dirName, unsigned(g.pathHash));
}

void statePath(char* buf, size_t n, const db::GameEntry& g, int slot) {
    char dir[128];
    gameDir(dir, sizeof dir, g);
    std::snprintf(buf, n, "%s/state%d.rst", dir, slot);
}

void previewPath(char* buf, size_t n, const db::GameEntry& g, int slot) {
    char dir[128];
    gameDir(dir, sizeof dir, g);
    std::snprintf(buf, n, "%s/state%d.prv", dir, slot);
}

constexpr u32 PREVIEW_MAGIC = 0x56505352u;   /* "RSPV" */

/* fs::DirEntry::mtime packing (see fs_psp.cpp) to a YYYYMMDDHHMM stamp. */
u64 stampFromPacked(u32 t) {
    if (!t) return 0;
    const u64 year = 2000u + (t >> 26), month = (t >> 22) & 0xFu;
    const u64 day = (t >> 17) & 0x1Fu, hour = (t >> 12) & 0x1Fu;
    const u64 minute = (t >> 6) & 0x3Fu;
    return year * 100000000ull + month * 1000000ull + day * 10000ull +
           hour * 100ull + minute;
}

}  // namespace

namespace {
const char* const SAVE_FILES[] = {
    "sram.bin", "rtc.bin", "state0.rst", "state1.rst", "state2.rst",
    "state3.rst", "state4.rst", "state0.prv", "state1.prv", "state2.prv",
    "state3.prv", "state4.prv",
};
static_assert(sizeof(SAVE_FILES) / sizeof(SAVE_FILES[0]) == 2 + 2 * SLOTS,
              "SAVE_FILES must list every state slot and its preview");
}  // namespace

bool hasAnySave(const db::GameEntry& game) {
    char dir[128];
    gameDir(dir, sizeof dir, game);
    for (const char* name : SAVE_FILES) {
        char path[192];
        std::snprintf(path, sizeof path, "%s/%s", dir, name);
        if (fs::fileSize(path) >= 0) return true;
    }
    return false;
}

int deleteAll(const db::GameEntry& game) {
    char dir[128];
    gameDir(dir, sizeof dir, game);
    int removed = 0;
    for (const char* name : SAVE_FILES) {
        char path[192], backup[200];
        std::snprintf(path, sizeof path, "%s/%s", dir, name);
        std::snprintf(backup, sizeof backup, "%s.bak", path);
        const bool existed = fs::exists(path);
        if (fs::removeFile(path) && existed) removed++;
        fs::removeFile(backup);
    }
    return removed;
}

void querySlots(const db::GameEntry& game, SlotInfo out[SLOTS]) {
    /* One listing gives every slot's modification time. */
    char dir[128];
    gameDir(dir, sizeof dir, game);
    std::vector<fs::DirEntry> entries;
    fs::listDir(dir, entries);
    for (int i = 0; i < SLOTS; i++) {
        out[i] = SlotInfo{};
        char path[160];
        statePath(path, sizeof path, game, i);
        StateHeader h{};
        if (fs::readRange(path, &h, 0, sizeof h) == int(sizeof h) &&
            h.magic == STATE_MAGIC && h.version == STATE_VERSION &&
            h.payloadSize > 0 && h.payloadSize <= MAX_STATE_BYTES &&
            ((h.thumbW == 0 && h.thumbH == 0) ||
             (h.thumbW == THUMB_W && h.thumbH == THUMB_H))) {
            out[i].exists = true;
            out[i].payloadSize = h.payloadSize;
            std::memcpy(out[i].coreName, h.coreName, sizeof out[i].coreName);
            out[i].coreName[sizeof out[i].coreName - 1] = 0;
            char name[16];
            std::snprintf(name, sizeof name, "state%d.rst", i);
            for (const auto& e : entries)
                if (e.name == name) out[i].stamp = stampFromPacked(e.mtime);
        }
    }
}

bool deleteState(const db::GameEntry& game, int slot) {
    if (slot < 0 || slot >= SLOTS) return false;
    char path[160], backup[168];
    statePath(path, sizeof path, game, slot);
    const bool existed = fs::exists(path);
    fs::removeFile(path);
    std::snprintf(backup, sizeof backup, "%s.bak", path);
    fs::removeFile(backup);
    previewPath(path, sizeof path, game, slot);
    fs::removeFile(path);
    std::snprintf(backup, sizeof backup, "%s.bak", path);
    fs::removeFile(backup);
    RS_LOGI("save: state slot %d deleted", slot);
    return existed;
}

void dropLegacyResumeState(const db::GameEntry& game) {
    char path[160];
    statePath(path, sizeof path, game, SLOTS);
    if (!fs::exists(path)) return;
    fs::removeFile(path);
    char backup[168];
    std::snprintf(backup, sizeof backup, "%s.bak", path);
    fs::removeFile(backup);
    RS_LOGI("save: removed leftover Apply & Restart state");
}

bool savePreview(const db::GameEntry& game, int slot, const u16* pixels, int w,
                 int h) {
    if (slot < 0 || slot >= SLOTS || !pixels || w <= 0 || h <= 0 ||
        w > PREVIEW_MAX_W || h > PREVIEW_MAX_H)
        return false;
    const size_t bytes = 8 + size_t(w) * size_t(h) * 2;
    std::vector<u8> blob(bytes);
    const u32 magic = PREVIEW_MAGIC;
    const u16 dims[2] = {u16(w), u16(h)};
    std::memcpy(blob.data(), &magic, 4);
    std::memcpy(blob.data() + 4, dims, 4);
    std::memcpy(blob.data() + 8, pixels, bytes - 8);
    char path[160];
    previewPath(path, sizeof path, game, slot);
    const bool ok = fs::writeFileAtomic(path, blob.data(), u32(bytes));
    if (!ok) RS_LOGW("save: preview for slot %d not written", slot);
    return ok;
}

void dropPreview(const db::GameEntry& game, int slot) {
    if (slot < 0 || slot >= SLOTS) return;
    char path[160], backup[168];
    previewPath(path, sizeof path, game, slot);
    fs::removeFile(path);
    std::snprintf(backup, sizeof backup, "%s.bak", path);
    fs::removeFile(backup);
}

bool loadPreview(const db::GameEntry& game, int slot, std::vector<u16>& out,
                 int& w, int& h) {
    out.clear();
    w = h = 0;
    if (slot < 0 || slot >= SLOTS) return false;
    char path[160];
    previewPath(path, sizeof path, game, slot);
    std::vector<u8> blob;
    if (!fs::exists(path) ||
        !fs::readFile(path, blob, 8u + PREVIEW_MAX_W * PREVIEW_MAX_H * 2u) ||
        blob.size() < 8)
        return false;
    u32 magic;
    u16 dims[2];
    std::memcpy(&magic, blob.data(), 4);
    std::memcpy(dims, blob.data() + 4, 4);
    if (magic != PREVIEW_MAGIC || !dims[0] || !dims[1] ||
        dims[0] > PREVIEW_MAX_W || dims[1] > PREVIEW_MAX_H ||
        blob.size() < 8 + size_t(dims[0]) * dims[1] * 2)
        return false;
    w = dims[0];
    h = dims[1];
    out.resize(size_t(w) * size_t(h));
    std::memcpy(out.data(), blob.data() + 8, out.size() * 2);
    return true;
}

bool saveState(const db::GameEntry& game, EmulatorCore& core, int slot,
               const u16* thumb) {
    if (slot < 0 || slot >= SLOTS) return false;
    RS_LOGI("save: state slot %d querying core size", slot);
    const u32 maxSize = core.stateSize();
    if (!maxSize || maxSize > MAX_STATE_BYTES) {
        RS_LOGE("save: invalid core state size (%u)", unsigned(maxSize));
        return false;
    }
    RS_LOGI("save: state slot %d core size %u bytes", slot,
            unsigned(maxSize));

    const u32 thumbBytes = thumb ? THUMB_W * THUMB_H * 2u : 0u;
    const u32 capacity = u32(sizeof(StateHeader)) + thumbBytes + maxSize;
    ScopedHostBuffer file(host::table(), capacity);
    if (!file.data()) {
        RS_LOGE("save: no arena space for state file (%u bytes)",
                unsigned(capacity));
        return false;
    }
    u8* payload = file.bytes() + sizeof(StateHeader) + thumbBytes;
    RS_LOGI("save: state slot %d serializing", slot);
    const int used = core.stateSave(payload, maxSize);
    if (used <= 0 || u32(used) > maxSize) {
        RS_LOGE("save: core state_save failed");
        return false;
    }

    StateHeader h{};
    h.magic = STATE_MAGIC;
    h.version = STATE_VERSION;
    std::snprintf(h.coreName, sizeof h.coreName, "%s", core.name());
    std::snprintf(h.coreVersion, sizeof h.coreVersion, "%s", core.version());
    h.payloadSize = u32(used);
    h.thumbW = thumb ? u16(THUMB_W) : 0;
    h.thumbH = thumb ? u16(THUMB_H) : 0;

    std::memcpy(file.data(), &h, sizeof h);
    if (thumb)
        std::memcpy(file.bytes() + sizeof h, thumb, thumbBytes);
    const u32 fileBytes = u32(sizeof h) + thumbBytes + u32(used);

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    statePath(path, sizeof path, game, slot);
    const bool ok = fs::writeFileAtomic(path, file.data(), fileBytes);
    RS_LOGI("save: state slot %d %s (%d bytes)", slot, ok ? "ok" : "FAILED",
            used);
    return ok;
}

bool loadState(const db::GameEntry& game, EmulatorCore& core, int slot) {
    if (slot < 0 || slot >= SLOTS) return false;
    RS_LOGI("save: state slot %d preparing load", slot);
    char path[160];
    statePath(path, sizeof path, game, slot);
    const s32 fileSize = fs::fileSize(path);
    const u32 maxFileBytes = MAX_STATE_BYTES + 64u * 1024u;
    if (fileSize < s32(sizeof(StateHeader)) ||
        fileSize > s32(maxFileBytes)) {
        RS_LOGW("save: state slot %d unusable size %d (%s)", slot,
                int(fileSize), path);
        return false;
    }

    const RSHostAPI* services = host::table();
    ScopedHostBuffer file(services, u32(fileSize));
    if (!file.data()) {
        RS_LOGE("save: no arena space to load state (%d bytes)", int(fileSize));
        return false;
    }
    if (fs::readRange(path, file.data(), 0, u32(fileSize)) != fileSize) {
        RS_LOGE("save: incomplete state read");
        return false;
    }

    StateHeader h{};
    std::memcpy(&h, file.data(), sizeof h);
    if (h.magic != STATE_MAGIC || h.version != STATE_VERSION) {
        RS_LOGW("save: state slot %d bad header (magic %08x, version %u)",
                slot, unsigned(h.magic), unsigned(h.version));
        return false;
    }
    if (!h.payloadSize || h.payloadSize > MAX_STATE_BYTES ||
        !((h.thumbW == 0 && h.thumbH == 0) ||
          (h.thumbW == THUMB_W && h.thumbH == THUMB_H))) {
        RS_LOGW("save: state slot %d bad layout (payload %u, thumb %ux%u)",
                slot, unsigned(h.payloadSize), unsigned(h.thumbW),
                unsigned(h.thumbH));
        return false;
    }
    h.coreName[sizeof h.coreName - 1] = 0;   /* a corrupt field may lack NUL */
    if (std::strncmp(h.coreName, core.name(), sizeof h.coreName) != 0) {
        RS_LOGW("save: state belongs to core '%s', running '%s'", h.coreName,
                core.name());
        return false;
    }
    /* Validate header fields with overflow-safe arithmetic: payloadSize and
     * thumb dimensions come straight from the file and a corrupt state must
     * not wrap the bounds check into an out-of-bounds read. */
    const size_t total = size_t(fileSize);
    const size_t thumbBytes = size_t(h.thumbW) * h.thumbH * 2;
    const size_t off = sizeof h + thumbBytes;
    if (off < sizeof h || off > total ||                      /* thumb overflow */
        h.payloadSize > total - off) {                        /* payload overflow */
        RS_LOGW("save: state slot %d truncated (%u of %u payload bytes)", slot,
                unsigned(off <= total ? total - off : 0),
                unsigned(h.payloadSize));
        return false;
    }
    /* A rejecting core may already have consumed part of the input. Keep a
     * rollback snapshot in the session arena so an incompatible state cannot
     * leave the running game half-modified. */
    const u32 rollbackCapacity = core.stateSize();
    if (!rollbackCapacity || rollbackCapacity > MAX_STATE_BYTES) {
        RS_LOGW("save: state slot %d core reports state size %u", slot,
                unsigned(rollbackCapacity));
        return false;
    }
    if (rollbackCapacity != h.payloadSize)
        RS_LOGW("save: state slot %d size differs (file %u, core %u)", slot,
                unsigned(h.payloadSize), unsigned(rollbackCapacity));
    void* rollback = services->mem_alloc(rollbackCapacity, 16);
    if (!rollback) {
        RS_LOGE("save: no arena space for a %u-byte rollback",
                unsigned(rollbackCapacity));
        return false;
    }
    RS_LOGI("save: state slot %d capturing rollback (%u bytes)", slot,
            unsigned(rollbackCapacity));
    const int rollbackSize = core.stateSave(rollback, rollbackCapacity);
    if (rollbackSize <= 0 || u32(rollbackSize) > rollbackCapacity) {
        RS_LOGW("save: state slot %d rollback capture failed (%d)", slot,
                rollbackSize);
        services->mem_free(rollback);
        return false;
    }
    RS_LOGI("save: state slot %d applying %u-byte payload", slot,
            unsigned(h.payloadSize));
    const bool ok = core.stateLoad(file.bytes() + off, h.payloadSize);
    if (!ok && !core.stateLoad(rollback, u32(rollbackSize)))
        RS_LOGE("save: rollback failed after rejected state");
    services->mem_free(rollback);
    RS_LOGI("save: state slot %d load %s", slot, ok ? "ok" : "FAILED");
    return ok;
}

bool loadThumb(const db::GameEntry& game, int slot, u16* out) {
    if (slot < 0 || slot >= SLOTS || !out) return false;
    char path[160];
    statePath(path, sizeof path, game, slot);
    StateHeader h{};
    if (fs::readRange(path, &h, 0, sizeof h) != int(sizeof h) ||
        h.magic != STATE_MAGIC || h.thumbW != THUMB_W || h.thumbH != THUMB_H)
        return false;
    return fs::readRange(path, out, sizeof h, THUMB_W * THUMB_H * 2) ==
           THUMB_W * THUMB_H * 2;
}

namespace {
constexpr u32 MAX_RTC_BYTES = 4096;

bool saveRtc(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.rtcSize();
    const void* data = core.rtcData();
    if (!size || !data) return true;
    if (size > MAX_RTC_BYTES) {
        RS_LOGE("save: refusing oversized RTC (%u)", unsigned(size));
        return false;
    }
    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    std::snprintf(path, sizeof path, "%s/rtc.bin", dir);
    const bool ok = fs::writeFileAtomic(path, data, size);
    RS_LOGI("save: rtc %s (%u bytes)", ok ? "ok" : "FAILED",
            unsigned(size));
    return ok;
}

bool loadRtc(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.rtcSize();
    void* data = core.rtcData();
    if (!size || !data) return true;
    if (size > MAX_RTC_BYTES) return false;
    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    std::snprintf(path, sizeof path, "%s/rtc.bin", dir);
    const s32 onDisk = fs::fileSize(path);
    if (onDisk < 0) return true;
    if (onDisk != s32(size)) {
        RS_LOGW("save: RTC size mismatch (%d != %u)", int(onDisk),
                unsigned(size));
        return false;
    }
    const bool ok = fs::readRange(path, data, 0, size) == s32(size);
    RS_LOGI("save: rtc restore %s (%u bytes)", ok ? "ok" : "FAILED",
            unsigned(size));
    return ok;
}
}  // namespace

bool savePersistent(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.sramSize();
    void* data = core.sramData();
    bool sramOk = true;
    if (size && data && size <= MAX_SRAM_BYTES) {
        sramOk = writeSramSnapshot(game, data, size);
        RS_LOGI("save: sram %s (%u bytes)", sramOk ? "ok" : "FAILED",
                unsigned(size));
    } else if (size > MAX_SRAM_BYTES) {
        RS_LOGE("save: refusing oversized SRAM (%u)", unsigned(size));
        sramOk = false;
    } else if (size && !data) {
        RS_LOGE("save: core reports %u SRAM bytes with no buffer",
                unsigned(size));
        sramOk = false;
    }
    /* size == 0 is the ordinary "cartridge has no battery backup" answer,
     * and cores may still hand back a non-null pointer for it (PicoDrive
     * does). Treating that as an error failed the no_error_log release gate
     * on every game without a save chip. */
    const bool rtcOk = saveRtc(game, core);
    return sramOk && rtcOk;
}

bool writeSramSnapshot(const db::GameEntry& game, const void* data, u32 size) {
    if (!data || !size || size > MAX_SRAM_BYTES) return false;
    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    std::snprintf(path, sizeof path, "%s/sram.bin", dir);
    return fs::writeFileAtomic(path, data, size);
}

bool loadPersistent(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.sramSize();
    void* data = core.sramData();
    bool sramOk = true;
    /* size == 0 means the cartridge has no battery backup, which is normal
     * and not an error even when the core still returns a buffer pointer. */
    if (size) {
        if (!data || size > MAX_SRAM_BYTES) {
            RS_LOGE("save: invalid SRAM buffer (%u bytes)", unsigned(size));
            sramOk = false;
        } else {
            char dir[128], path[160];
            gameDir(dir, sizeof dir, game);
            std::snprintf(path, sizeof path, "%s/sram.bin", dir);
            const s32 onDisk = fs::fileSize(path);
            if (onDisk >= 0 && onDisk != s32(size)) {
                RS_LOGW("save: SRAM size mismatch (%d != %u)", int(onDisk),
                        unsigned(size));
                sramOk = false;
            } else if (onDisk >= 0) {
                /* Use the bounded core arena so a fragmented PSP-1000 heap
                 * cannot turn a save restore into an exception/termination.
                 * Copy only after the complete file has been read. */
                const RSHostAPI* services = host::table();
                void* temp = services->mem_alloc(size, 16);
                if (!temp) {
                    RS_LOGW("save: no core-arena space to restore %u-byte SRAM",
                            unsigned(size));
                    sramOk = false;
                } else {
                    const bool readOk =
                        fs::readRange(path, temp, 0, size) == s32(size);
                    if (readOk) {
                        std::memcpy(data, temp, size);
                        RS_LOGI("save: sram restored (%u bytes)",
                                unsigned(size));
                    } else {
                        RS_LOGW("save: SRAM read failed (%u bytes)",
                                unsigned(size));
                        sramOk = false;
                    }
                    services->mem_free(temp);
                }
            }
        }
    }

    /* SRAM and RTC are independent persistence regions. A stale SRAM file
     * must not prevent a valid clock record from being restored. */
    const bool rtcOk = loadRtc(game, core);
    return sramOk && rtcOk;
}

}  // namespace rs::save
