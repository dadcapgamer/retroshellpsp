/** Process-replacement launcher for proven standalone PSP emulators.
 *
 * A native backend receives argv[0] = its EBOOT and argv[1] = the selected
 * ROM. The emulator then owns the PSP exactly as its upstream build expects;
 * no RetroShell arena, audio thread, renderer, or frame scheduler remains.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/database/game_index.h"

namespace rs::nativeemu {

/* Does not return on success. Returns the negative PSP loadexec error when
 * custom firmware/firmware policy rejects the executable. */
int launch(const CoreInfo& core, const db::GameEntry& game);

/* Consumes the bounded receipt left by a native adapter when it chain-loads
 * RetroShell again. Safe to call on every boot after logging is available.
 * Reads and removes the last adapter session receipt. When that session
 * ended early, writes a short user-facing reason into `notice` (from the
 * adapter's own session log when it left one) and returns true. */
bool consumeReturnReceipt(char* notice, size_t size);

}  // namespace rs::nativeemu
