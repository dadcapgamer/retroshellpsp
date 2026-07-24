#include "frontend/themes/theme.h"
#include "runtime/jsonfile.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include "cJSON.h"
#include "stb_image.h"

#include <pspgu.h>

#include <cstdio>
#include <cstring>

namespace rs::theme {

namespace {

/* "#RRGGBB" or "#RRGGBBAA" → ABGR. Returns fallback on parse failure. */
u32 parseColor(const char* s, u32 fallback) {
    if (!s || s[0] != '#') return fallback;
    const size_t n = std::strlen(s + 1);
    if (n != 6 && n != 8) return fallback;
    u32 v = 0;
    for (const char* p = s + 1; *p; p++) {
        v <<= 4;
        if (*p >= '0' && *p <= '9') v |= u32(*p - '0');
        else if (*p >= 'a' && *p <= 'f') v |= u32(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F') v |= u32(*p - 'A' + 10);
        else return fallback;
    }
    const u32 a = (n == 8) ? (v & 0xFF) : 0xFF;
    const u32 rgb = (n == 8) ? (v >> 8) : v;
    return rsHex(rgb, a);
}

void applyColors(Palette& p, const cJSON* colors) {
    if (!cJSON_IsObject(colors)) return;
    struct { const char* key; u32* dst; } MAP[] = {
        {"bgTop", &p.bgTop},           {"bgBottom", &p.bgBottom},
        {"waveA", &p.waveA},           {"waveB", &p.waveB},
        {"textPrimary", &p.textPrimary},
        {"textSecondary", &p.textSecondary},
        {"textDim", &p.textDim},       {"accent", &p.accent},
        {"tileBg", &p.tileBg},         {"tileFocusBg", &p.tileFocusBg},
        {"panelBg", &p.panelBg},       {"panelOutline", &p.panelOutline},
        {"menuBg", &p.menuBg},
        {"shadow", &p.shadow},         {"scrim", &p.scrim},
    };
    for (auto& m : MAP) {
        const cJSON* v = cJSON_GetObjectItemCaseSensitive(colors, m.key);
        if (cJSON_IsString(v)) *m.dst = parseColor(v->valuestring, *m.dst);
    }
}

Theme builtin(bool dark) {
    Theme t;
    t.id = dark ? "dark" : "light";
    t.title = dark ? "Dark" : "Light";
    t.palette = dark ? theme::dark() : theme::light();
    return t;
}

}  // namespace

Theme loadTheme(const std::string& id) {
    if (id == "dark") return builtin(true);
    if (id == "light") return builtin(false);

    char path[256];
    std::snprintf(path, sizeof path, "%s/themes/%s/theme.json", fs::ROOT,
                  id.c_str());
    cJSON* root = json::parseFile(path);
    if (!root) {
        RS_LOGW("theme: %s missing or invalid, using dark", id.c_str());
        return builtin(true);
    }

    const cJSON* darkFlag = cJSON_GetObjectItemCaseSensitive(root, "dark");
    Theme t = builtin(!cJSON_IsFalse(darkFlag));
    t.id = id;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "name");
        cJSON_IsString(v))
        t.title = v->valuestring;
    applyColors(t.palette, cJSON_GetObjectItemCaseSensitive(root, "colors"));
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "waves");
        cJSON_IsBool(v))
        t.waves = cJSON_IsTrue(v);

    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "background");
        cJSON_IsString(v)) {
        char bg[256];
        std::snprintf(bg, sizeof bg, "%s/themes/%s/%s", fs::ROOT, id.c_str(),
                      v->valuestring);
        std::vector<u8> img;
        if (fs::readFile(bg, img)) {
            int w = 0, h = 0, comp = 0;
            stbi_uc* px = stbi_load_from_memory(img.data(), int(img.size()),
                                                &w, &h, &comp, 4);
            if (px) {
                gfx::Renderer::createTexture(t.background, w, h, GU_PSM_8888,
                                             px);
                stbi_image_free(px);
            }
        }
    }
    cJSON_Delete(root);
    RS_LOGI("theme: loaded '%s'", t.title.c_str());
    return t;
}

std::vector<std::string> availableThemes() {
    std::vector<std::string> out = {"dark", "light"};
    char dir[128];
    std::snprintf(dir, sizeof dir, "%s/themes", fs::ROOT);
    std::vector<fs::DirEntry> entries;
    if (fs::listDir(dir, entries)) {
        for (const auto& e : entries)
            if (e.isDir) out.push_back(e.name);
    }
    return out;
}

}  // namespace rs::theme
