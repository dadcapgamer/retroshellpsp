#include "frontend/scenes/home_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/grid_window.h"
#include "runtime/config.h"
#include "runtime/save_manager.h"

#include <pspctrl.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace rs {

namespace {

struct Category {
    int systemIdx;     /* >= 0: index into db::System */
};

constexpr int CATEGORY_FAVORITES = -2;
constexpr int CATEGORY_SETTINGS  = -1;
constexpr int CAT_SETTINGS  = 10;
constexpr int SETTINGS_ICON = 9;

const Category& cat(int i) {
    static const Category CATS[] = {
        /* Keep PC Engine in the first eight visible cards. Game Gear remains
         * one step to the right alongside Favorites and Settings. */
        { 0}, { 1}, { 2}, { 3}, { 4}, { 5}, { 6}, { 8}, { 7},
        {CATEGORY_FAVORITES}, {CATEGORY_SETTINGS},
    };
    return CATS[i];
}

constexpr float GRID_TOP      = 72.f;
constexpr float GRID_TILE     = 72.f;
constexpr float GRID_STEP     = 80.f;
constexpr int   GRID_COLS     = 3;
constexpr int   GRID_ROWS     = 2;
constexpr int   GRID_VISIBLE  = GRID_COLS * GRID_ROWS;
constexpr int   RECENT_VISIBLE = 5;

int gridStartRow(int index, int count) {
    if (count <= 0) return 0;
    const int selectedRow = rsClamp(index, 0, count - 1) / GRID_COLS;
    const int totalRows = (count + GRID_COLS - 1) / GRID_COLS;
    return rsClamp(selectedRow - 1, 0, rsClamp(totalRows - GRID_ROWS, 0,
                                               totalRows));
}

u32 opaqueOver(u32 foreground, u32 background) {
    const u32 alpha = rsAlphaOf(foreground);
    u32 result = 0xFF000000u;
    for (int shift = 0; shift < 24; shift += 8) {
        const u32 fg = (foreground >> shift) & 0xFFu;
        const u32 bg = (background >> shift) & 0xFFu;
        result |= ((fg * alpha + bg * (255u - alpha) + 127u) / 255u)
                  << shift;
    }
    return result;
}

u32 backgroundAt(const theme::Palette& pal, float y) {
    return rsWithAlpha(
        rsLerpColor(pal.bgTop, pal.bgBottom,
                    rsClamp(y / float(RS_SCREEN_H), 0.f, 1.f)),
        255u);
}

u32 surfaceAt(const theme::Palette& pal, u32 surface, float y) {
    return opaqueOver(surface, backgroundAt(pal, y));
}

void maskArtCorners(gfx::Renderer& r, float x, float y, float w, float h,
                    u32 color) {
    /* Pixel-aligned approximation of a 4px CSS radius. */
    r.rect(x,         y,         2.f, 1.f, color);
    r.rect(x,         y + 1.f,   1.f, 1.f, color);
    r.rect(x + w - 2.f, y,       2.f, 1.f, color);
    r.rect(x + w - 1.f, y + 1.f, 1.f, 1.f, color);
    r.rect(x,         y + h - 1.f, 2.f, 1.f, color);
    r.rect(x,         y + h - 2.f, 1.f, 1.f, color);
    r.rect(x + w - 2.f, y + h - 1.f, 2.f, 1.f, color);
    r.rect(x + w - 1.f, y + h - 2.f, 1.f, 1.f, color);
}

void drawArtCover(gfx::Renderer& r, const gfx::Texture& art,
                  float x, float y, float w, float h, u32 color,
                  u32 cornerFill) {
    const float srcAspect = float(art.width) / float(art.height);
    const float dstAspect = w / h;
    float sx = 0.f, sy = 0.f;
    float sw = float(art.width), sh = float(art.height);
    if (srcAspect > dstAspect) {
        sw = sh * dstAspect;
        sx = (float(art.width) - sw) * .5f;
    } else if (srcAspect < dstAspect) {
        sh = sw / dstAspect;
        sy = (float(art.height) - sh) * .5f;
    }
    r.sprite(art, sx, sy, sw, sh, x, y, w, h, color);
    maskArtCorners(r, x, y, w, h, cornerFill);
}

void drawWrapped(const text::Font& font, gfx::Renderer& r,
                 float x, float y, float maxWidth, int maxLines,
                 const std::string& value, u32 color) {
    if (value.empty() || maxLines <= 0) return;
    const char* p = value.c_str();
    char line[128] = {};
    int len = 0;
    int lines = 0;

    while (*p && lines < maxLines) {
        while (*p == ' ') ++p;
        const char* word = p;
        while (*p && *p != ' ') ++p;
        const int wordLen = int(p - word);
        if (wordLen <= 0) break;

        char candidate[128];
        const int addSpace = len > 0 ? 1 : 0;
        const int nextLen = rsClamp(len + addSpace + wordLen, 0, 127);
        std::memcpy(candidate, line, size_t(len));
        if (addSpace) candidate[len] = ' ';
        std::memcpy(candidate + len + addSpace, word,
                    size_t(nextLen - len - addSpace));
        candidate[nextLen] = '\0';

        if (len > 0 && font.measure(candidate) > maxWidth) {
            font.draw(r, x, y + lines * font.lineHeight(), line, color);
            lines++;
            len = 0;
            line[0] = '\0';
            if (lines >= maxLines) break;
        }
        if (len > 0) line[len++] = ' ';
        const int copy = rsClamp(wordLen, 0, 127 - len);
        std::memcpy(line + len, word, size_t(copy));
        len += copy;
        line[len] = '\0';
    }
    if (len > 0 && lines < maxLines)
        font.draw(r, x, y + lines * font.lineHeight(), line, color);
}

void drawEllipsized(const text::Font& font, gfx::Renderer& r,
                    float x, float y, float maxWidth,
                    const std::string& value, u32 color,
                    text::Align align) {
    if (font.measure(value.c_str()) <= maxWidth) {
        font.draw(r, x, y, value.c_str(), color, align);
        return;
    }

    char source[96];
    std::snprintf(source, sizeof source, "%s", value.c_str());
    int len = int(std::strlen(source));
    char output[100];
    while (len > 0) {
        do {
            --len;
        } while (len > 0 &&
                 (static_cast<unsigned char>(source[len]) & 0xC0u) == 0x80u);
        std::snprintf(output, sizeof output, "%.*s...", len, source);
        if (font.measure(output) <= maxWidth) {
            font.draw(r, x, y, output, color, align);
            return;
        }
    }
    font.draw(r, x, y, "...", color, align);
}

void drawCoverPlaceholder(gfx::Renderer& r, const App::Fonts& fonts,
                          const theme::Palette& pal,
                          const db::GameEntry& game, float x, float y,
                          float w, float h, u32 alpha) {
    x = float(int(x));
    y = float(int(y));
    w = float(int(w));
    h = float(int(h));
    ui::prim::roundedRect(
        r, x, y, w, h, 4.f,
        rsWithAlpha(pal.tileBg, rsAlphaOf(pal.tileBg) * alpha / 255u));
    (void)fonts;
    ui::prim::roundedOutline(
        r, x + 4.f, y + 4.f, w - 8.f, h - 8.f, 4.f,
        rsWithAlpha(pal.panelOutline,
                    rsAlphaOf(pal.panelOutline) * alpha / 255u));
    const float iconSize = (w >= 100.f && h >= 64.f) ? 64.f : 32.f;
    ui::prim::iconSystem(
        r, int(game.system),
        x + float(int((w - iconSize) * .5f)),
        y + float(int((h - iconSize) * .5f)),
        iconSize,
        rsWithAlpha(pal.textSecondary, alpha),
        rsWithAlpha(pal.accent, alpha));
}

void formatLastPlayed(u64 stamp, bool clock24Hour, char* out, size_t n) {
    if (!stamp) {
        std::snprintf(out, n, "Last played: Never");
        return;
    }
    const int minute = int(stamp % 100u);
    const int hour = int((stamp / 100u) % 100u);
    const int day = int((stamp / 10000u) % 100u);
    const int month = int((stamp / 1000000u) % 100u);
    const int year = int((stamp / 100000000u) % 100u);
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        std::snprintf(out, n, "Last played: Unknown");
        return;
    }
    if (clock24Hour) {
        std::snprintf(out, n, "Last played %02d/%02d/%02d %02d:%02d",
                      month, day, year, hour, minute);
    } else {
        const char* suffix = hour < 12 ? "AM" : "PM";
        const int displayHour = (hour % 12) == 0 ? 12 : hour % 12;
        std::snprintf(out, n, "Last played %02d/%02d/%02d %d:%02d %s",
                      month, day, year, displayHour, minute, suffix);
    }
}

