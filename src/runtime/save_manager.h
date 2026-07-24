/** Save manager: battery SRAM and save states for the running game.
 *
 * Layout: ms0:/RETROSUITE/saves/<SystemDir>/<pathHash>/
 *   sram.bin              battery save, flushed on pause/exit and dirty
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

/* Thumbnail is optional RGB565 THUMB_W x THUMB_H. */
bool saveState(const db::GameEntry& game, EmulatorCore& core, int slot,
               const u16* thumb);
bool loadState(const db::GameEntry& game, EmulatorCore& core, int slot);
/* Reads just the thumbnail; returns false if the slot is empty. */
bool loadThumb(const db::GameEntry& game, int slot, u16* out);

bool saveSram(const db::GameEntry& game, EmulatorCore& core);
bool loadSram(const db::GameEntry& game, EmulatorCore& core);

}  // namespace rs::save
