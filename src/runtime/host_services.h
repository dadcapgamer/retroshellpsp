/** Builds the RSHostAPI table handed to emulator cores, backed by the
 * shared runtime services (arena, audio, log, filesystem, config). */
#pragma once

#include "core_api/rs_host_api.h"
#include "rs_common.h"

namespace rs::host {

/* The table lives for the whole app; `gameHash` scopes get_option lookups
 * to the running game and `inputState` mirrors the latest mapped pad. */
const RSHostAPI* table();

void setActiveGame(u32 pathHash);
void setInputState(u32 rsButtons);

}  // namespace rs::host
