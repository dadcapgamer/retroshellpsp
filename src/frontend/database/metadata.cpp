#include "frontend/database/metadata.h"
#include "runtime/jsonfile.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include "cJSON.h"
#include "stb_image.h"

#include <pspgu.h>

#include <cstdio>
#include <cstring>

namespace rs::db {

namespace {
void metaPath(char* buf, size_t n, const GameEntry& g, const char* kind,
              const char* ext) {
    std::snprintf(buf, n, "%s/%s/%s/%s.%s", fs::ROOT, kind,
                  systemInfo(g.system).dirName, g.name.c_str(), ext);
}

}  // namespace

GameMeta loadMeta(const GameEntry& g) {
    GameMeta m;
    char path[512];
    metaPath(path, sizeof path, g, "metadata", "json");

    cJSON* root = json::parseFile(path);
    if (!root) return m;

    auto str = [&](const char* key) -> std::string {
        const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, key);
        return cJSON_IsString(v) ? v->valuestring : "";
    };
    m.description = str("description");
    m.developer   = str("developer");
    m.publisher   = str("publisher");
    m.genre       = str("genre");
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "year");
        cJSON_IsNumber(v))
        m.year = v->valueint;
    m.loaded = true;
    cJSON_Delete(root);
    return m;
}

void BoxartCache::loadShots() {
    m_shotsLoaded = true;
    m_shots.clear();
    char dir[64];
    std::snprintf(dir, sizeof dir, "%s/screenshots", fs::ROOT);
    std::vector<fs::DirEntry> entries;
    if (!fs::listDir(dir, entries)) return;
    std::vector<unsigned> newest;
    for (const auto& e : entries) {
        unsigned hash = 0, serial = 0;
        char tail[8] = {};
        if (e.isDir ||
            std::sscanf(e.name.c_str(), "%8x_%u.%7s", &hash, &serial, tail) != 3 ||
            std::strcmp(tail, "png") != 0)
            continue;
        size_t slot = m_shots.size();
        for (size_t i = 0; i < m_shots.size(); i++)
            if (m_shots[i].first == hash) slot = i;
        if (slot == m_shots.size()) {
            m_shots.push_back({hash, std::string()});
            newest.push_back(0);
        } else if (serial < newest[slot]) {
            continue;
        }
        newest[slot] = serial;
        m_shots[slot].second = std::string(dir) + "/" + e.name;
    }
}

const std::string* BoxartCache::shotFor(u32 hash) const {
    for (const auto& s : m_shots)
        if (s.first == hash) return &s.second;
    return nullptr;
}

const gfx::Texture* BoxartCache::get(const GameEntry& g) {
    for (auto& s : m_slots) {
        if (s.hash == g.pathHash) {
            if (s.missing) return nullptr;
            return s.tex.valid() ? &s.tex : nullptr;
        }
    }
    for (u32 hash : m_missing)
        if (hash == g.pathHash) return nullptr;

    /* Not cached: claim the next slot round-robin and decode now. Box art
     * on PSP-sized screens is small; one decode fits in a frame budget at
     * menu framerates. */
    Slot& s = m_slots[m_clock];
    m_clock = (m_clock + 1) % SLOTS;
    gfx::Renderer::freeTexture(s.tex);
    s.hash = g.pathHash;
    s.missing = true;

    std::vector<u8> file;
    constexpr u32 MAX_BOXART_FILE = 2u * 1024u * 1024u;
    char path[512];
    bool found = false;
    bool sibling = !g.artPath.empty();
    if (sibling) {
        std::snprintf(path, sizeof path, "%s", g.artPath.c_str());
        found = fs::readFile(path, file, MAX_BOXART_FILE);
    }

    /* No cover: use the player's own latest screenshot of this game. */
    bool screenshot = false;
    if (!found) {
        if (!m_shotsLoaded) loadShots();
        if (const std::string* shot = shotFor(g.pathHash)) {
            std::snprintf(path, sizeof path, "%s", shot->c_str());
            found = fs::readFile(path, file, MAX_BOXART_FILE);
            screenshot = found;
        }
    }

    /* Coverless entries stop here without touching the Memory Stick. The
     * scanner records beside-ROM artwork, avoiding repeated failed opens. */
    if (!found) {
        m_missing[m_missingClock] = g.pathHash;
        m_missingClock = (m_missingClock + 1) % MISSING_SLOTS;
        return nullptr;
    }
    RS_LOGI("boxart: loaded %s %s",
            screenshot ? "screenshot" : "beside ROM", path);

    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(file.data(), int(file.size()), &w, &h,
                                        &comp, 4);
    if (!px || w <= 0 || h <= 0 || w > 2048 || h > 2048) {
        RS_LOGW("boxart: decode failed for %s", g.name.c_str());
        if (px) stbi_image_free(px);
        return nullptr;
    }
    /* Cap size to keep VRAM/RAM predictable: art bigger than 160px is
     * downsampled 2x with a box filter. */
    while (w > 160 || h > 160) {
        const int nw = w > 1 ? w / 2 : 1;
        const int nh = h > 1 ? h / 2 : 1;
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                for (int c = 0; c < 4; c++) {
                    const int x0 = x * 2, x1 = x0 + 1 < w ? x0 + 1 : x0;
                    const int y0 = y * 2, y1 = y0 + 1 < h ? y0 + 1 : y0;
                    const int a = px[(y0 * w + x0) * 4 + c];
                    const int b = px[(y0 * w + x1) * 4 + c];
                    const int cc = px[(y1 * w + x0) * 4 + c];
                    const int d = px[(y1 * w + x1) * 4 + c];
                    px[(y * nw + x) * 4 + c] = u8((a + b + cc + d) / 4);
                }
        w = nw;
        h = nh;
    }

    const bool ok = gfx::Renderer::createTexture(s.tex, w, h, GU_PSM_8888, px);
    stbi_image_free(px);
    if (!ok) return nullptr;
    s.missing = false;
    return &s.tex;
}

const gfx::Texture* BoxartCache::peek(const GameEntry& g) const {
    for (const auto& s : m_slots)
        if (s.hash == g.pathHash)
            return (!s.missing && s.tex.valid()) ? &s.tex : nullptr;
    return nullptr;
}

void BoxartCache::clear() {
    for (auto& s : m_slots) {
        gfx::Renderer::freeTexture(s.tex);
        s = Slot{};
    }
    std::memset(m_missing, 0, sizeof m_missing);
    m_shots.clear();
    m_shotsLoaded = false;
    m_clock = 0;
    m_missingClock = 0;
}

}  // namespace rs::db