void formatLastPlayedDate(u64 stamp, char* out, size_t n) {
    if (!stamp) {
        std::snprintf(out, n, "Last Played: Never");
        return;
    }
    const int day = int((stamp / 10000u) % 100u);
    const int month = int((stamp / 1000000u) % 100u);
    const int year = int((stamp / 100000000u) % 100u);
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        std::snprintf(out, n, "Last Played: Unknown");
        return;
    }
    std::snprintf(out, n, "Last Played %02d/%02d/%02d", month, day, year);
}

}  // namespace

void HomeScene::enter(App& app) {
    m_catIdx  = rsClamp(app.snapshot().catIdx, 0, NUM_CATS - 1);
    m_listIdx = app.snapshot().listIdx;
    m_inList  = app.snapshot().inList;
    m_recentIdx = app.snapshot().recentIdx;
    m_recentFocus = app.snapshot().recentFocus && !m_inList;
    m_catPos.snap(float(m_catIdx));
    m_listFocus.snap(m_inList ? 1.f : 0.f);
    m_entrance.start(0.4f);
    m_actionsOpen = false;
    m_actionIdx = 0;
    m_saveCount = 0;
    rebuildList(app);
    rebuildRecents(app);
    if (m_visible.empty()) m_inList = false;
    if (m_recentVisible.empty()) m_recentFocus = false;
    m_listIdx = rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
    if (m_listIdx < 0) m_listIdx = 0;
    m_scroll.snap(float(gridStartRow(m_listIdx, int(m_visible.size()))));
}

void HomeScene::rebuildList(App& app) {
    m_visible.clear();
    const Category& c = cat(m_catIdx);
    if (c.systemIdx >= 0) {
        for (const auto& g : app.index().games(db::System(c.systemIdx)))
            m_visible.push_back(&g);
    } else if (c.systemIdx == CATEGORY_FAVORITES) {
        for (u32 hash : app.library().favorites())
            if (const db::GameEntry* game = app.index().byHash(hash))
                m_visible.push_back(game);
    }
    m_lastIndexGen = app.index().generation();
    refreshSelection(app);
}

