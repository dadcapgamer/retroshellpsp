/** Builds the RSHostAPI table handed to emulator cores, backed by the
 * shared runtime services (arena, audio, log, filesystem, config). */
#pragma once

#include "core_api/rs_host_api.h"
#include "rs_common.h"

namespace rs::host {

/* The table lives for the whole app; `gameHash` scopes get_option lookups
 * to the running game and `inputState` mirrors the latest mapped pad. */
const RSHostAPI* table();

/* Resets the core-only recyclable allocation table around each module
 * lifetime. The underlying arena remains owned by runtime/arena. */
void beginCoreSession();
void endCoreSession();
u32 allocationFailures();

void setActiveGame(u32 pathHash);
void setInputState(u32 rsButtons);

}  // namespace rs::host
