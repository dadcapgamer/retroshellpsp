#include "frontend/database/library.h"
#include "runtime/jsonfile.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rs::db {

namespace {
const char* LIB_PATH = "ms0:/RETROSHELL/library.json";

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
    cJSON* root = json::parseFile(LIB_PATH);
    if (!root) return;   /* first boot, or bad json — start fresh */
    readHashArray(root, "favorites", m_favorites);
    readHashArray(root, "recents", m_recents);
    const cJSON* counts = cJSON_GetObjectItemCaseSensitive(root, "playCounts");
    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, counts) {
        if (cJSON_IsNumber(it) && it->string)
            m_playCounts[u32(std::strtoul(it->string, nullptr, 16))] =
                it->valueint;
    }
    const cJSON* played = cJSON_GetObjectItemCaseSensitive(root, "lastPlayed");
    cJSON_ArrayForEach(it, played) {
        if (!cJSON_IsNumber(it) || !it->string) continue;
        const u64 stamp = u64(it->valuedouble);
        /* Bound malformed values before they reach the date formatter. */
        if (stamp < 200001010000ull || stamp > 219912312359ull) continue;
        m_lastPlayed[u32(std::strtoul(it->string, nullptr, 16))] = stamp;
    }
    const cJSON* selected =
        cJSON_GetObjectItemCaseSensitive(root, "lastSelected");
    cJSON_ArrayForEach(it, selected) {
        if (!cJSON_IsNumber(it) || !it->string) continue;
        const int system = int(std::strtol(it->string, nullptr, 10));
        if (system < 0 || system >= 32) continue;
        m_lastSelected.push_back({system, u32(it->valuedouble)});
    }
    const cJSON* seconds = cJSON_GetObjectItemCaseSensitive(root, "playSeconds");
    cJSON_ArrayForEach(it, seconds) {
        if (!cJSON_IsNumber(it) || !it->string || it->valuedouble < 0 ||
            it->valuedouble > 4e9)
            continue;
        m_playSeconds[u32(std::strtoul(it->string, nullptr, 16))] =
            u32(it->valuedouble);
    }
    const cJSON* views = cJSON_GetObjectItemCaseSensitive(root, "views");
    cJSON_ArrayForEach(it, views) {
        if (!cJSON_IsNumber(it) || !it->string) continue;
        const int system = int(std::strtol(it->string, nullptr, 10));
        if (system < 0 || system >= 32) continue;
        m_views.push_back({system, ViewState::unpack(it->valueint)});
    }
    if (const cJSON* loc = cJSON_GetObjectItemCaseSensitive(root, "location");
        cJSON_IsObject(loc)) {
        auto field = [&](const char* key, int lo, int hi, int& out) {
            const cJSON* v = cJSON_GetObjectItemCaseSensitive(loc, key);
            if (cJSON_IsNumber(v) && v->valueint >= lo && v->valueint <= hi)
                out = v->valueint;
        };
        field("system", 0, 31, m_location.system);
        field("layer", 0, 8, m_location.layer);
        field("continue", 0, 63, m_location.continueIdx);
        m_location.valid = true;
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
    cJSON* played = cJSON_AddObjectToObject(root, "lastPlayed");
    for (const auto& [hash, stamp] : m_lastPlayed) {
        char key[12];
        std::snprintf(key, sizeof key, "%08x", unsigned(hash));
        cJSON_AddNumberToObject(played, key, double(stamp));
    }
    cJSON* seconds = cJSON_AddObjectToObject(root, "playSeconds");
    for (const auto& [hash, secs] : m_playSeconds) {
        char key[12];
        std::snprintf(key, sizeof key, "%08x", unsigned(hash));
        cJSON_AddNumberToObject(seconds, key, double(secs));
    }
    cJSON* views = cJSON_AddObjectToObject(root, "views");
    for (const auto& [system, v] : m_views) {
        if (v.isDefault()) continue;
        char key[12];
        std::snprintf(key, sizeof key, "%d", system);
        cJSON_AddNumberToObject(views, key, v.pack());
    }
    if (m_location.valid) {
        cJSON* loc = cJSON_AddObjectToObject(root, "location");
        cJSON_AddNumberToObject(loc, "system", m_location.system);
        cJSON_AddNumberToObject(loc, "layer", m_location.layer);
        cJSON_AddNumberToObject(loc, "continue", m_location.continueIdx);
    }
    cJSON* selected = cJSON_AddObjectToObject(root, "lastSelected");
    for (const auto& [system, hash] : m_lastSelected) {
        char key[12];
        std::snprintf(key, sizeof key, "%d", system);
        cJSON_AddNumberToObject(selected, key, double(hash));
    }
    char* text = cJSON_Print(root);
    cJSON_Delete(root);
    if (text) {
        fs::mkdirs(fs::ROOT);
        const size_t len = std::strlen(text);
        if (len <= 256u * 1024u)
            fs::writeFileAtomic(LIB_PATH, text, u32(len));
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

void Library::notePlayed(u32 hash, u64 localTimestamp) {
    const auto it = std::find(m_recents.begin(), m_recents.end(), hash);
    if (it != m_recents.end()) m_recents.erase(it);
    m_recents.insert(m_recents.begin(), hash);
    if (int(m_recents.size()) > MAX_RECENTS) m_recents.resize(MAX_RECENTS);
    m_recentsRevision++;

    m_lastPlayed[hash] = localTimestamp;
    m_playCounts[hash]++;
    save();
}

bool Library::removeRecent(u32 hash) {
    const auto it = std::find(m_recents.begin(), m_recents.end(), hash);
    if (it == m_recents.end()) return false;
    m_recents.erase(it);
    m_recentsRevision++;
    save();
    return true;
}

u32 Library::lastSelected(int systemIdx) const {
    for (const auto& [system, hash] : m_lastSelected)
        if (system == systemIdx) return hash;
    return 0;
}

void Library::setLastSelected(int systemIdx, u32 hash) {
    for (auto& [system, h] : m_lastSelected) {
        if (system != systemIdx) continue;
        if (h != hash) {
            h = hash;
            m_dirty = true;
        }
        return;
    }
    m_lastSelected.push_back({systemIdx, hash});
    m_dirty = true;
}

void Library::flush() {
    if (!m_dirty) return;
    m_dirty = false;
    save();
}

int Library::playCount(u32 hash) const {
    const auto it = m_playCounts.find(hash);
    return it == m_playCounts.end() ? 0 : it->second;
}

u64 Library::lastPlayed(u32 hash) const {
    const auto it = m_lastPlayed.find(hash);
    return it == m_lastPlayed.end() ? 0 : it->second;
}

ViewState Library::view(int systemIdx) const {
    for (const auto& [system, v] : m_views)
        if (system == systemIdx) return v;
    return {};
}

void Library::setView(int systemIdx, ViewState v) {
    for (auto& [system, current] : m_views) {
        if (system != systemIdx) continue;
        if (!(current == v)) {
            current = v;
            m_dirty = true;
        }
        return;
    }
    if (v.isDefault()) return;
    m_views.push_back({systemIdx, v});
    m_dirty = true;
}

void Library::setLocation(int system, int layer, int continueIdx) {
    if (m_location.valid && m_location.system == system &&
        m_location.layer == layer && m_location.continueIdx == continueIdx)
        return;
    m_location = {system, layer, continueIdx, true};
    m_dirty = true;
}

void Library::addPlaytime(u32 hash, u32 seconds) {
    if (!seconds) return;
    u32& total = m_playSeconds[hash];
    total = seconds > 0xFFFFFFFFu - total ? 0xFFFFFFFFu : total + seconds;
    m_dirty = true;
}

u32 Library::playSeconds(u32 hash) const {
    const auto it = m_playSeconds.find(hash);
    return it == m_playSeconds.end() ? 0 : it->second;
}

}  // namespace rs::db
