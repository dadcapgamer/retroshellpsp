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
const char* CFG_PATH = "ms0:/RETROSUITE/config.json";
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
    std::snprintf(buf, n, "ms0:/RETROSUITE/pergame/%08x.json", unsigned(hash));
}

}  // namespace

Config& get() { return s_cfg; }

void load() {
    cJSON* root = json::parseFile(CFG_PATH);
    if (!root) return;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "theme");
        cJSON_IsString(v) && safeId(v->valuestring))
        s_cfg.theme = v->valuestring;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "cpuMenuMhz");
        cJSON_IsNumber(v) && validClock(v->valueint))
        s_cfg.cpuMenuMhz = v->valueint;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "cpuGameMhz");
        cJSON_IsNumber(v) && validClock(v->valueint))
        s_cfg.cpuGameMhz = v->valueint;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "uiSounds");
        cJSON_IsBool(v))
        s_cfg.uiSounds = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "showFps");
        cJSON_IsBool(v))
        s_cfg.showFps = cJSON_IsTrue(v);
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "autosave");
        cJSON_IsBool(v))
        s_cfg.autosave = cJSON_IsTrue(v);
    cJSON_Delete(root);
}

void save() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "theme", s_cfg.theme.c_str());
    cJSON_AddNumberToObject(root, "cpuMenuMhz", s_cfg.cpuMenuMhz);
    cJSON_AddNumberToObject(root, "cpuGameMhz", s_cfg.cpuGameMhz);
    cJSON_AddBoolToObject(root, "uiSounds", s_cfg.uiSounds);
    cJSON_AddBoolToObject(root, "showFps", s_cfg.showFps);
    cJSON_AddBoolToObject(root, "autosave", s_cfg.autosave);
    fs::mkdirs(fs::ROOT);
    if (!json::writeFile(CFG_PATH, root)) RS_LOGW("config: save failed");
    cJSON_Delete(root);
}

std::string gameOption(u32 pathHash, const char* key) {
    if (!safeId(key, 64)) return {};
    char path[96];
    gamePath(path, sizeof path, pathHash);
    cJSON* root = json::parseFile(path);
    if (!root) return {};
    std::string out;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, key);
        cJSON_IsString(v) && std::strlen(v->valuestring) <= 256)
        out = v->valuestring;
    cJSON_Delete(root);
    return out;
}

void setGameOption(u32 pathHash, const char* key, const char* value) {
    if (!safeId(key, 64) || !value || std::strlen(value) > 256) return;
    char path[96];
    gamePath(path, sizeof path, pathHash);
    cJSON* root = json::parseFile(path);
    if (!root) root = cJSON_CreateObject();
    cJSON_DeleteItemFromObjectCaseSensitive(root, key);
    cJSON_AddStringToObject(root, key, value);
    fs::mkdirs("ms0:/RETROSUITE/pergame");
    json::writeFile(path, root);
    cJSON_Delete(root);
}

}  // namespace rs::cfg
