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
    /* Retired by the Astra redesign: Home has one layout now. The field is
     * still read and written so existing config.json files round-trip. */
    HOME_LAYOUT_MODERN = 0,  /* beta.3 text rail: icon + label, no panels */
    HOME_LAYOUT_CLASSIC = 1, /* pre-beta.3: 50px badge cards and a recent shelf */
    HOME_LAYOUT_COUNT
};

struct Config {
    std::string theme  = "dark";    /* a built-in id or a theme dir name */
    int  accent        = 0;         /* index into theme::accentOption */
    int  cpuMenuMhz    = 222;
    int  cpuGameMhz    = 333;
    bool uiSounds      = true;
    bool clock24Hour   = false;
    bool showFps       = false;
    bool autosave      = true;
    /* Text-only mode: the Library collapses to full-width rows with no
     * preview column and no artwork is decoded. */
    bool showArt       = true;
    /* Home screen presentation. Classic is the pre-beta.3 card rail, kept
     * because some users prefer its denser, more console-like shelf. */
    int  homeLayout    = HOME_LAYOUT_MODERN;
    /* First-run setup (scan, confirm systems, confirm emulators) has been
     * completed or skipped. A config.json written before this key existed
     * counts as done: those users are not new. */
    bool setupDone     = false;
    /* Per-system default emulator: db::SystemInfo::coreId -> core name.
     * A game's own remembered core still wins, because save states are
     * core-specific. */
    std::vector<std::pair<std::string, std::string>> systemCores;
    /* Systems the user turned off in Settings → Systems (coreIds). Their
     * games stay indexed, but Home does not show them. */
    std::vector<std::string> disabledSystems;
};

Config& get();

/* Default-emulator override for a system, or "" for "automatic". */
std::string systemCore(const char* coreId);
void setSystemCore(const char* coreId, const char* coreName);   /* "" clears */
bool systemEnabled(const char* coreId);
void setSystemEnabled(const char* coreId, bool enabled);
void load();
void save();

/* Per-game overlays. Values persist immediately on set; "" clears. */
std::string gameOption(u32 pathHash, const char* key);
void setGameOption(u32 pathHash, const char* key, const char* value);

/* Per-emulator defaults (RETROSHELL/percore/<core>.json), shared by every
 * game that core runs. Values persist immediately on set. */
std::string coreOption(const char* core, const char* key);
void setCoreOption(const char* core, const char* key, const char* value);

/* A game setting as it applies: the game's own value, else its
 * emulator's, else "" (the built-in default). */
std::string option(u32 pathHash, const char* core, const char* key);

/* Per-game key: "game" while a game keeps its own settings instead of
 * following its emulator's ("This game only" in Emulator Settings). */
constexpr const char* SCOPE_KEY = "rs_scope";
bool gameScoped(u32 pathHash);

/* Stores a changed setting where it belongs: the game's own copy while it
 * is game-scoped, otherwise the emulator's (clearing any older per-game
 * value that would shadow it). */
void storeOption(u32 pathHash, const char* core, const char* key,
                 const char* value);

}  // namespace rs::cfg