void HomeScene::rebuildRecents(App& app) {
    m_recentVisible.clear();
    for (u32 hash : app.library().recents()) {
        if (const db::GameEntry* game = app.index().byHash(hash))
            m_recentVisible.push_back(game);
        if (int(m_recentVisible.size()) == RECENT_VISIBLE) break;
    }
    m_recentIdx =
        rsClamp(m_recentIdx, 0, int(m_recentVisible.size()) - 1);
    if (m_recentIdx < 0) m_recentIdx = 0;
}

void HomeScene::refreshSelection(App& app) {
    m_selCore = nullptr;
    m_selMultiCore = false;
    if (m_visible.empty()) return;
    const auto& g =
        *m_visible[size_t(rsClamp(m_listIdx, 0, int(m_visible.size()) - 1))];
    m_selCore = app.cores().resolve(g);
    m_selMultiCore = app.cores().hasChoice(g.system);
    m_selMeta = db::loadMeta(g);   /* cached here, not re-read every frame */
}

void HomeScene::updateCats(App& app) {
    const auto& pad = app.pad();
    if (m_recentFocus) {
        const int count = int(m_recentVisible.size());
        if (pad.navPressed(PSP_CTRL_LEFT) && m_recentIdx > 0)
            m_recentIdx--;
        if (pad.navPressed(PSP_CTRL_RIGHT) && m_recentIdx + 1 < count)
            m_recentIdx++;
        if (pad.navPressed(PSP_CTRL_UP) ||
            pad.isPressed(PSP_CTRL_CIRCLE)) {
            m_recentFocus = false;
            return;
        }
        if (count <= 0) {
            m_recentFocus = false;
            return;
        }
        const db::GameEntry& game =
            *m_recentVisible[size_t(m_recentIdx)];
        if (pad.isPressed(PSP_CTRL_TRIANGLE)) {
            openActions(app, game);
            return;
        }
        if (pad.isPressed(PSP_CTRL_CROSS)) {
            if (app.cores().needsChoice(game))
                openCorePicker(app, game);
            else
                app.launchGame(game);
        }
        return;
    }

    const int prev = m_catIdx;
    if (pad.navPressed(PSP_CTRL_LEFT) && m_catIdx > 0) m_catIdx--;
    if (pad.navPressed(PSP_CTRL_RIGHT) && m_catIdx < NUM_CATS - 1) m_catIdx++;
    if (m_catIdx != prev) {
        m_listIdx = 0;
        m_scroll.snap(0.f);
        rebuildList(app);
    }

    if (pad.isPressed(PSP_CTRL_CROSS)) {
        if (m_catIdx == CAT_SETTINGS) {
            app.snapshot().catIdx = m_catIdx;
            app.switchScene(std::make_unique<SettingsScene>());
            return;
        }
        if (!m_visible.empty()) {
            m_inList = true;
        } else {
            app.toast("No games in this category yet");
        }
    }
    if (pad.navPressed(PSP_CTRL_DOWN) && !m_recentVisible.empty())
        m_recentFocus = true;
    if (pad.isPressed(PSP_CTRL_TRIANGLE)) {
        app.snapshot().catIdx = m_catIdx;
        app.switchScene(std::make_unique<SettingsScene>());
        return;
    }
}

void HomeScene::updateList(App& app) {
    const auto& pad = app.pad();
    const int prevIdx = m_listIdx;
    const int count = int(m_visible.size());
    const int col = m_listIdx % GRID_COLS;
    if (pad.navPressed(PSP_CTRL_UP)) {
        if (m_listIdx >= GRID_COLS) m_listIdx -= GRID_COLS;
        else m_inList = false;
    }
    if (pad.navPressed(PSP_CTRL_DOWN)) {
        const int target = m_listIdx + GRID_COLS;
        if (target < count) {
            m_listIdx = target;
        } else if (m_listIdx / GRID_COLS < (count - 1) / GRID_COLS) {
            m_listIdx = count - 1;
        }
    }
    if (pad.navPressed(PSP_CTRL_LEFT) && col > 0) m_listIdx--;
    if (pad.navPressed(PSP_CTRL_RIGHT) &&
        col < GRID_COLS - 1 && m_listIdx + 1 < count)
        m_listIdx++;
    if (m_listIdx != prevIdx) refreshSelection(app);
    if (pad.isPressed(PSP_CTRL_CIRCLE)) m_inList = false;

    if (pad.isPressed(PSP_CTRL_TRIANGLE) && !m_visible.empty())
        openActions(app, *m_visible[size_t(m_listIdx)]);

    if (pad.isPressed(PSP_CTRL_SQUARE) && !m_visible.empty()) {
        openActions(app, *m_visible[size_t(m_listIdx)]);
        m_actionIdx = 2;  /* Save States */
    }

    if (pad.isPressed(PSP_CTRL_CROSS) && !m_visible.empty()) {
        const auto& game = *m_visible[size_t(m_listIdx)];
        if (app.cores().needsChoice(game))
            openCorePicker(app, game);
        else
            app.launchGame(game);
    }
}

void HomeScene::openActions(App& app, const db::GameEntry& game) {
    (void)app;
    m_actionsOpen = true;
    m_actionIdx = 0;
    m_saveCount = 0;
    m_actionGame = game;
    save::SlotInfo slots[save::SLOTS];
    save::querySlots(m_actionGame, slots);
    for (const auto& slot : slots)
        if (slot.exists) m_saveCount++;
    m_actionsFade.start(.18f);
}

