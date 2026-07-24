/** CoreManager — the frontend's only doorway to emulator cores.
 *
 * Hides how a core is delivered: as a relocatable PRX loaded on demand
 * (default; only one core occupies RAM at a time) or compiled into the
 * EBOOT (-DRS_STATIC_CORES=ON fallback). The frontend holds an
 * EmulatorCore adapter and never learns which loader produced it.
 *
 * PRX handshake: the frontend passes the address of a pointer slot as the
 * module-start argument; the core's module_start writes its RSCoreAPI
 * table address into that slot. No export lookup machinery needed.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/emulator_core.h"
#include "rs_common.h"

namespace rs {

class CoreManager {
public:
    /* Loads the module and adopts its API table. The caller must still run
     * core().initialize(host::table()) — after the memory arena is back in
     * place, since cores may allocate during init. On failure the manager
     * is left empty and error() explains why. */
    bool loadCore(const CoreInfo& info);
    void unloadCore();

#ifdef RS_STATIC_CORES
    /* The linked-in cores, for CoreRegistry::discover(). */
    static int staticCoreCount();
    static const RSCoreAPI* staticCoreApi(int i);
#endif

    bool loaded() const { return m_core.valid(); }
    EmulatorCore& core() { return m_core; }
    const char* error() const { return m_error; }

private:
    EmulatorCore m_core;
    int  m_moduleId = -1;      /* PRX SceUID, or -1 when static/unloaded */
    char m_error[160] = {};    /* roomy enough for a full module path */
};

}  // namespace rs
