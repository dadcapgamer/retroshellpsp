/** The systems RetroShell emulates: display names, legacy directory names,
 * accents and ROM extensions. Single source of truth used by the scanner,
 * the home UI and (Phase 3+) core selection.
 */
#pragma once

#include "rs_common.h"

namespace rs::db {

enum class System : u8 {
    GameBoy = 0,
    GameBoyColor,
    GBA,
    NES,
    SNES,
    Genesis,
    MasterSystem,
    GameGear,
    PCEngine,
    Count
};

struct SystemInfo {
    System id;
    const char* dirName;      /* legacy/category hint; not required by scanner */
    const char* displayName;
    const char* badge;        /* short label on the category tile */
    const char* coreId;       /* stable token used in core manifests */
    u32 accentRgb;
    const char* extensions;   /* pipe-separated, lower case, no dots */
};

inline const SystemInfo& systemInfo(System s) {
    static const SystemInfo TABLE[] = {
        {System::GameBoy,      "GameBoy",      "Game Boy",         "GB",
         "gb",   0x7BB661, "gb|dmg"},
        {System::GameBoyColor, "GameBoyColor", "Game Boy Color",   "GBC",
         "gbc",  0x9B72CF, "gbc|cgb"},
        {System::GBA,          "GBA",          "Game Boy Advance", "GBA",
         "gba",  0x5C67C7, "gba|agb"},
        {System::NES,          "NES",          "Nintendo",         "NES",
         "nes",  0xD9534F, "nes"},
        {System::SNES,         "SNES",         "Super Nintendo",   "SNES",
         "snes", 0x8E7CC3, "sfc|smc"},
        {System::Genesis,      "Genesis",      "Genesis",          "MD",
         "md",   0x4A90D9, "md|gen|smd|bin"},
        {System::MasterSystem, "MasterSystem", "Master System",    "SMS",
         "sms",  0xD64545, "sms"},
        {System::GameGear,     "GameGear",     "Game Gear",        "GG",
         "gg",   0x2FA4D9, "gg"},
        {System::PCEngine,     "PCEngine",     "PC Engine",        "PCE",
         "pce",  0xF2A03D, "pce"},
    };
    return TABLE[u8(s)];
}

constexpr int SYSTEM_COUNT = int(System::Count);

/* True if `ext` (lower case, no dot) appears in the pipe-separated list. */
bool extMatches(const char* list, const char* ext);

}  // namespace rs::db