void HomeScene::updateActions(App& app) {
    const auto& pad = app.pad();
    constexpr int ACTION_COUNT = 4;
    if (pad.navPressed(PSP_CTRL_UP) && m_actionIdx > 0) m_actionIdx--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_actionIdx < ACTION_COUNT - 1)
        m_actionIdx++;
    if (pad.isPressed(PSP_CTRL_CIRCLE) ||
        pad.isPressed(PSP_CTRL_TRIANGLE)) {
        m_actionsOpen = false;
        return;
    }
    if (!pad.isPressed(PSP_CTRL_CROSS)) return;

    const db::GameEntry& game = m_actionGame;
    switch (m_actionIdx) {
        case 0:
            m_actionsOpen = false;
            if (app.cores().needsChoice(game))
                openCorePicker(app, game);
            else
                app.launchGame(game);
            break;
        case 1:
            app.library().toggleFavorite(game.pathHash);
            app.toast(app.library().isFavorite(game.pathHash)
                          ? "Added to Favorites"
                          : "Removed from Favorites");
            if (cat(m_catIdx).systemIdx == CATEGORY_FAVORITES) {
                m_actionsOpen = false;
                rebuildList(app);
                m_listIdx =
                    rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
                if (m_listIdx < 0) m_listIdx = 0;
                if (m_visible.empty()) m_inList = false;
            }
            break;
        case 2: {
            char msg[64];
            if (m_saveCount > 0)
                std::snprintf(msg, sizeof msg, "%d save state%s · load in game",
                              m_saveCount, m_saveCount == 1 ? "" : "s");
            else
                std::snprintf(msg, sizeof msg, "No save states yet");
            app.toast(msg);
            break;
        }
        case 3:
            m_actionsOpen = false;
            openCorePicker(app, game);
            break;
    }
}

/* ---------------------------------------------------------------------- */
/* Core picker                                                             */
/* ---------------------------------------------------------------------- */

void HomeScene::openCorePicker(App& app, const db::GameEntry& game) {
    m_pickerCores = app.cores().coresFor(game.system);
    if (m_pickerCores.empty()) {
        app.launchGame(game);   /* reuses its "no core installed" toast */
        return;
    }
    m_pickerGame = game;
    m_pickerCurrent = app.cores().resolve(game);
    m_pickerOpen = true;
    m_pickerFade.start(0.18f);

    /* Preselect what a plain launch would use. */
    m_pickerIdx = 0;
    for (size_t i = 0; i < m_pickerCores.size(); i++)
        if (m_pickerCores[i] == m_pickerCurrent) m_pickerIdx = int(i);
}

void HomeScene::updatePicker(App& app) {
    const auto& pad = app.pad();
    if (pad.navPressed(PSP_CTRL_UP) && m_pickerIdx > 0) m_pickerIdx--;
    if (pad.navPressed(PSP_CTRL_DOWN) &&
        m_pickerIdx < int(m_pickerCores.size()) - 1)
        m_pickerIdx++;

    if (pad.isPressed(PSP_CTRL_CIRCLE)) m_pickerOpen = false;

    if (pad.isPressed(PSP_CTRL_CROSS)) {
        m_pickerOpen = false;
        /* GameSession persists the pick once the core actually boots. */
        app.launchGame(m_pickerGame, m_pickerCores[size_t(m_pickerIdx)]);
    }
}

void HomeScene::drawPicker(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_pickerFade.t);
    const u32 a = u32(t * 255.f);

    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
           rsWithAlpha(pal.scrim, a * 130u / 255u));

    constexpr int VISIBLE = 5;
    const int visible = rsClamp(int(m_pickerCores.size()), 1, VISIBLE);
    const int first = rsClamp(m_pickerIdx - 2, 0,
                              int(m_pickerCores.size()) - visible);
    const float rowH = 32.f;
    const float pw = 288.f;
    const float ph = 64.f + rowH * float(visible);
    const float px = (RS_SCREEN_W - pw) / 2.f;
    const float py = (RS_SCREEN_H - ph) / 2.f - (1.f - t) * 10.f;

    ui::prim::roundedRect(r, px, py, pw, ph, 8.f,
                          rsWithAlpha(pal.menuBg, a));
    ui::prim::roundedOutline(r, px, py, pw, ph, 8.f,
                             rsWithAlpha(pal.panelOutline, a));

    fonts.small.draw(r, px + 16.f, py + 8.f, "RUN WITH",
                     rsWithAlpha(pal.accent, a), text::Align::Left);
    r.setScissor(int(px + 16.f), int(py + 24.f), int(pw - 32.f), 12);
    fonts.small.draw(r, px + 16.f, py + 24.f, m_pickerGame.name.c_str(),
                     rsWithAlpha(pal.textDim, a), text::Align::Left);
    r.resetScissor();

    r.setScissor(int(px + 16.f), int(py + 48.f), int(pw - 32.f),
                 int(rowH * visible));
    float ry = py + 48.f;
    for (int i = first; i < first + visible; i++, ry += rowH) {
        const CoreInfo& c = *m_pickerCores[size_t(i)];
        const bool focused = i == m_pickerIdx;
        if (focused)
            ui::prim::focusRow(
                r, px + 16.f, ry, pw - 32.f, 32.f,
                rsWithAlpha(pal.tileFocusBg,
                            rsAlphaOf(pal.tileFocusBg) * a / 255u),
                rsWithAlpha(pal.accent, a),
                rsWithAlpha(pal.shadow,
                            rsAlphaOf(pal.shadow) * a / (255u * 2u)));
        fonts.body.draw(r, px + 32.f, ry + 8.f, c.name.c_str(),
                        rsWithAlpha(focused ? pal.textPrimary : pal.textDim, a),
                        text::Align::Left);
        /* Right column: version, plus a marker on the game's current core. */
        char right[32];
        std::snprintf(right, sizeof right, "%s%s", c.version.c_str(),
                      (&c == m_pickerCurrent) ? "  \xE2\x80\xA2" : "");
        fonts.small.draw(r, px + pw - 32.f, ry + 10.f, right,
                         rsWithAlpha(pal.textDim, a * 3u / 4u),
                         text::Align::Right);
    }
    r.resetScissor();
}

