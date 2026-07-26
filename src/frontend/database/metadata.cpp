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

const gfx::Texture* BoxartCache::get(const GameEntry& g) {
    for (auto& s : m_slots) {
        if (s.hash == g.pathHash) {
            if (s.missing) return nullptr;
            return s.tex.valid() ? &s.tex : nullptr;
        }
    }

    /* Not cached: claim the next slot round-robin and decode now. Box art
     * on PSP-sized screens is small; one decode fits in a frame budget at
     * menu framerates. */
    Slot& s = m_slots[m_clock];
    m_clock = (m_clock + 1) % SLOTS;
    gfx::Renderer::freeTexture(s.tex);
    s.hash = g.pathHash;
    s.missing = true;

    char path[512];
    metaPath(path, sizeof path, g, "boxart", "png");
    std::vector<u8> file;
    constexpr u32 MAX_BOXART_FILE = 2u * 1024u * 1024u;
    if (!fs::readFile(path, file, MAX_BOXART_FILE)) return nullptr;

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

void BoxartCache::clear() {
    for (auto& s : m_slots) {
        gfx::Renderer::freeTexture(s.tex);
        s = Slot{};
    }
}

}  // namespace rs::db
