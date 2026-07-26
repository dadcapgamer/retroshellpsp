#include "frontend/core_registry.h"

#include "runtime/config.h"
#include "runtime/log.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>

#ifdef RS_STATIC_CORES
#include "frontend/core_manager.h"
#else
#include "platform/psp/fs_psp.h"
#include "runtime/jsonfile.h"
#endif

namespace rs {

namespace {
bool safeCoreName(const char* s) {
    if (!s || !*s || std::strlen(s) > 48) return false;
    for (; *s; ++s) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

bool knownSystems(const char* systems) {
    if (!systems || !*systems || std::strlen(systems) > 64) return false;
    const char* p = systems;
    while (*p) {
        const char* end = std::strchr(p, '|');
        const size_t len = end ? size_t(end - p) : std::strlen(p);
        bool known = false;
        for (int i = 0; i < db::SYSTEM_COUNT; ++i) {
            const char* id = db::systemInfo(db::System(i)).coreId;
            if (std::strlen(id) == len && std::strncmp(id, p, len) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
        if (!end) break;
        p = end + 1;
    }
    return true;
}

void sortCores(std::vector<CoreInfo>& cores) {
    std::sort(cores.begin(), cores.end(),
              [](const CoreInfo& a, const CoreInfo& b) {
                  if (a.priority != b.priority)
                      return a.priority > b.priority;
                  return a.name < b.name;
              });
}
}  // namespace

bool CoreInfo::serves(db::System s) const {
    return db::extMatches(systems.c_str(), db::systemInfo(s).coreId);
}

const CoreInfo* CoreRegistry::find(const char* name) const {
    for (const auto& c : m_cores)
        if (c.name == name) return &c;
    return nullptr;
}

std::vector<const CoreInfo*> CoreRegistry::coresFor(db::System s) const {
    std::vector<const CoreInfo*> out;
    for (const auto& c : m_cores)
        if (c.serves(s)) out.push_back(&c);
    return out;
}

int CoreRegistry::countFor(db::System s) const {
    int n = 0;
    for (const auto& c : m_cores)
        if (c.serves(s)) n++;
    return n;
}

const CoreInfo* CoreRegistry::resolve(const db::GameEntry& game) const {
    const std::string remembered = cfg::gameOption(game.pathHash, "core");
    if (!remembered.empty()) {
        const CoreInfo* c = find(remembered.c_str());
        if (c && c->serves(game.system)) return c;
        /* A remembered core that vanished falls through to the default. */
    }
    for (const auto& c : m_cores)
        if (c.serves(game.system)) return &c;
    return nullptr;
}

bool CoreRegistry::needsChoice(const db::GameEntry& game) const {
    const std::string remembered = cfg::gameOption(game.pathHash, "core");
    if (remembered.empty()) return false; /* deterministic default */
    /* A remembered core that was since uninstalled must re-prompt rather
     * than let resolve() silently substitute a different core (whose save
     * states wouldn't match). */
    const CoreInfo* c = find(remembered.c_str());
    return !(c && c->serves(game.system));
}

#ifdef RS_STATIC_CORES

void CoreRegistry::discover() {
    m_cores.clear();
    for (int i = 0; i < CoreManager::staticCoreCount(); i++) {
        const RSCoreAPI* api = CoreManager::staticCoreApi(i);
        if (!api || api->api_version != RS_CORE_API_VERSION) continue;
        m_cores.push_back({api->name, api->version, api->systems,
                           0, false, true, true});
    }
    sortCores(m_cores);
    RS_LOGI("cores: %d linked in", int(m_cores.size()));
}

#else

void CoreRegistry::discover() {
    m_cores.clear();

    char dir[64];
    std::snprintf(dir, sizeof dir, "%s/cores", fs::ROOT);
    std::vector<fs::DirEntry> entries;
    if (!fs::listDir(dir, entries)) {
        RS_LOGW("cores: %s missing", dir);
        return;
    }

    for (const auto& e : entries) {
        const size_t dot = e.name.rfind(".json");
        if (e.isDir || dot == std::string::npos ||
            dot + 5 != e.name.size())
            continue;

        char path[128];
        std::snprintf(path, sizeof path, "%s/%s", dir, e.name.c_str());
        cJSON* root = json::parseFile(path);
        if (!root) {
            RS_LOGW("cores: %s unreadable or not valid JSON", e.name.c_str());
            continue;
        }

        const cJSON* name = cJSON_GetObjectItem(root, "name");
        const cJSON* ver = cJSON_GetObjectItem(root, "version");
        const cJSON* systems = cJSON_GetObjectItem(root, "systems");
        const cJSON* priority = cJSON_GetObjectItem(root, "priority");
        const cJSON* testOnly = cJSON_GetObjectItem(root, "testOnly");
        const cJSON* requiresFullContent =
            cJSON_GetObjectItem(root, "requiresFullContent");
        if (cJSON_IsString(name) && cJSON_IsString(systems) &&
            safeCoreName(name->valuestring) &&
            knownSystems(systems->valuestring)) {
            const bool isTest = cJSON_IsTrue(testOnly);
#ifndef RS_INCLUDE_TEST_CORES
            if (isTest) {
                cJSON_Delete(root);
                continue;
            }
#endif
            /* The module itself must be present, not just its manifest. */
            std::snprintf(path, sizeof path, "%s/%s.prx", dir,
                          name->valuestring);
            if (fs::exists(path)) {
                m_cores.push_back({name->valuestring,
                                   cJSON_IsString(ver) ? ver->valuestring : "",
                                   systems->valuestring,
                                   cJSON_IsNumber(priority)
                                       ? rsClamp(priority->valueint, -1000, 1000)
                                       : 0,
                                   isTest,
                                   cJSON_IsTrue(requiresFullContent) != 0,
                                   false});
            } else {
                RS_LOGW("cores: manifest %s has no %s.prx", e.name.c_str(),
                        name->valuestring);
            }
        } else {
            RS_LOGW("cores: %s lacks name/systems", e.name.c_str());
        }
        cJSON_Delete(root);
    }

    sortCores(m_cores);
    for (const auto& c : m_cores)
        RS_LOGI("cores: found '%s' %s (%s)", c.name.c_str(),
                c.version.c_str(), c.systems.c_str());
}

#endif  /* RS_STATIC_CORES */

}  // namespace rs