void HomeScene::update(App& app, float dt) {
    /* A finished background scan invalidates the visible list. */
    if (app.index().generation() != m_lastIndexGen) {
        rebuildList(app);
        rebuildRecents(app);
        m_listIdx = rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
        if (m_listIdx < 0) m_listIdx = 0;
        if (m_visible.empty()) m_inList = false;
    }

    if (m_pickerOpen) updatePicker(app);
    else if (m_actionsOpen) updateActions(app);
    else if (m_inList) updateList(app);
    else updateCats(app);

    app.snapshot().catIdx  = m_catIdx;
    app.snapshot().listIdx = m_listIdx;
    app.snapshot().recentIdx = m_recentIdx;
    app.snapshot().inList  = m_inList;
    app.snapshot().recentFocus = m_recentFocus;

    m_catPos.to(float(m_catIdx));
    m_catPos.update(dt, 13.f);
    m_listFocus.to(m_inList ? 1.f : 0.f);
    m_listFocus.update(dt, 11.f);
    m_scroll.to(float(gridStartRow(m_listIdx, int(m_visible.size()))));
    m_scroll.update(dt, 13.f);
    m_entrance.update(dt);
    m_pickerFade.update(dt);
    m_actionsFade.update(dt);
}

/* ---------------------------------------------------------------------- */


