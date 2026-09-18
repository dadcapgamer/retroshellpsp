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

enum HomeLayout {
    HOME_LAYOUT_MODERN = 0,  /* beta.3 text rail: icon + label, no panels */
    HOME_LAYOUT_CLASSIC = 1, /* pre-beta.3: 50px badge cards and a recent shelf */
    HOME_LAYOUT_COUNT
};

struct Config {
    std::string theme  = "dark";    /* "dark", "light" or a theme dir name */
    int  accent        = 0;         /* index into theme::accentOption */
    int  cpuMenuMhz    = 222;
    int  cpuGameMhz    = 333;
    bool uiSounds      = true;
    bool clock24Hour   = false;
    bool showFps       = false;
    bool autosave      = true;
    /* Home screen presentation. Classic is the pre-beta.3 card rail, kept
     * because some users prefer its denser, more console-like shelf. */
    int  homeLayout    = HOME_LAYOUT_MODERN;
};

Config& get();
void load();
void save();

/* Per-game overlays. Values persist immediately on set. */
std::string gameOption(u32 pathHash, const char* key);
void setGameOption(u32 pathHash, const char* key, const char* value);

}  // namespace rs::cfg
