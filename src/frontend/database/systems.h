/** The systems RetroSuite emulates: directory layout, display names,
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
    const char* dirName;      /* under ms0:/ROMS/ */
    const char* displayName;
    const char* badge;        /* short label on the category tile */
    u32 accentRgb;
    const char* extensions;   /* pipe-separated, lower case, no dots */
};

inline const SystemInfo& systemInfo(System s) {
    static const SystemInfo TABLE[] = {
        {System::GameBoy,      "GameBoy",      "Game Boy",         "GB",
         0x7BB661, "gb|dmg"},
        {System::GameBoyColor, "GameBoyColor", "Game Boy Color",   "GBC",
         0x9B72CF, "gbc|cgb"},
        {System::GBA,          "GBA",          "Game Boy Advance", "GBA",
         0x5C67C7, "gba|agb"},
        {System::NES,          "NES",          "Nintendo",         "NES",
         0xD9534F, "nes"},
        {System::SNES,         "SNES",         "Super Nintendo",   "SNES",
         0x8E7CC3, "sfc|smc"},
        {System::Genesis,      "Genesis",      "Genesis",          "MD",
         0x4A90D9, "md|gen|smd|bin"},
        {System::MasterSystem, "MasterSystem", "Master System",    "SMS",
         0xD64545, "sms"},
        {System::GameGear,     "GameGear",     "Game Gear",        "GG",
         0x2FA4D9, "gg"},
        {System::PCEngine,     "PCEngine",     "PC Engine",        "PCE",
         0xF2A03D, "pce"},
    };
    return TABLE[u8(s)];
}

constexpr int SYSTEM_COUNT = int(System::Count);

/* True if `ext` (lower case, no dot) appears in the pipe-separated list. */
bool extMatches(const char* list, const char* ext);

}  // namespace rs::db