void HomeScene::drawHome(App& app, float alpha, float slide) {
    if (alpha <= 2.f) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const u32 a = u32(rsClamp(alpha, 0.f, 255.f));

    fonts.body.draw(r, 12.f, 28.f + slide, "BROWSE LIBRARY",
                    rsWithAlpha(pal.textPrimary, a));

    constexpr float CARD_W = 50.f;
    constexpr float CARD_H = 50.f;
    constexpr float STEP = 56.f;
    constexpr int VISIBLE_CATS = 8;
    const float first =
        rsClamp(m_catPos.v - 3.f, 0.f, float(NUM_CATS - VISIBLE_CATS));
    r.setScissor(12, int(52.f + slide), 456, int(CARD_H + 4.f));
    for (int i = 0; i < NUM_CATS; i++) {
        const float x = 12.f + (float(i) - first) * STEP;
        if (x + CARD_W < 0.f || x > RS_SCREEN_W) continue;
        const bool current = i == m_catIdx;
        const bool focused = current && !m_recentFocus;
        const float y = 52.f + slide;
        const u32 panelAlpha = u32((current ? rsAlphaOf(pal.tileFocusBg)
                                             : rsAlphaOf(pal.tileBg)) *
                                   a / 255u);
        if (focused)
            ui::prim::dropShadow(
                r, x, y, CARD_W, CARD_H, 8.f,
                rsWithAlpha(pal.shadow,
                            rsAlphaOf(pal.shadow) * a / (255u * 2u)));
        ui::prim::roundedRect(r, x, y, CARD_W, CARD_H, 8.f,
                              rsWithAlpha(current ? pal.tileFocusBg
                                                   : pal.tileBg,
                                          panelAlpha));
        ui::prim::roundedOutline(
            r, x, y, CARD_W, CARD_H, 8.f,
            focused
                ? rsWithAlpha(pal.accent, a)
                : rsWithAlpha(pal.panelOutline,
                              rsAlphaOf(pal.panelOutline) * a / 255u));
        const int systemIdx = cat(i).systemIdx;
        const char* badge = systemIdx >= 0
            ? db::systemInfo(db::System(systemIdx)).badge
            : (systemIdx == CATEGORY_FAVORITES ? "FAV" : "SET");
        if (systemIdx == CATEGORY_FAVORITES) {
            ui::prim::iconStar(
                r, x + CARD_W * .5f, y + 15.f, 10.f,
                rsWithAlpha(current ? pal.accent : pal.textSecondary, a));
        } else {
            ui::prim::iconSystem(
                r, systemIdx >= 0 ? systemIdx : SETTINGS_ICON,
                x + 9.f, y + 1.f, 32.f,
                rsWithAlpha(current ? pal.accent : pal.textSecondary, a),
                rsWithAlpha(current ? pal.textPrimary : pal.textDim, a));
        }
        fonts.small.draw(r, x + CARD_W * .5f, y + 35.f, badge,
                         rsWithAlpha(current ? pal.accent
                                             : pal.textPrimary, a),
                         text::Align::Center);
    }
    r.resetScissor();

    const Category& selectedCategory = cat(m_catIdx);
    const bool isSettings =
        selectedCategory.systemIdx == CATEGORY_SETTINGS;
    const bool isFavorites =
        selectedCategory.systemIdx == CATEGORY_FAVORITES;
    const db::GameEntry* focusedRecent =
        (m_recentFocus && !m_recentVisible.empty())
            ? m_recentVisible[size_t(m_recentIdx)] : nullptr;
    if (focusedRecent) {
        r.setScissor(12, int(108.f + slide), 456, 16);
        drawEllipsized(fonts.body, r, 12.f, 108.f + slide, 456.f,
                       focusedRecent->name,
                       rsWithAlpha(pal.textPrimary, a), text::Align::Left);
        r.resetScissor();
        char played[48];
        formatLastPlayedDate(
            app.library().lastPlayed(focusedRecent->pathHash), played,
            sizeof played);
        char recentMeta[96];
        std::snprintf(
            recentMeta, sizeof recentMeta, "%s  ·  %s",
            db::systemInfo(focusedRecent->system).displayName, played);
        r.setScissor(12, int(128.f + slide), 456, 12);
        fonts.small.draw(r, 12.f, 128.f + slide, recentMeta,
                         rsWithAlpha(pal.textDim, a));
        r.resetScissor();
    } else if (isSettings) {
        fonts.body.draw(r, 12.f, 108.f + slide, "Settings",
                        rsWithAlpha(pal.textPrimary, a));
        fonts.small.draw(r, 12.f, 128.f + slide,
                         "Theme, display and performance",
                         rsWithAlpha(pal.textDim, a));
    } else if (isFavorites) {
        fonts.body.draw(r, 12.f, 108.f + slide, "Favorites",
                        rsWithAlpha(pal.textPrimary, a));
        char favoriteCount[32];
        std::snprintf(favoriteCount, sizeof favoriteCount, "%d GAME%s",
                      int(m_visible.size()),
                      m_visible.size() == 1 ? "" : "S");
        fonts.small.draw(r, 12.f, 128.f + slide, favoriteCount,
                         rsWithAlpha(pal.textDim, a));
    } else {
        const auto& system =
            db::systemInfo(db::System(selectedCategory.systemIdx));
        fonts.body.draw(r, 12.f, 108.f + slide, system.displayName,
                        rsWithAlpha(pal.textPrimary, a));
        char gameCount[32];
        std::snprintf(gameCount, sizeof gameCount, "%d GAME%s",
                      int(m_visible.size()), m_visible.size() == 1 ? "" : "S");
        fonts.small.draw(r, 12.f, 128.f + slide, gameCount,
                         rsWithAlpha(pal.textDim, a));
    }
    r.rect(0.f, 140.f + slide, RS_SCREEN_W, 1.f,
           rsWithAlpha(pal.panelOutline,
                       rsClamp<u32>(u32(rsAlphaOf(pal.panelOutline) * 2u),
                                    28u, 76u)));

    fonts.body.draw(r, 12.f, 148.f + slide, "RECENTLY PLAYED",
                    rsWithAlpha(pal.textPrimary, a));
    const int recentCount = int(m_recentVisible.size());
    if (recentCount == 0) {
        fonts.small.draw(r, 12.f, 180.f + slide,
                         "Games you launch will appear here.",
                         rsWithAlpha(pal.textDim, a));
        return;
    }

    const int warm = int(app.time() * 18.f) % recentCount;
    app.boxart().get(*m_recentVisible[size_t(warm)]);

    for (int i = 0; i < recentCount; i++) {
        const db::GameEntry* g = m_recentVisible[size_t(i)];
        const float x = 12.f + float(i) * 56.f;
        const float y = 170.f + slide;
        const bool focused = m_recentFocus && i == m_recentIdx;
        if (focused)
            ui::prim::focusRow(
                r, x, y, 50.f, 50.f,
                rsWithAlpha(pal.tileFocusBg,
                            rsAlphaOf(pal.tileFocusBg) * a / 255u),
                rsWithAlpha(pal.accent, a),
                rsWithAlpha(pal.shadow,
                            rsAlphaOf(pal.shadow) * a / (255u * 2u)));
        else
            ui::prim::roundedRect(
                r, x, y, 50.f, 50.f, 8.f,
                rsWithAlpha(pal.tileBg,
                            rsAlphaOf(pal.tileBg) * a / 255u));
        if (const gfx::Texture* art = app.boxart().peek(*g))
            drawArtCover(r, *art, x + 8.f, y + 4.f, 34.f, 34.f,
                         rsWithAlpha(rsHex(0xFFFFFF), a),
                         rsWithAlpha(
                             surfaceAt(pal, focused ? pal.tileFocusBg
                                                    : pal.tileBg,
                                       y + 24.f),
                             a));
        else
            ui::prim::iconSystem(
                r, int(g->system), x + 9.f, y + 5.f, 32.f,
                rsWithAlpha(pal.textSecondary, a),
                rsWithAlpha(pal.accent, a));
        r.setScissor(int(x + 3.f), int(y + 39.f), 44, 11);
        drawEllipsized(
            fonts.small, r, x + 25.f, y + 39.f, 44.f, g->name,
            rsWithAlpha(focused ? pal.accent : pal.textPrimary, a),
            text::Align::Center);
        r.resetScissor();
    }
}

