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

void gameDir(char* buf, size_t n, const db::GameEntry& g) {
    std::snprintf(buf, n, "%s/saves/%s/%08x", fs::ROOT,
                  db::systemInfo(g.system).dirName, unsigned(g.pathHash));
}

void statePath(char* buf, size_t n, const db::GameEntry& g, int slot) {
    char dir[128];
    gameDir(dir, sizeof dir, g);
    std::snprintf(buf, n, "%s/state%d.rst", dir, slot);
}

}  // namespace

void querySlots(const db::GameEntry& game, SlotInfo out[SLOTS]) {
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
        }
    }
}

bool saveState(const db::GameEntry& game, EmulatorCore& core, int slot,
               const u16* thumb) {
    if (slot < 0 || slot >= SLOTS) return false;
    const u32 maxSize = core.stateSize();
    if (!maxSize || maxSize > MAX_STATE_BYTES) return false;

    const u32 thumbBytes = thumb ? THUMB_W * THUMB_H * 2u : 0u;
    std::vector<u8> file(sizeof(StateHeader) + thumbBytes + maxSize);
    u8* payload = file.data() + sizeof(StateHeader) + thumbBytes;
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
        std::memcpy(file.data() + sizeof h, thumb, thumbBytes);
    file.resize(sizeof h + thumbBytes + u32(used));

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    statePath(path, sizeof path, game, slot);
    const bool ok = fs::writeFileAtomic(path, file.data(), u32(file.size()));
    RS_LOGI("save: state slot %d %s (%d bytes)", slot, ok ? "ok" : "FAILED",
            used);
    return ok;
}

bool loadState(const db::GameEntry& game, EmulatorCore& core, int slot) {
    if (slot < 0 || slot >= SLOTS) return false;
    char path[160];
    statePath(path, sizeof path, game, slot);
    std::vector<u8> file;
    if (!fs::readFile(path, file, MAX_STATE_BYTES + 64u * 1024u) ||
        file.size() < sizeof(StateHeader))
        return false;

    StateHeader h{};
    std::memcpy(&h, file.data(), sizeof h);
    if (h.magic != STATE_MAGIC || h.version != STATE_VERSION) return false;
    if (!h.payloadSize || h.payloadSize > MAX_STATE_BYTES) return false;
    if (!((h.thumbW == 0 && h.thumbH == 0) ||
          (h.thumbW == THUMB_W && h.thumbH == THUMB_H)))
        return false;
    h.coreName[sizeof h.coreName - 1] = 0;   /* a corrupt field may lack NUL */
    if (std::strncmp(h.coreName, core.name(), sizeof h.coreName) != 0) {
        RS_LOGW("save: state belongs to core '%s'", h.coreName);
        return false;
    }
    /* Validate header fields with overflow-safe arithmetic: payloadSize and
     * thumb dimensions come straight from the file and a corrupt state must
     * not wrap the bounds check into an out-of-bounds read. */
    const size_t total = file.size();
    const size_t thumbBytes = size_t(h.thumbW) * h.thumbH * 2;
    const size_t off = sizeof h + thumbBytes;
    if (off < sizeof h || off > total) return false;          /* thumb overflow */
    if (h.payloadSize > total - off) return false;            /* payload overflow */
    /* A rejecting core may already have consumed part of the input. Keep a
     * rollback snapshot in the session arena so an incompatible state cannot
     * leave the running game half-modified. */
    const u32 rollbackCapacity = core.stateSize();
    if (!rollbackCapacity || rollbackCapacity > MAX_STATE_BYTES) return false;
    const RSHostAPI* services = host::table();
    void* rollback = services->mem_alloc(rollbackCapacity, 16);
    if (!rollback) return false;
    const int rollbackSize = core.stateSave(rollback, rollbackCapacity);
    if (rollbackSize <= 0 || u32(rollbackSize) > rollbackCapacity) {
        services->mem_free(rollback);
        return false;
    }
    const bool ok = core.stateLoad(file.data() + off, h.payloadSize);
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

bool saveSram(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.sramSize();
    void* data = core.sramData();
    if (!size || !data) return true;   /* game has no battery save */
    if (size > MAX_SRAM_BYTES) {
        RS_LOGE("save: refusing oversized SRAM (%u)", unsigned(size));
        return false;
    }

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    std::snprintf(path, sizeof path, "%s/sram.bin", dir);
    const bool ok = fs::writeFileAtomic(path, data, size);
    RS_LOGI("save: sram %s (%u bytes)", ok ? "ok" : "FAILED", unsigned(size));
    return ok;
}

bool loadSram(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.sramSize();
    void* data = core.sramData();
    if (!size || !data) return true;
    if (size > MAX_SRAM_BYTES) return false;

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    std::snprintf(path, sizeof path, "%s/sram.bin", dir);
    const s32 onDisk = fs::fileSize(path);
    if (onDisk < 0) return true;  /* first run */
    if (onDisk != s32(size)) {
        RS_LOGW("save: SRAM size mismatch (%d != %u)", int(onDisk),
                unsigned(size));
        return false;
    }
    std::vector<u8> temp;
    if (!fs::readFile(path, temp, MAX_SRAM_BYTES) || temp.size() != size)
        return false;
    std::memcpy(data, temp.data(), size);
    return true;
}

}  // namespace rs::save
