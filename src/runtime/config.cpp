#include "runtime/config.h"
#include "runtime/jsonfile.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include "cJSON.h"

#include <cstdio>
#include <cstring>
#include <cctype>

namespace rs::cfg {

namespace {
const char* CFG_PATH = "ms0:/RETROSHELL/config.json";
Config s_cfg;

bool safeId(const char* s, size_t maxLen = 48) {
    if (!s || !*s || std::strlen(s) > maxLen) return false;
    for (; *s; ++s)
        if (!(std::isalnum(static_cast<unsigned char>(*s)) ||
              *s == '_' || *s == '-'))
            return false;
    return true;
}

bool validClock(int mhz) {
    return mhz == 222 || mhz == 266 || mhz == 333;
}

void gamePath(char* buf, size_t n, u32 hash) {
    std::snprintf(buf, n, "ms0:/RETROSHELL/pergame/%08x.json", unsigned(hash));
}

bool corePath(char* buf, size_t n, const char* core) {
    if (!safeId(core, 31)) return false;
    std::snprintf(buf, n, "ms0:/RETROSHELL/percore/%s.json", core);
    return true;
}

std::string readOption(const char* path, const char* key) {
    cJSON* root = json::parseFile(path);
    if (!root) return {};
    std::string out;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, key);
        cJSON_IsString(v) && std::strlen(v->valuestring) <= 256)
        out = v->valuestring;
    cJSON_Delete(root);
    return out;
}

void writeOption(const char* path, const char* dir, const char* key,
                 const char* value) {
    cJSON* root = json::parseFile(path);
    if (!root) root = cJSON_CreateObject();
    cJSON_DeleteItemFromObjectCaseSensitive(root, key);
    cJSON_AddStringToObject(root, key, value);
    fs::mkdirs(dir);
    json::writeFile(path, root);
    cJSON_Delete(root);
}

}  // namespace

Config& get() { return s_cfg; }

std::string systemCore(const char* coreId) {
    for (const auto& [id, name] : s_cfg.systemCores)
        if (id == coreId) return name;
    return {};
}

void setSystemCore(const char* coreId, const char* coreName) {
    if (!safeId(coreId, 16)) return;
    for (auto it = s_cfg.systemCores.begin(); it != s_cfg.systemCores.end();
         ++it) {
        if (it->first != coreId) continue;
        if (!coreName || !*coreName) s_cfg.systemCores.erase(it);
        else if (safeId(coreName)) it->second = coreName;
        return;
    }
    if (coreName && safeId(coreName))
        s_cfg.systemCores.push_back({coreId, coreName});
}

bool systemEnabled(const char* coreId) {
    for (const auto& id : s_cfg.disabledSystems)
        if (id == coreId) return false;
    return true;
}

void setSystemEnabled(const char* coreId, bool enabled) {
    if (!safeId(coreId, 16)) return;
    auto& list = s_cfg.disabledSystems;
    for (auto it = list.begin(); it != list.end(); ++it)
        if (*it == coreId) {
            if (enabled) list.erase(it);
            return;
        }
    if (!enabled) list.push_back(coreId);
}

void load() {
    cJSON* root = json::parseFile(CFG_PATH);
    if (!root) return;   /* first boot: setupDone stays false */
    /* An existing config predates first-run setup unless it says otherwise. */
    s_cfg.setupDone = true;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "setupDone");
        cJSON_IsBool(v))
        s_cfg.setupDone = cJSON_IsTrue(v);
    if (const cJSON* map = cJSON_GetObjectItemCaseSensitive(root, "systemCores");
        cJSON_IsObject(map)) {
        for (const cJSON* it = map->child; it; it = it->next)
            if (cJSON_IsString(it) && it->string)
                setSystemCore(it->string, it->valuestring);
    }
    if (const cJSON* list = cJSON_GetObjectItemCaseSensitive(root, "disabledSystems");
        cJSON_IsArray(list)) {
        for (const cJSON* it = list->child; it; it = it->next)
            if (cJSON_IsString(it)) setSystemEnabled(it->valuestring, false);
    }
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "theme");
        cJSON_IsString(v) && safeId(v->valuestring))
        s_cfg.theme = v->valuestring;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "accent");
        cJSON_IsNumber(v) && v->valueint >= 0 && v->valueint < 5)
        s_cfg.accent = v->valueint;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "cpuMenuMhz");
        cJSON_IsNumber(v) && validClock(v->valueint))
        s_cfg.cpuMenuMhz = v->valueint;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "cpuGameMhz");
        cJSON_IsNumber(v) && validClock(v->valueint))
        s_cfg.cpuGameMhz = v->valueint;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "uiSounds");
        cJSON_IsBool(v))
        s_cfg.uiSounds = cJSON_IsTrue(v);
    if (const cJSON* v =
            cJSON_GetObjectItemCaseSensitive(root, "clock24Hour");
        cJSON_IsBool(v))
        s_cfg.clock24Hour = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "showFps");
        cJSON_IsBool(v))
        s_cfg.showFps = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "autosave");
        cJSON_IsBool(v))
        s_cfg.autosave = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "showArt");
        cJSON_IsBool(v))
        s_cfg.showArt = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "homeLayout");
        cJSON_IsNumber(v) && v->valueint >= 0 &&
        v->valueint < HOME_LAYOUT_COUNT)
        s_cfg.homeLayout = v->valueint;
    cJSON_Delete(root);
}