void HomeScene::drawBrowser(App& app, float alpha) {
    if (alpha <= 2.f || m_visible.empty()) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const u32 a = u32(rsClamp(alpha, 0.f, 255.f));
    const bool browsingFavorites =
        cat(m_catIdx).systemIdx == CATEGORY_FAVORITES;
    fonts.large.draw(
        r, 16.f, 32.f,
        browsingFavorites
            ? "Favorites"
            : db::systemInfo(db::System(cat(m_catIdx).systemIdx)).displayName,
        rsWithAlpha(pal.accent, a));
    char count[32];
    std::snprintf(count, sizeof count, "%d GAME%s",
                  int(m_visible.size()), m_visible.size() == 1 ? "" : "S");
    fonts.small.draw(r, 16.f, 56.f, count,
                     rsWithAlpha(pal.textDim, a));

    const int totalRows =
        (int(m_visible.size()) + GRID_COLS - 1) / GRID_COLS;
    const float maxStartRow =
        float(rsClamp(totalRows - GRID_ROWS, 0, totalRows));
    const float scrollRow = rsClamp(m_scroll.v, 0.f, maxStartRow);
    const int warmTile = int(app.time() * 18.f) % GRID_VISIBLE;
    const int warmIndex = int(scrollRow) * GRID_COLS + warmTile;
    if (warmIndex >= 0 && warmIndex < int(m_visible.size()))
        app.boxart().get(*m_visible[size_t(warmIndex)]);

    const ui::GridWindow window = ui::visibleGridWindow(
        int(m_visible.size()), GRID_COLS, GRID_ROWS, scrollRow);
    r.setScissor(16, int(GRID_TOP), 232, 164);
    for (int i = window.first; i < window.pastLast; i++) {
        const int row = i / GRID_COLS;
        const int col = i % GRID_COLS;
        const float x = 16.f + float(col) * GRID_STEP;
        const float y =
            float(int(GRID_TOP + (float(row) - scrollRow) * GRID_STEP));
        if (y < GRID_TOP - GRID_STEP ||
            y >= GRID_TOP + GRID_ROWS * GRID_STEP) continue;
        const bool selected = i == m_listIdx;
        const db::GameEntry& g = *m_visible[size_t(i)];
        if (selected) {
            ui::prim::focusRow(
                r, x, y, GRID_TILE, GRID_TILE,
                rsWithAlpha(pal.tileFocusBg,
                            rsAlphaOf(pal.tileFocusBg) * a / 255u),
                rsWithAlpha(pal.accent, a),
                rsWithAlpha(pal.shadow,
                            rsAlphaOf(pal.shadow) * a / (255u * 2u)));
        } else {
            ui::prim::roundedRect(
                r, x, y, GRID_TILE, GRID_TILE, 8.f,
                rsWithAlpha(pal.tileBg,
                            rsAlphaOf(pal.tileBg) * a / 255u));
        }

        const float tx = x + 8.f, ty = y + 8.f;
        if (const gfx::Texture* art = app.boxart().peek(g))
            drawArtCover(r, *art, tx, ty, 56.f, 56.f,
                         rsWithAlpha(rsHex(0xFFFFFF), a),
                         rsWithAlpha(
                             surfaceAt(pal, selected ? pal.tileFocusBg
                                                     : pal.tileBg,
                                       ty + 28.f),
                             a));
        else
            drawCoverPlaceholder(r, fonts, pal, g, tx, ty, 56.f, 56.f, a);
        r.setScissor(16, int(GRID_TOP), 232, 164);
    }
    r.resetScissor();

    const float px = 272.f, py = 40.f, pw = 192.f, ph = 192.f;
    ui::prim::roundedRect(
        r, px, py, pw, ph, 8.f,
        rsWithAlpha(pal.panelBg, rsAlphaOf(pal.panelBg) * a / 255u));
    ui::prim::roundedOutline(
        r, px, py, pw, ph, 8.f,
        rsWithAlpha(pal.panelOutline,
                    rsAlphaOf(pal.panelOutline) * a / 255u));
    const db::GameEntry& selected = *m_visible[size_t(m_listIdx)];
    const gfx::Texture* selectedArt = app.boxart().get(selected);
    constexpr float ART_SIZE = 96.f;
    const float artX = px + (pw - ART_SIZE) * .5f;
    const float artY = py + 8.f;
    ui::prim::roundedRect(
        r, artX, artY, ART_SIZE, ART_SIZE, 4.f,
        rsWithAlpha(pal.tileBg, rsAlphaOf(pal.tileBg) * a / 255u));
    if (selectedArt)
        drawArtCover(r, *selectedArt, artX, artY, ART_SIZE, ART_SIZE,
                     rsWithAlpha(rsHex(0xFFFFFF), a),
                     rsWithAlpha(
                         opaqueOver(
                             pal.tileBg,
                             opaqueOver(pal.panelBg,
                                        backgroundAt(pal,
                                                     artY + ART_SIZE * .5f))),
                         a));
    else
        drawCoverPlaceholder(r, fonts, pal, selected,
                             artX, artY, ART_SIZE, ART_SIZE, a);

    r.setScissor(int(px + 16.f), int(py + 112.f),
                 int(pw - 32.f), int(ph - 112.f));
    drawWrapped(fonts.body, r, px + 16.f, py + 112.f, pw - 32.f, 2,
                selected.name, rsWithAlpha(pal.accent, a));
    char meta[64];
    if (m_selMeta.year > 0)
        std::snprintf(meta, sizeof meta, "%d%s%s", m_selMeta.year,
                      m_selMeta.genre.empty() ? "" : "  ·  ",
                      m_selMeta.genre.c_str());
    else
        std::snprintf(meta, sizeof meta, "%s%s%s",
                      db::systemInfo(selected.system).badge,
                      m_selMeta.genre.empty() ? "" : "  ·  ",
                      m_selMeta.genre.c_str());
    fonts.small.draw(r, px + 16.f, py + 144.f, meta,
                     rsWithAlpha(pal.textSecondary, a));
    if (m_selCore) {
        char core[64];
        std::snprintf(core, sizeof core, "%s%s", m_selCore->name.c_str(),
                      m_selMultiCore ? "  ·  Core options" : "");
        fonts.small.draw(r, px + 16.f, py + 160.f, core,
                         rsWithAlpha(pal.textDim, a));
    }
    char lastPlayed[48];
    formatLastPlayed(app.library().lastPlayed(selected.pathHash),
                     cfg::get().clock24Hour,
                     lastPlayed, sizeof lastPlayed);
    fonts.small.draw(r, px + 16.f, py + 176.f, lastPlayed,
                     rsWithAlpha(pal.textDim, a));
    r.resetScissor();
}

