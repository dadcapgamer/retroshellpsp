#include "frontend/themes/theme.h"
#include "frontend/database/systems.h"
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
    /* Current role names first; the second block keeps theme.json files
     * written for the older palette working by mapping each legacy key onto
     * the role that replaced it. */
    struct { const char* key; u32* dst; } MAP[] = {
        {"bg", &p.bg},                 {"surface", &p.surface},
        {"surface2", &p.surface2},     {"line", &p.line},
        {"textPrimary", &p.textPrimary},
        {"textSecondary", &p.textSecondary},
        {"textMuted", &p.textMuted},   {"textDisabled", &p.textDisabled},
        {"accent", &p.accent},         {"onAccent", &p.onAccent},
        {"focusEdge", &p.focusEdge},   {"danger", &p.danger},
        {"scrim", &p.scrim},           {"dim", &p.dim},
        {"pattern", &p.pattern},
        {"waveA", &p.waveA},           {"waveB", &p.waveB},
        /* legacy */
        {"bgTop", &p.bg},              {"panelBg", &p.surface},
        {"tileBg", &p.surface},        {"menuBg", &p.surface2},
        {"divider", &p.line},          {"textDim", &p.textMuted},
        {"selectBg", &p.accent},       {"selectText", &p.onAccent},
        {"fallbackDot", &p.pattern},
    };
    for (auto& m : MAP) {
        const cJSON* v = cJSON_GetObjectItemCaseSensitive(colors, m.key);
        if (cJSON_IsString(v)) *m.dst = parseColor(v->valuestring, *m.dst);
    }
}

const Palette& galleryPalette() {
    static const Palette p = {
        rsHex(0x222222), rsHex(0x111111), rsHex(0x444444), rsHex(0x333333),
        rsHex(0xFFFFFF), rsHex(0x999999), rsHex(0x777777), rsHex(0x555555),
        rsHex(0x444444), rsHex(0xFFFFFF), rsHex(0xFFFFFF), rsHex(0xF0645A),
        rsHex(0x000000), rsHex(0x000000), rsHex(0x2A2A2A),
        rsHex(0x000000, 0), rsHex(0x000000, 0), true,
    };
    return p;
}

bool safeThemeId(const std::string& id) {
    if (id.empty() || id.size() > 48) return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    return true;
}

bool safeRelativeAsset(const char* value) {
    return value && *value && value[0] != '/' && !std::strstr(value, "..") &&
           !std::strchr(value, ':') && !std::strchr(value, '\\');
}

bool loadImage4444(gfx::Texture& out, const u8* bytes, size_t size) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes, int(size), &w, &h, &comp, 4);
    bool ok = false;
    if (pixels && w > 0 && h > 0 && w <= 512 && h <= 512) {
        const size_t count = size_t(w) * size_t(h);
        std::vector<u16> packed(count);
        for (size_t i = 0; i < count; ++i) {
            const u8 r = pixels[i * 4 + 0];
            const u8 g = pixels[i * 4 + 1];
            const u8 b = pixels[i * 4 + 2];
            const u8 a = pixels[i * 4 + 3];
            packed[i] = u16((r >> 4) | ((g >> 4) << 4) |
                            ((b >> 4) << 8) | ((a >> 4) << 12));
        }
        ok = gfx::Renderer::createTexture(out, w, h, GU_PSM_4444,
                                          packed.data());
    }
    if (pixels) stbi_image_free(pixels);
    return ok;
}

bool loadThemeImage(gfx::Texture& out, const char* themeRoot,
                    const char* relative) {
    if (!safeRelativeAsset(relative)) return false;
    char path[384];
    const int n = std::snprintf(path, sizeof path, "%s/%s", themeRoot,
                                relative);
    if (n <= 0 || n >= int(sizeof path)) return false;
    std::vector<u8> image;
    return fs::readFile(path, image, 1024u * 1024u) &&
           loadImage4444(out, image.data(), image.size());
}

void loadSystemAssets(const cJSON* root, const char* themeRoot,
                      const char* property,
                      std::vector<gfx::Texture>& destination) {
    const cJSON* assets = cJSON_GetObjectItemCaseSensitive(root, property);
    if (!cJSON_IsObject(assets)) return;
    destination.resize(size_t(db::SYSTEM_COUNT));
    for (int i = 0; i < db::SYSTEM_COUNT; ++i) {
        const char* key = db::systemInfo(db::System(i)).coreId;
        const cJSON* value = cJSON_GetObjectItemCaseSensitive(assets, key);
        if (!cJSON_IsString(value)) continue;
        if (!loadThemeImage(destination[size_t(i)], themeRoot,
                            value->valuestring))
            RS_LOGW("theme: %s asset '%s' failed", property, key);
    }
}

