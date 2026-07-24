#include "frontend/database/library.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rs::db {

namespace {
const char* LIB_PATH = "ms0:/RETROSUITE/library.json";

void readHashArray(const cJSON* root, const char* key, std::vector<u32>& out) {
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsArray(arr)) return;
    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, arr) {
        if (cJSON_IsNumber(it)) out.push_back(u32(it->valuedouble));
    }
}

void writeHashArray(cJSON* root, const char* key, const std::vector<u32>& v) {
    cJSON* arr = cJSON_AddArrayToObject(root, key);
    for (u32 h : v) cJSON_AddItemToArray(arr, cJSON_CreateNumber(double(h)));
}
}  // namespace

void Library::load() {
    std::vector<u8> buf;
    if (!fs::readFile(LIB_PATH, buf)) return;
    buf.push_back(0);
    cJSON* root = cJSON_Parse(reinterpret_cast<const char*>(buf.data()));
    if (!root) {
        RS_LOGW("library: bad json, starting fresh");
        return;
    }
    readHashArray(root, "favorites", m_favorites);
    readHashArray(root, "recents", m_recents);
    const cJSON* counts = cJSON_GetObjectItemCaseSensitive(root, "playCounts");
    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, counts) {
        if (cJSON_IsNumber(it) && it->string)
            m_playCounts.push_back({u32(std::strtoul(it->string, nullptr, 16)),
                                    it->valueint});
    }
    cJSON_Delete(root);
}

void Library::save() const {
    cJSON* root = cJSON_CreateObject();
    writeHashArray(root, "favorites", m_favorites);
    writeHashArray(root, "recents", m_recents);
    cJSON* counts = cJSON_AddObjectToObject(root, "playCounts");
    for (const auto& [hash, n] : m_playCounts) {
        char key[12];
        std::snprintf(key, sizeof key, "%08x", unsigned(hash));
        cJSON_AddNumberToObject(counts, key, n);
    }
    char* text = cJSON_Print(root);
    cJSON_Delete(root);
    if (text) {
        fs::mkdirs(fs::ROOT);
        fs::writeFile(LIB_PATH, text, u32(std::strlen(text)));
        cJSON_free(text);
    }
}

bool Library::isFavorite(u32 hash) const {
    return std::find(m_favorites.begin(), m_favorites.end(), hash) !=
           m_favorites.end();
}

void Library::toggleFavorite(u32 hash) {
    const auto it = std::find(m_favorites.begin(), m_favorites.end(), hash);
    if (it != m_favorites.end()) m_favorites.erase(it);
    else m_favorites.push_back(hash);
    save();
}

void Library::notePlayed(u32 hash) {
    const auto it = std::find(m_recents.begin(), m_recents.end(), hash);
    if (it != m_recents.end()) m_recents.erase(it);
    m_recents.insert(m_recents.begin(), hash);
    if (int(m_recents.size()) > MAX_RECENTS) m_recents.resize(MAX_RECENTS);

    for (auto& [h, n] : m_playCounts) {
        if (h == hash) {
            n++;
            save();
            return;
        }
    }
    m_playCounts.push_back({hash, 1});
    save();
}

int Library::playCount(u32 hash) const {
    for (const auto& [h, n] : m_playCounts)
        if (h == hash) return n;
    return 0;
}

}  // namespace rs::db
