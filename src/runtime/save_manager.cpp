#include "runtime/save_manager.h"
#include "frontend/emulator_core.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace rs::save {

namespace {

constexpr u32 STATE_MAGIC   = 0x54535352u;  /* "RSST" */
constexpr u32 STATE_VERSION = 1;

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
            h.magic == STATE_MAGIC) {
            out[i].exists = true;
            out[i].payloadSize = h.payloadSize;
        }
    }
}

bool saveState(const db::GameEntry& game, EmulatorCore& core, int slot,
               const u16* thumb) {
    const u32 maxSize = core.stateSize();
    if (!maxSize) return false;

    std::vector<u8> payload(maxSize);
    const int used = core.stateSave(payload.data(), maxSize);
    if (used <= 0) {
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

    std::vector<u8> file;
    file.reserve(sizeof h + THUMB_W * THUMB_H * 2 + size_t(used));
    file.insert(file.end(), reinterpret_cast<u8*>(&h),
                reinterpret_cast<u8*>(&h) + sizeof h);
    if (thumb)
        file.insert(file.end(), reinterpret_cast<const u8*>(thumb),
                    reinterpret_cast<const u8*>(thumb) +
                        THUMB_W * THUMB_H * 2);
    file.insert(file.end(), payload.begin(), payload.begin() + used);

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    statePath(path, sizeof path, game, slot);
    const bool ok = fs::writeFile(path, file.data(), u32(file.size()));
    RS_LOGI("save: state slot %d %s (%d bytes)", slot, ok ? "ok" : "FAILED",
            used);
    return ok;
}

bool loadState(const db::GameEntry& game, EmulatorCore& core, int slot) {
    char path[160];
    statePath(path, sizeof path, game, slot);
    std::vector<u8> file;
    if (!fs::readFile(path, file) || file.size() < sizeof(StateHeader))
        return false;

    StateHeader h{};
    std::memcpy(&h, file.data(), sizeof h);
    if (h.magic != STATE_MAGIC || h.version != STATE_VERSION) return false;
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
    const bool ok = core.stateLoad(file.data() + off, h.payloadSize);
    RS_LOGI("save: state slot %d load %s", slot, ok ? "ok" : "FAILED");
    return ok;
}

bool loadThumb(const db::GameEntry& game, int slot, u16* out) {
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

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    fs::mkdirs(dir);
    std::snprintf(path, sizeof path, "%s/sram.bin", dir);
    const bool ok = fs::writeFile(path, data, size);
    RS_LOGI("save: sram %s (%u bytes)", ok ? "ok" : "FAILED", unsigned(size));
    return ok;
}

bool loadSram(const db::GameEntry& game, EmulatorCore& core) {
    const u32 size = core.sramSize();
    void* data = core.sramData();
    if (!size || !data) return true;

    char dir[128], path[160];
    gameDir(dir, sizeof dir, game);
    std::snprintf(path, sizeof path, "%s/sram.bin", dir);
    const s32 got = fs::readRange(path, data, 0, size);
    return got == s32(size);
}

}  // namespace rs::save