struct Builtin { const char* id; const char* title; const Palette& (*palette)(); };
const Builtin BUILTINS[] = {
    {"dark", "Dark", &theme::dark},
    {"graphite", "Graphite", &theme::graphite},
    {"light", "Light", &theme::light},
    {"mist", "Mist", &theme::mist},
};

Theme builtin(const Builtin& b) {
    Theme t;
    t.id = b.id;
    t.title = b.title;
    t.palette = b.palette();
    return t;
}

Theme builtin(bool dark) { return builtin(BUILTINS[dark ? 0 : 2]); }

}  // namespace

Theme loadTheme(const std::string& id) {
    for (const Builtin& b : BUILTINS)
        if (id == b.id) return builtin(b);
    if (!safeThemeId(id)) {
        RS_LOGW("theme: rejected unsafe id");
        return builtin(true);
    }

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
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "presentation");
        cJSON_IsString(v) && std::strcmp(v->valuestring, "gallery") == 0) {
        t.presentation = Presentation::Gallery;
        t.palette = galleryPalette();
    }
    applyColors(t.palette, cJSON_GetObjectItemCaseSensitive(root, "colors"));
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "waves");
        cJSON_IsBool(v))
        t.waves = cJSON_IsTrue(v);

    char themeRoot[256];
    const int rootLen = std::snprintf(themeRoot, sizeof themeRoot,
                                      "%s/themes/%s", fs::ROOT, id.c_str());
    if (rootLen > 0 && rootLen < int(sizeof themeRoot)) {
        loadSystemAssets(root, themeRoot, "systemArt", t.systemArt);
        loadSystemAssets(root, themeRoot, "systemLogos", t.systemLogos);
    }

    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "background");
        cJSON_IsString(v) && safeRelativeAsset(v->valuestring)) {
        char bg[256];
        std::snprintf(bg, sizeof bg, "%s/themes/%s/%s", fs::ROOT, id.c_str(),
                      v->valuestring);
        std::vector<u8> img;
        if (fs::readFile(bg, img, 2u * 1024u * 1024u)) {
            int w = 0, h = 0, comp = 0;
            stbi_uc* px = stbi_load_from_memory(img.data(), int(img.size()),
                                                &w, &h, &comp, 4);
            if (px && w > 0 && h > 0 && w <= 1024 && h <= 1024) {
                gfx::Renderer::createTexture(t.background, w, h, GU_PSM_8888,
                                             px);
            }
            if (px) stbi_image_free(px);
        }
    }
    cJSON_Delete(root);
    RS_LOGI("theme: loaded '%s'", t.title.c_str());
    return t;
}

Palette paletteFor(const std::string& id) {
    for (const Builtin& b : BUILTINS)
        if (id == b.id) return b.palette();
    if (!safeThemeId(id)) return builtin(true).palette;
    char path[256];
    std::snprintf(path, sizeof path, "%s/themes/%s/theme.json", fs::ROOT,
                  id.c_str());
    cJSON* root = json::parseFile(path);
    if (!root) return builtin(true).palette;
    const cJSON* darkFlag = cJSON_GetObjectItemCaseSensitive(root, "dark");
    Palette p = builtin(!cJSON_IsFalse(darkFlag)).palette;
    if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(root, "presentation");
        cJSON_IsString(v) && std::strcmp(v->valuestring, "gallery") == 0)
        p = galleryPalette();
    applyColors(p, cJSON_GetObjectItemCaseSensitive(root, "colors"));
    cJSON_Delete(root);
    return p;
}

std::vector<std::string> availableThemes() {
    std::vector<std::string> out;
    for (const Builtin& b : BUILTINS) out.push_back(b.id);
    char dir[128];
    std::snprintf(dir, sizeof dir, "%s/themes", fs::ROOT);
    std::vector<fs::DirEntry> entries;
    if (fs::listDir(dir, entries)) {
        for (const auto& e : entries) {
            if (!e.isDir || !safeThemeId(e.name)) continue;
            char manifest[256];
            const int n = std::snprintf(manifest, sizeof manifest,
                "%s/themes/%s/theme.json", fs::ROOT, e.name.c_str());
            if (n <= 0 || n >= int(sizeof manifest) || !fs::exists(manifest))
                continue;
            bool duplicate = false;
            for (const auto& existing : out)
                if (existing == e.name) { duplicate = true; break; }
            if (!duplicate) out.push_back(e.name);
        }
    }
    return out;
}

}  // namespace rs::theme
