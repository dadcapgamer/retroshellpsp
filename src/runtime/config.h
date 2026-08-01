/** Global configuration (ms0:/RETROSHELL/config.json) plus per-game
 * option overlays (ms0:/RETROSHELL/pergame/<hash>.json).
 *
 * Per-game files carry free-form string options resolved by cores through
 * RSHostAPI::get_option in Phase 3+.
 */
#pragma once

#include "rs_common.h"

#include <string>
#include <utility>
#include <vector>

namespace rs::cfg {

struct Config {
    std::string theme  = "dark";    /* "dark", "light" or a theme dir name */
    int  accent        = 0;         /* index into theme::accentOption */
    int  cpuMenuMhz    = 222;
    int  cpuGameMhz    = 333;
    bool uiSounds      = true;
    bool clock24Hour   = false;
    bool showFps       = false;
    bool autosave      = true;
    bool psp1000SafeMode = true;
    bool psp1000SafeModeConfigured = false; /* runtime metadata, not a setting */
};

Config& get();
void load();
void save();
void applyHardwareDefaults(bool isPsp1000);

/* Per-game overlays. Values persist immediately on set. */
std::string gameOption(u32 pathHash, const char* key);
void setGameOption(u32 pathHash, const char* key, const char* value);

}  // namespace rs::cfg
