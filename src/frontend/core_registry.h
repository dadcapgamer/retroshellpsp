/** CoreRegistry — which emulator cores are installed, and which one runs a
 * given game.
 *
 * PRX builds discover cores at boot from manifest files the core drops
 * next to its module:
 *
 *     ms0:/RETROSHELL/cores/gambatte.prx
 *     ms0:/RETROSHELL/cores/gambatte.json
 *       { "name": "gambatte", "version": "0.5.0", "systems": "gb|gbc" }
 *
 * The manifest exists so the frontend never has to load a module just to
 * list it. `systems` uses the short ids from SystemInfo::coreId. Static
 * builds skip manifests entirely and read the same fields from the linked
 * cores' API tables.
 *
 * Core resolution for a launch ("adaptive core step"):
 *   1. the core remembered for this game (per-game "core" option) — save
 *      states are core-specific, so a game sticks with the core that made
 *      them;
 *   2. otherwise, the highest-priority core claiming the game's system;
 *   3. the UI exposes installed alternatives through Options -> Per-game
 *      Settings. needsChoice() is reserved for a remembered core that has
 *      disappeared, avoiding a silent cross-core save-state substitution.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "frontend/database/systems.h"

#include <string>
#include <vector>

namespace rs {

enum class CoreBackend : uint8_t {
    InProcessPrx,
    NativeEboot,
};

struct CoreInfo {
    std::string name;      /* module / manifest name, e.g. "gambatte" */
    std::string version;
    std::string systems;   /* pipe-separated coreIds, e.g. "gb|gbc"   */
    int priority = 0;      /* higher sorts first and becomes the default */
    bool testOnly = false; /* visible, but labelled as testing in the picker */
    bool psp1000Safe = false; /* explicitly qualified for the 32 MB model */
    bool requiresFullContent = false; /* core cannot use host VFS/fullpath */
    bool preferVfs = false; /* avoid a duplicate in-arena ROM when possible */
    bool isStatic = false;
    CoreBackend backend = CoreBackend::InProcessPrx;
    /* Native backends are process replacements. This path is relative to
     * RETROSHELL and constrained to emulators/<core-name>/EBOOT.PBP. */
    std::string executable;
    uint32_t maxRomPath = 767;
    std::string biosSource;
    std::string biosDestination;
    uint32_t biosBytes = 0;
    /* Working directories the native emulator expects to already exist.
     * A zip cannot carry empty directories, so the frontend creates them
     * before handing the machine over. */
    std::vector<std::string> requiredDirectories;
    /* Native adapter contract. Version zero is a legacy process replacement
     * that may ignore RetroShell's optional return/session arguments. */
    uint32_t adapterProtocol = 0;
    std::string pauseMode;
    std::string pauseHotkey;
    std::string returnMode;

    bool serves(db::System s) const;
    bool isNative() const { return backend == CoreBackend::NativeEboot; }
};

class CoreRegistry {
public:
    void discover();

    const std::vector<CoreInfo>& all() const { return m_cores; }
    const CoreInfo* find(const char* name) const;

    std::vector<const CoreInfo*> coresFor(db::System s) const;
    int countFor(db::System s) const;          /* no allocation */

    /* Whether this system offers a real alternative — the registry's one
     * definition of "multiple cores", shared by the picker and the UI. */
    bool hasChoice(db::System s) const { return countFor(s) >= 2; }

    /* The core a plain launch would use: the game's own choice when it has
     * one, otherwise the system default. nullptr when no installed core
     * claims the game's system. */
    const CoreInfo* resolve(const db::GameEntry& game) const;
    /* The game's own emulator choice (Game Details > Emulator), or nullptr
     * when it follows the system default. Reads the per-game config. */
    const CoreInfo* overrideFor(const db::GameEntry& game) const;
    /* Sets the game's own choice; nullptr returns it to the system default. */
    static void setOverride(const db::GameEntry& game, const CoreInfo* core);
    /* Deterministic system default with no per-game Memory Stick lookup. */
    const CoreInfo* defaultFor(db::System system) const;

    /* True when launching should first ask the user which core to use. */
    bool needsChoice(const db::GameEntry& game) const;

private:
    std::vector<CoreInfo> m_cores;
};

}  // namespace rs