void save() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "theme", s_cfg.theme.c_str());
    cJSON_AddNumberToObject(root, "accent", s_cfg.accent);
    cJSON_AddNumberToObject(root, "cpuMenuMhz", s_cfg.cpuMenuMhz);
    cJSON_AddNumberToObject(root, "cpuGameMhz", s_cfg.cpuGameMhz);
    cJSON_AddBoolToObject(root, "uiSounds", s_cfg.uiSounds);
    cJSON_AddBoolToObject(root, "clock24Hour", s_cfg.clock24Hour);
    cJSON_AddBoolToObject(root, "showFps", s_cfg.showFps);
    cJSON_AddBoolToObject(root, "autosave", s_cfg.autosave);
    cJSON_AddBoolToObject(root, "showArt", s_cfg.showArt);
    cJSON_AddNumberToObject(root, "homeLayout", s_cfg.homeLayout);
    cJSON_AddBoolToObject(root, "setupDone", s_cfg.setupDone);
    cJSON* disabled = cJSON_AddArrayToObject(root, "disabledSystems");
    for (const auto& id : s_cfg.disabledSystems)
        cJSON_AddItemToArray(disabled, cJSON_CreateString(id.c_str()));
    cJSON* cores = cJSON_AddObjectToObject(root, "systemCores");
    for (const auto& [id, name] : s_cfg.systemCores)
        cJSON_AddStringToObject(cores, id.c_str(), name.c_str());
    fs::mkdirs(fs::ROOT);
    if (!json::writeFile(CFG_PATH, root)) RS_LOGW("config: save failed");
    cJSON_Delete(root);
}

std::string gameOption(u32 pathHash, const char* key) {
    if (!safeId(key, 64)) return {};
    char path[96];
    gamePath(path, sizeof path, pathHash);
    return readOption(path, key);
}

void setGameOption(u32 pathHash, const char* key, const char* value) {
    if (!safeId(key, 64) || !value || std::strlen(value) > 256) return;
    char path[96];
    gamePath(path, sizeof path, pathHash);
    writeOption(path, "ms0:/RETROSHELL/pergame", key, value);
}

std::string coreOption(const char* core, const char* key) {
    char path[96];
    if (!safeId(key, 64) || !corePath(path, sizeof path, core)) return {};
    return readOption(path, key);
}

void setCoreOption(const char* core, const char* key, const char* value) {
    char path[96];
    if (!safeId(key, 64) || !value || std::strlen(value) > 256 ||
        !corePath(path, sizeof path, core))
        return;
    writeOption(path, "ms0:/RETROSHELL/percore", key, value);
}

std::string option(u32 pathHash, const char* core, const char* key) {
    std::string value = gameOption(pathHash, key);
    if (value.empty() && core && *core) value = coreOption(core, key);
    return value;
}

bool gameScoped(u32 pathHash) {
    return gameOption(pathHash, SCOPE_KEY) == "game";
}

void storeOption(u32 pathHash, const char* core, const char* key,
                 const char* value) {
    if (gameScoped(pathHash) || !core || !*core) {
        setGameOption(pathHash, key, value);
        return;
    }
    setCoreOption(core, key, value);
    /* An older per-game value would keep shadowing the emulator's. */
    if (!gameOption(pathHash, key).empty()) setGameOption(pathHash, key, "");
}

}  // namespace rs::cfg