void HomeScene::drawActions(App& app) {
    if (!m_actionsOpen) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_actionsFade.t);
    const u32 a = u32(t * 255.f);
    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
           rsWithAlpha(pal.scrim, 168u * a / 255u));

    constexpr float PX = 240.f;
    constexpr float PY = 40.f;
    constexpr float PW = 224.f;
    constexpr float ROW = 32.f;
    constexpr int COUNT = 4;
    const float ph = ROW * COUNT + 48.f;
    ui::prim::roundedRect(r, PX, PY, PW, ph, 8.f,
                          rsWithAlpha(pal.menuBg, a));
    ui::prim::roundedOutline(
        r, PX, PY, PW, ph, 8.f,
        rsWithAlpha(pal.panelOutline,
                    rsAlphaOf(pal.panelOutline) * a / 255u));

    const db::GameEntry& game = m_actionGame;
    r.setScissor(int(PX + 16.f), int(PY + 8.f), int(PW - 32.f), 12);
    fonts.small.draw(r, PX + 16.f, PY + 8.f, game.name.c_str(),
                     rsWithAlpha(pal.textDim, a));
    r.resetScissor();
    const char* labels[COUNT] = {
        "Play",
        app.library().isFavorite(game.pathHash)
            ? "Remove Favorite" : "Favorite",
        "Save States",
        "Per-game Settings",
    };
    for (int i = 0; i < COUNT; i++) {
        const float y = PY + 40.f + i * ROW;
        const bool focused = i == m_actionIdx;
        if (focused)
            ui::prim::focusRow(
                r, PX + 16.f, y, PW - 32.f, 32.f,
                rsWithAlpha(pal.tileFocusBg,
                            rsAlphaOf(pal.tileFocusBg) * a / 255u),
                rsWithAlpha(pal.accent, a),
                rsWithAlpha(pal.shadow,
                            rsAlphaOf(pal.shadow) * a / (255u * 2u)));
        ui::prim::circle(r, PX + 32.f, y + 16.f, 2.5f,
                         rsWithAlpha(focused ? pal.accent
                                             : pal.textDim, a));
        fonts.body.draw(r, PX + 48.f, y + 8.f, labels[i],
                        rsWithAlpha(focused ? pal.accent
                                           : pal.textPrimary, a));
        if (i == 2 && m_saveCount > 0) {
            char slots[16];
            std::snprintf(slots, sizeof slots, "%d", m_saveCount);
            fonts.small.draw(r, PX + PW - 24.f, y + 10.f, slots,
                             rsWithAlpha(pal.textDim, a),
                             text::Align::Right);
        }
        if (i < COUNT - 1)
            r.rect(PX + 48.f, y + 28.f, PW - 72.f, 1.f,
                   rsWithAlpha(pal.panelOutline,
                               rsAlphaOf(pal.panelOutline) * a / 255u));
    }
}

void HomeScene::draw(App& app) {
    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const float slideUp = (1.f - enter) * 10.f;
    const float enterA = enter * 255.f;
    const float focus = m_listFocus.v;

    const float homeReveal = 1.f - rsClamp(focus * 2.f, 0.f, 1.f);
    const float browserReveal =
        rsClamp((focus - .12f) / .88f, 0.f, 1.f);
    drawHome(app, homeReveal * enterA, slideUp);
    drawBrowser(app, browserReveal * 255.f);

    if (m_actionsOpen) drawActions(app);
    if (m_pickerOpen) drawPicker(app);

    if (m_pickerOpen) {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Play"},
            {ui::prim::Button::Circle, "Cancel"},
        };
        app.drawHintBar(hints, 2);
    } else if (m_actionsOpen) {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Select"},
            {ui::prim::Button::Circle, "Close"},
        };
        app.drawHintBar(hints, 2);
    } else if (m_inList) {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Play"},
            {ui::prim::Button::Square, "Save States"},
            {ui::prim::Button::Triangle, "Options"},
            {ui::prim::Button::Circle, "Back"},
        };
        app.drawHintBar(hints, 4);
    } else if (m_recentFocus) {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Play"},
            {ui::prim::Button::Triangle, "Options"},
            {ui::prim::Button::Circle, "Back"},
        };
        app.drawHintBar(hints, 3);
    } else {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Open"},
            {ui::prim::Button::Triangle, "Settings"},
        };
        app.drawHintBar(hints, 2);
    }

}

}  // namespace rs
