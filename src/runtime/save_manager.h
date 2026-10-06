/** Save manager: battery SRAM and save states for the running game.
 *
 * Layout: ms0:/RETROSHELL/saves/<SystemDir>/<pathHash>/
 *   sram.bin              battery save, flushed on pause/exit and dirty
 *   rtc.bin               battery-backed cartridge clock/register data
 *   state<N>.rst          save state, N in 0..SLOTS-1
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
};

/* Reads slot headers for the menu (cheap: header only). */
void querySlots(const db::GameEntry& game, SlotInfo out[SLOTS]);

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

bool savePersistent(const db::GameEntry& game, EmulatorCore& core);
bool loadPersistent(const db::GameEntry& game, EmulatorCore& core);
/* Writes an already-copied SRAM image. The caller owns `data` until this
 * returns; GameSession uses this on its joined Memory Stick worker. */
bool writeSramSnapshot(const db::GameEntry& game, const void* data, u32 size);

}  // namespace rs::save
