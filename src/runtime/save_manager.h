/** Save manager: battery SRAM and save states for the running game.
 *
 * Layout: ms0:/RETROSHELL/saves/<SystemDir>/<pathHash>/
 *   sram.bin              battery save, flushed on pause/exit and dirty
 *   rtc.bin               battery-backed cartridge clock/register data
 *   state<N>.rst          save state, N in 0..SLOTS-1
 *   state<N>.prv          full-size preview of that state (optional):
 *                         u32 magic "RSPV", u16 w, u16 h, w*h RGB565
 *
 * .rst layout (little endian):
 *   u32 magic "RSST", u32 version
 *   char coreName[16], coreVersion[16]
 *   u32 payloadSize
 *   u16 thumbW, thumbH                      (RGB565 thumbnail follows)
 *   thumbnail pixels, then payload
 */
#pragma once

#include "frontend/database/game_index.h"
#include "rs_common.h"

#include <vector>

namespace rs {
class EmulatorCore;
}

namespace rs::save {

constexpr int SLOTS = 5;
constexpr int THUMB_W = 96;
constexpr int THUMB_H = 54;

struct SlotInfo {
    bool exists = false;
    u32  payloadSize = 0;
    u64  stamp = 0;          /* YYYYMMDDHHMM local, 0 when unknown */
    char coreName[16] = {};  /* emulator that wrote the state */
};

/* Largest preview kept: covers every supported core's native frame. */
constexpr int PREVIEW_MAX_W = 512;
constexpr int PREVIEW_MAX_H = 512;

/* Reads slot headers for the menu (cheap: header only, plus one directory
 * listing for the timestamps). */
void querySlots(const db::GameEntry& game, SlotInfo out[SLOTS]);
/* Removes one slot: its state, preview and their .bak twins. */
bool deleteState(const db::GameEntry& game, int slot);

/* True when the game has any battery save, RTC data or save state. */
bool hasAnySave(const db::GameEntry& game);
/* "Delete Save Data": removes sram.bin, rtc.bin and every state slot plus
 * their atomic-write .bak twins (readFile would otherwise resurrect a save
 * from the backup). Returns the number of files removed. */
int deleteAll(const db::GameEntry& game);

/* Thumbnail is optional RGB565 THUMB_W x THUMB_H. */
bool saveState(const db::GameEntry& game, EmulatorCore& core, int slot,
               const u16* thumb);
bool loadState(const db::GameEntry& game, EmulatorCore& core, int slot);
/* Reads just the thumbnail; returns false if the slot is empty. */
bool loadThumb(const db::GameEntry& game, int slot, u16* out);
/* Full-size RGB565 preview written beside a state (the game frame at its
 * native resolution). Older states have none; loadPreview then fails and
 * callers fall back to the header thumbnail. */
bool savePreview(const db::GameEntry& game, int slot, const u16* pixels, int w,
                 int h);
bool loadPreview(const db::GameEntry& game, int slot, std::vector<u16>& out,
                 int& w, int& h);
void dropPreview(const db::GameEntry& game, int slot);

bool savePersistent(const db::GameEntry& game, EmulatorCore& core);
bool loadPersistent(const db::GameEntry& game, EmulatorCore& core);
/* Writes an already-copied SRAM image. The caller owns `data` until this
 * returns; GameSession uses this on its joined Memory Stick worker. */
bool writeSramSnapshot(const db::GameEntry& game, const void* data, u32 size);

}  // namespace rs::save
