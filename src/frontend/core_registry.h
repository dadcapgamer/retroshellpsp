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
 *   3. the UI exposes the installed alternatives through the Square
 *      shortcut. needsChoice() is reserved for a remembered core that has
 *      disappeared, avoiding a silent cross-core save-state substitution.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "frontend/database/systems.h"

#include <string>
#include <vector>

namespace rs {

struct CoreInfo {
    std::string name;      /* module / manifest name, e.g. "gambatte" */
    std::string version;
    std::string systems;   /* pipe-separated coreIds, e.g. "gb|gbc"   */
    int priority = 0;      /* higher sorts first and becomes the default */
    bool testOnly = false; /* never exposed in a production build */
    bool psp1000Safe = false; /* explicitly qualified for the 32 MB model */
    bool requiresFullContent = false; /* core cannot use host VFS/fullpath */
    bool preferVfs = false; /* avoid a duplicate in-arena ROM when possible */
    bool isStatic = false;

    bool serves(db::System s) const;
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

    /* The core a plain launch would use, or nullptr when no installed core
     * claims the game's system. */
    const CoreInfo* resolve(const db::GameEntry& game) const;

    /* True when launching should first ask the user which core to use. */
    bool needsChoice(const db::GameEntry& game) const;

private:
    std::vector<CoreInfo> m_cores;
};

}  // namespace rs
