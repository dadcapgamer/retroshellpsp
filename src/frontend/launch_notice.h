/** Turns a raw launch failure ("rom missing", "core init failed", ...) into
 * the actionable notice the user sees: what went wrong in plain words and,
 * where one exists, the single action that can fix it.
 *
 * The raw strings come from App::launchGame's preflight, GameSession and the
 * native launcher; classification is a substring match so the many existing
 * call sites keep their short diagnostic text for the log. Pure and
 * header-only so the host tests can pin the wording rules.
 */
#pragma once

#include <cstring>
#include <string>

namespace rs::launch {

enum class Kind : unsigned char {
    Generic, RomMissing, StorageUnavailable, CoreMissing, BiosMissing,
    CoreRejected, OutOfMemory, PathInvalid, ArchiveInvalid,
};

/* What the triangle button offers on the failure screen. */
enum class Action : unsigned char { None, Rescan, ChooseCore };

struct Notice {
    Kind kind = Kind::Generic;
    const char* title = "Unable to launch";
    const char* message = "";
    Action action = Action::None;
};

inline bool has(const char* haystack, const char* needle) {
    return std::strstr(haystack, needle) != nullptr;
}

inline Notice classify(const char* raw) {
    Notice n;
    if (!raw) raw = "";
    if (has(raw, "storage unavailable")) {
        n.kind = Kind::StorageUnavailable;
        n.title = "Memory Stick unavailable";
        n.message = "RetroShell cannot read the Memory Stick. Check that it "
                    "is inserted and not locked, then try again.";
    } else if (has(raw, "rom missing")) {
        n.kind = Kind::RomMissing;
        n.title = "ROM file not found";
        n.message = "This game was moved or deleted since the last scan. "
                    "Rescan to refresh the library.";
        n.action = Action::Rescan;
    } else if (has(raw, "not installed")) {
        n.kind = Kind::CoreMissing;
        n.title = "Emulator not installed";
        n.message = "RetroShell could not find the emulator configured for "
                    "this system. Install a core package, or pick another "
                    "emulator.";
        n.action = Action::ChooseCore;
    } else if (has(raw, "BIOS") || has(raw, "system/")) {
        n.kind = Kind::BiosMissing;
        n.title = "BIOS required";
        n.message = "This emulator needs a BIOS file that is missing or the "
                    "wrong size. Copy it into ms0:/RETROSHELL/system.";
    } else if (has(raw, "path is too long") || has(raw, "too long")) {
        n.kind = Kind::PathInvalid;
        n.title = "File path too long";
        n.message = "This emulator cannot open a path this long. Move the "
                    "ROM closer to ms0:/ROMS and rescan.";
        n.action = Action::Rescan;
    } else if (has(raw, "exceeds")) {
        n.kind = Kind::OutOfMemory;
        n.title = "Game too large";
        n.message = "This game does not fit in memory with the selected "
                    "emulator. Try another emulator for this system.";
        n.action = Action::ChooseCore;
    } else if (has(raw, "zip")) {
        n.kind = Kind::ArchiveInvalid;
        n.title = "Archive could not be read";
        n.message = "The ZIP is damaged or has no supported ROM inside. "
                    "Re-copy it, or extract the ROM beside it.";
    } else if (has(raw, "rejected") || has(raw, "init failed")) {
        n.kind = Kind::CoreRejected;
        n.title = "Emulator could not start this game";
        n.message = "The emulator refused this ROM. It may be damaged, "
                    "headerless or unsupported. Another emulator may work.";
        n.action = Action::ChooseCore;
    } else {
        n.message = "RetroShell could not start this game. Check the log "
                    "in ms0:/RETROSHELL for details.";
    }
    return n;
}

inline const char* actionLabel(Action a) {
    switch (a) {
        case Action::Rescan:     return "Rescan Library";
        case Action::ChooseCore: return "Choose Emulator";
        default:                 return "";
    }
}

}  // namespace rs::launch
