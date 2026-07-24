/** C++ adapter over the C core table — the interface the rest of the
 * frontend programs against. Owns nothing; the CoreManager controls the
 * underlying module's lifetime.
 */
#pragma once

#include "core_api/rs_core_api.h"
#include "rs_common.h"

#include <string>

namespace rs {

class EmulatorCore {
public:
    EmulatorCore() = default;
    explicit EmulatorCore(const RSCoreAPI* api) : m_api(api) {}

    bool valid() const { return m_api != nullptr; }
    const char* name() const    { return m_api ? m_api->name : "?"; }
    const char* version() const { return m_api ? m_api->version : "?"; }
    double fps() const {
        return (m_api && m_api->fps > 1.0) ? m_api->fps : 60.0;
    }

    bool initialize(const RSHostAPI* host) {
        return m_api && m_api->init(host) == 0;
    }
    void shutdown() {
        if (m_api) m_api->shutdown();
    }

    bool loadROM(const char* path, const void* data, u32 size) {
        return m_api && m_api->load_rom(path, data, size) == 0;
    }
    void unloadROM() {
        if (m_api) m_api->unload_rom();
    }
    void reset() {
        if (m_api) m_api->reset();
    }
    void runFrame(u32 buttons) {
        if (m_api) m_api->run_frame(buttons);
    }
    RSVideoFrame frame() const {
        return m_api ? m_api->get_frame() : RSVideoFrame{};
    }

    u32  stateSize() const { return m_api ? m_api->state_size() : 0; }
    int  stateSave(void* buf, u32 size) {
        return m_api ? m_api->state_save(buf, size) : -1;
    }
    bool stateLoad(const void* buf, u32 size) {
        return m_api && m_api->state_load(buf, size) == 0;
    }

    u32   sramSize() const  { return m_api ? m_api->sram_size() : 0; }
    void* sramData() const  { return m_api ? m_api->sram_data() : nullptr; }
    bool  sramDirty() const { return m_api && m_api->sram_dirty() != 0; }

    bool setOption(const char* key, const char* value) {
        return m_api && m_api->set_option(key, value) == 0;
    }

private:
    const RSCoreAPI* m_api = nullptr;
};

}  // namespace rs
