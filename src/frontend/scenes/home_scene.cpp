#include "frontend/scenes/home_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/settings_scene.h"

#include <pspctrl.h>

#include <cmath>
#include <cstdio>

namespace rs {

namespace {

enum IconKind { ICON_BADGE, ICON_CLOCK, ICON_STAR, ICON_GEAR };

struct Category {
    const char* name;
    IconKind icon;
    int systemIdx;     /* >= 0: index into db::System */
};

constexpr int CAT_RECENT   = 0;
constexpr int CAT_FAVS     = 1;
constexpr int CAT_SETTINGS = 11;

const Category& cat(int i) {
    static const Category CATS[] = {
        {"Recently Played", ICON_CLOCK, -1},
        {"Favorites",       ICON_STAR,  -1},
        {nullptr,           ICON_BADGE,  0},  /* names come from SystemInfo */
        {nullptr,           ICON_BADGE,  1},
        {nullptr,           ICON_BADGE,  2},
        {nullptr,           ICON_BADGE,  3},
        {nullptr,           ICON_BADGE,  4},
        {nullptr,           ICON_BADGE,  5},
        {nullptr,           ICON_BADGE,  6},
        {nullptr,           ICON_BADGE,  7},
        {nullptr,           ICON_BADGE,  8},
        {"Settings",        ICON_GEAR,  -1},
    };
    return CATS[i];
}

const char* catName(int i) {
    const Category& c = cat(i);
    if (c.systemIdx >= 0)
        return db::systemInfo(db::System(c.systemIdx)).displayName;
    return c.name;
}

u32 catAccent(int i) {
    const Category& c = cat(i);
    if (c.systemIdx >= 0)
        return rsHex(db::systemInfo(db::System(c.systemIdx)).accentRgb);
    switch (i) {
        case CAT_RECENT: return rsHex(0x56B3B4);
        case CAT_FAVS:   return rsHex(0xE8B33C);
        default:         return rsHex(0x7A8494);
    }
}

constexpr float LIST_TOP    = 64.f;
constexpr float ROW_H       = 25.f;
constexpr int   VISIBLE_ROWS = 7;

}  // namespace

void HomeScene::enter(App& app) {
    m_catIdx  = rsClamp(app.snapshot().catIdx, 0, NUM_CATS - 1);
    m_listIdx = app.snapshot().listIdx;
    m_inList  = app.snapshot().inList;
    m_catPos.snap(float(m_catIdx));
    m_listFocus.snap(m_inList ? 1.f : 0.f);
    m_entrance.start(0.4f);
    rebuildList(app);
    if (m_visible.empty()) m_inList = false;
    m_listIdx = rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
    if (m_listIdx < 0) m_listIdx = 0;
    m_scroll.snap(float(m_listIdx));
}

void HomeScene::rebuildList(App& app) {
    m_visible.clear();
    const Category& c = cat(m_catIdx);
    if (c.systemIdx >= 0) {
        for (const auto& g : app.index().games(db::System(c.systemIdx)))
            m_visible.push_back(&g);
    } else if (m_catIdx == CAT_RECENT) {
        for (u32 h : app.library().recents())
            if (const auto* g = app.index().byHash(h)) m_visible.push_back(g);
    } else if (m_catIdx == CAT_FAVS) {
        for (u32 h : app.library().favorites())
            if (const auto* g = app.index().byHash(h)) m_visible.push_back(g);
    }
    m_lastIndexGen = app.index().generation();
    refreshSelection(app);
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
    const int prev = m_catIdx;
    if (pad.navPressed(PSP_CTRL_LEFT) && m_catIdx > 0) m_catIdx--;
    if (pad.navPressed(PSP_CTRL_RIGHT) && m_catIdx < NUM_CATS - 1) m_catIdx++;
    if (m_catIdx != prev) {
        m_listIdx = 0;
        m_scroll.snap(0.f);
        rebuildList(app);
    }

    if (pad.isPressed(PSP_CTRL_CROSS)) {
        m_pulse.start(0.28f);
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
    if (pad.navPressed(PSP_CTRL_DOWN) && !m_visible.empty()) m_inList = true;
    if (pad.isPressed(PSP_CTRL_SQUARE)) app.toggleTheme();
}

void HomeScene::updateList(App& app) {
    const auto& pad = app.pad();
    const int prevIdx = m_listIdx;
    if (pad.navPressed(PSP_CTRL_UP)) {
        if (m_listIdx > 0) m_listIdx--;
        else m_inList = false;
    }
    if (pad.navPressed(PSP_CTRL_DOWN) &&
        m_listIdx < int(m_visible.size()) - 1)
        m_listIdx++;
    if (m_listIdx != prevIdx) refreshSelection(app);
    if (pad.isPressed(PSP_CTRL_CIRCLE)) m_inList = false;

    if (pad.isPressed(PSP_CTRL_TRIANGLE) && !m_visible.empty()) {
        const auto* g = m_visible[size_t(m_listIdx)];
        app.library().toggleFavorite(g->pathHash);
        app.toast(app.library().isFavorite(g->pathHash)
                      ? "Added to Favorites"
                      : "Removed from Favorites");
        if (m_catIdx == CAT_FAVS) {
            rebuildList(app);
            if (m_visible.empty()) m_inList = false;
            m_listIdx = rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
            if (m_listIdx < 0) m_listIdx = 0;
        }
    }

    if (pad.isPressed(PSP_CTRL_SQUARE) && !m_visible.empty()) {
        openCorePicker(app, *m_visible[size_t(m_listIdx)]);
    }

    if (pad.isPressed(PSP_CTRL_CROSS) && !m_visible.empty()) {
        const auto& game = *m_visible[size_t(m_listIdx)];
        if (app.cores().needsChoice(game))
            openCorePicker(app, game);
        else
            app.launchGame(game);
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

    const float rowH = 28.f;
    const float pw = 250.f;
    const float ph = 58.f + rowH * float(m_pickerCores.size());
    const float px = (RS_SCREEN_W - pw) / 2.f;
    const float py = (RS_SCREEN_H - ph) / 2.f - (1.f - t) * 10.f;

    ui::prim::roundedRect(r, px, py, pw, ph, 12.f,
                          rsWithAlpha(pal.menuBg, a));
    ui::prim::roundedOutline(r, px, py, pw, ph, 12.f,
                             rsWithAlpha(pal.panelOutline, a));

    fonts.small.draw(r, px + 16.f, py + 12.f, "RUN WITH",
                     rsWithAlpha(pal.accent, a), text::Align::Left);
    fonts.small.draw(r, px + pw - 16.f, py + 12.f,
                     m_pickerGame.name.c_str(),
                     rsWithAlpha(pal.textDim, a), text::Align::Right);

    float ry = py + 36.f;
    for (size_t i = 0; i < m_pickerCores.size(); i++, ry += rowH) {
        const CoreInfo& c = *m_pickerCores[i];
        const bool focused = int(i) == m_pickerIdx;
        if (focused)
            ui::prim::focusRow(r, px + 8.f, ry, pw - 16.f, rowH - 4.f,
                               rsWithAlpha(pal.tileFocusBg,
                                           rsAlphaOf(pal.tileFocusBg) * a /
                                               255u),
                               rsWithAlpha(pal.accent, a));
        fonts.body.draw(r, px + 22.f, ry + 3.f, c.name.c_str(),
                        rsWithAlpha(focused ? pal.textPrimary : pal.textDim, a),
                        text::Align::Left);
        /* Right column: version, plus a marker on the game's current core. */
        char right[32];
        std::snprintf(right, sizeof right, "%s%s", c.version.c_str(),
                      (&c == m_pickerCurrent) ? "  \xE2\x80\xA2" : "");
        fonts.small.draw(r, px + pw - 18.f, ry + 5.f, right,
                         rsWithAlpha(pal.textDim, a * 3u / 4u),
                         text::Align::Right);
    }
}

void HomeScene::update(App& app, float dt) {
    /* A finished background scan invalidates the visible list. */
    if (app.index().generation() != m_lastIndexGen) {
        rebuildList(app);
        m_listIdx = rsClamp(m_listIdx, 0, int(m_visible.size()) - 1);
        if (m_listIdx < 0) m_listIdx = 0;
        if (m_visible.empty()) m_inList = false;
    }

    if (m_pickerOpen) updatePicker(app);
    else if (m_inList) updateList(app);
    else updateCats(app);

    app.snapshot().catIdx  = m_catIdx;
    app.snapshot().listIdx = m_listIdx;
    app.snapshot().inList  = m_inList;

    m_catPos.to(float(m_catIdx));
    m_catPos.update(dt, 13.f);
    m_listFocus.to(m_inList ? 1.f : 0.f);
    m_listFocus.update(dt, 11.f);
    m_scroll.to(float(m_listIdx));
    m_scroll.update(dt, 13.f);
    m_entrance.update(dt);
    m_pulse.update(dt);
    m_pickerFade.update(dt);
}

/* ---------------------------------------------------------------------- */

void HomeScene::drawCatBar(App& app, float focus, float enterA,
                           float slideUp) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    const float centerX = RS_SCREEN_W / 2.f;
    const float barY = 92.f - 56.f * focus + slideUp;
    const float step = 76.f - 26.f * focus;
    const float baseSize  = 46.f - 14.f * focus;
    const float bonusSize = 16.f - 7.f * focus;

    for (int i = 0; i < NUM_CATS; i++) {
        const float off = (float(i) - m_catPos.v) * step;
        if (off < -centerX - step || off > centerX + step) continue;

        const float f = rsClamp(1.f - std::fabs(float(i) - m_catPos.v), 0.f,
                                1.f);
        float pop = 0.f;
        if (i == m_catIdx && m_pulse.running())
            pop = std::sin(ui::easeOutCubic(m_pulse.t) * 3.14159f) * 4.f;

        const float size = baseSize + bonusSize * f + pop;
        const float x = centerX + off - size / 2.f;
        const float y = barY - size / 2.f;

        const float edgeFade =
            rsClamp(1.6f - std::fabs(off) / (centerX * 0.82f), 0.f, 1.f);
        const u32 a = u32(edgeFade * enterA);

        const u32 accent = catAccent(i);
        const u32 tileColor =
            rsWithAlpha(rsLerpColor(pal.tileBg, pal.tileFocusBg, f),
                        u32(float(rsAlphaOf(pal.tileBg)) +
                            f * float(rsAlphaOf(pal.tileFocusBg) -
                                      rsAlphaOf(pal.tileBg))) *
                            a / 255u);

        ui::prim::roundedRect(r, x, y, size, size, 12.f, tileColor);
        if (f > 0.55f) {
            const u32 ringA = u32((f - 0.55f) / 0.45f * float(a));
            ui::prim::roundedOutline(r, x - 2.f, y - 2.f, size + 4.f,
                                     size + 4.f, 13.f,
                                     rsWithAlpha(accent, ringA));
        }

        const float cx = x + size / 2.f, cy = y + size / 2.f;
        const u32 iconColor = rsWithAlpha(
            rsLerpColor(pal.textSecondary, accent, 0.35f + 0.65f * f), a);
        const Category& c = cat(i);
        switch (c.icon) {
            case ICON_BADGE: {
                const auto& fnt =
                    (f > 0.5f && focus < 0.5f) ? fonts.large : fonts.body;
                fnt.draw(r, cx, cy - fnt.lineHeight() / 2.f,
                         db::systemInfo(db::System(c.systemIdx)).badge,
                         iconColor, text::Align::Center);
                break;
            }
            case ICON_CLOCK:
                ui::prim::iconClock(r, cx, cy, size * 0.28f, iconColor);
                break;
            case ICON_STAR:
                ui::prim::iconStar(r, cx, cy, size * 0.33f, iconColor);
                break;
            case ICON_GEAR:
                ui::prim::iconGear(r, cx, cy, size * 0.30f, iconColor);
                break;
        }
    }

    /* Focused category label fades out in list mode. */
    const u32 labelA = u32((1.f - focus) * enterA);
    if (labelA > 8) {
        fonts.large.drawShadow(r, centerX, 132.f + slideUp, catName(m_catIdx),
                               rsWithAlpha(pal.textPrimary, labelA),
                               pal.shadow, text::Align::Center);
    }
}

void HomeScene::drawEmptyPanel(App& app, float alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float centerX = RS_SCREEN_W / 2.f;
    const u32 a = u32(alpha);

    const float py = 168.f;
    ui::prim::roundedRect(r, 60.f, py, 360.f, 64.f, 12.f,
                          rsWithAlpha(pal.panelBg,
                                      rsAlphaOf(pal.panelBg) * a / 255u));
    ui::prim::roundedOutline(r, 60.f, py, 360.f, 64.f, 12.f,
                             rsWithAlpha(pal.panelOutline,
                                         rsAlphaOf(pal.panelOutline) * a /
                                             255u));

    const Category& c = cat(m_catIdx);
    char hint[96];
    const char* line1;
    if (!m_visible.empty()) {
        line1 = nullptr;
        const int n = int(m_visible.size());
        std::snprintf(hint, sizeof hint, "%d game%s — press × to browse", n,
                      n == 1 ? "" : "s");
    } else if (c.systemIdx >= 0) {
        line1 = "No games yet";
        std::snprintf(hint, sizeof hint, "Copy ROMs to ms0:/ROMS/%s/",
                      db::systemInfo(db::System(c.systemIdx)).dirName);
    } else if (m_catIdx == CAT_SETTINGS) {
        line1 = "Settings";
        std::snprintf(hint, sizeof hint,
                      "Themes, controls and system options");
    } else {
        line1 = "Nothing here yet";
        std::snprintf(hint, sizeof hint, "%s will appear once you play",
                      m_catIdx == CAT_FAVS ? "Favorites" : "Recent games");
    }

    if (line1) {
        fonts.body.draw(r, centerX, py + 12.f, line1,
                        rsWithAlpha(pal.textSecondary, a),
                        text::Align::Center);
        fonts.small.draw(r, centerX, py + 34.f, hint,
                         rsWithAlpha(pal.textDim, a), text::Align::Center);
    } else {
        fonts.body.draw(r, centerX, py + 22.f, hint,
                        rsWithAlpha(pal.textSecondary, a),
                        text::Align::Center);
    }
}

void HomeScene::drawGameList(App& app, float focus) {
    if (focus <= 0.02f || m_visible.empty()) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const u32 a = u32(focus * 255.f);

    /* --- rows ------------------------------------------------------------ */
    const float listX = 20.f, listW = 268.f;
    const float slide = (1.f - focus) * 26.f;

    r.setScissor(0, int(LIST_TOP) - 4, RS_SCREEN_W,
                 int(ROW_H) * VISIBLE_ROWS + 12);
    const float scrollOff = m_scroll.v - float(VISIBLE_ROWS - 1) / 2.f;
    for (int i = 0; i < int(m_visible.size()); i++) {
        const float y = LIST_TOP + (float(i) - scrollOff) * ROW_H + slide;
        if (y < LIST_TOP - ROW_H || y > LIST_TOP + VISIBLE_ROWS * ROW_H)
            continue;
        const bool sel = i == m_listIdx;
        if (sel) {
            ui::prim::roundedRect(r, listX, y, listW, ROW_H - 3.f, 8.f,
                                  rsWithAlpha(pal.tileFocusBg,
                                              rsAlphaOf(pal.tileFocusBg) * a /
                                                  255u));
            r.rect(listX + 4.f, y + 4.f, 3.f, ROW_H - 11.f,
                   rsWithAlpha(pal.accent, a));
        }
        const auto* g = m_visible[size_t(i)];
        u32 color = sel ? pal.textPrimary : pal.textSecondary;
        fonts.body.draw(r, listX + 16.f, y + 3.f, g->name.c_str(),
                        rsWithAlpha(color, a));
        if (app.library().isFavorite(g->pathHash))
            ui::prim::iconStar(r, listX + listW - 12.f, y + ROW_H / 2.f - 1.f,
                               5.f, rsWithAlpha(rsHex(0xE8B33C), a));
    }
    r.resetScissor();

    /* --- detail panel ----------------------------------------------------- */
    const float px = 300.f, pw = 160.f, py = LIST_TOP, ph = 178.f;
    ui::prim::roundedRect(r, px, py, pw, ph, 12.f,
                          rsWithAlpha(pal.panelBg,
                                      rsAlphaOf(pal.panelBg) * a / 255u));
    ui::prim::roundedOutline(r, px, py, pw, ph, 12.f,
                             rsWithAlpha(pal.panelOutline,
                                         rsAlphaOf(pal.panelOutline) * a /
                                             255u));

    const auto* g = m_visible[size_t(rsClamp(m_listIdx, 0,
                                             int(m_visible.size()) - 1))];
    const gfx::Texture* art = app.boxart().get(*g);
    const float artBox = 108.f;
    const float ax = px + (pw - artBox) / 2.f, ay = py + 12.f;
    if (art) {
        /* Fit preserving aspect. */
        float dw = artBox, dh = artBox;
        if (art->width > art->height)
            dh = artBox * float(art->height) / float(art->width);
        else
            dw = artBox * float(art->width) / float(art->height);
        r.sprite(*art, 0, 0, art->width, art->height,
                 ax + (artBox - dw) / 2.f, ay + (artBox - dh) / 2.f, dw, dh,
                 rsWithAlpha(rsHex(0xFFFFFF), a));
    } else {
        ui::prim::roundedRect(r, ax, ay, artBox, artBox, 10.f,
                              rsWithAlpha(pal.tileBg,
                                          rsAlphaOf(pal.tileBg) * a / 510u));
        const auto& info = db::systemInfo(g->system);
        fonts.large.draw(r, ax + artBox / 2.f, ay + artBox / 2.f - 10.f,
                         info.badge,
                         rsWithAlpha(rsHex(info.accentRgb), a * 3u / 4u),
                         text::Align::Center);
    }

    /* Name + facts (metadata cached in refreshSelection, not read here). */
    const db::GameMeta& meta = m_selMeta;
    float ty = ay + artBox + 8.f;
    fonts.small.draw(r, px + pw / 2.f, ty, g->name.c_str(),
                     rsWithAlpha(pal.textPrimary, a), text::Align::Center);
    ty += 15.f;
    char facts[64] = {};
    if (meta.year > 0 && !meta.genre.empty())
        std::snprintf(facts, sizeof facts, "%d · %s", meta.year,
                      meta.genre.c_str());
    else if (meta.year > 0)
        std::snprintf(facts, sizeof facts, "%d", meta.year);
    else if (!meta.genre.empty())
        std::snprintf(facts, sizeof facts, "%s", meta.genre.c_str());
    else
        std::snprintf(facts, sizeof facts, "%u KB",
                      unsigned(g->size / 1024));
    fonts.small.draw(r, px + pw / 2.f, ty, facts,
                     rsWithAlpha(pal.textDim, a), text::Align::Center);

    if (m_selCore) {
        ty += 13.f;
        char via[40];
        std::snprintf(via, sizeof via, "via %s", m_selCore->name.c_str());
        fonts.small.draw(r, px + pw / 2.f, ty, via,
                         rsWithAlpha(pal.textDim, a * 3u / 5u),
                         text::Align::Center);
    }
}

void HomeScene::draw(App& app) {
    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const float slideUp = (1.f - enter) * 18.f;
    const float enterA = enter * 255.f;
    const float focus = m_listFocus.v;

    drawCatBar(app, focus, enterA, slideUp);
    if (focus < 0.98f && m_visible.empty())
        drawEmptyPanel(app, (1.f - focus) * enterA);
    else if (focus < 0.98f && !m_visible.empty() && focus < 0.02f)
        drawEmptyPanel(app, enterA);
    drawGameList(app, focus);

    if (m_pickerOpen) {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Play"},
            {ui::prim::Button::Circle, "Cancel"},
        };
        app.drawHintBar(hints, 2);
        drawPicker(app);
    } else if (m_inList) {
        /* Advertise the core switcher only where it has something to
         * switch between. */
        App::Hint hints[4] = {{ui::prim::Button::Cross, "Play"},
                              {ui::prim::Button::Triangle, "Favorite"}};
        int n = 2;
        if (m_selMultiCore) hints[n++] = {ui::prim::Button::Square, "Core"};
        hints[n++] = {ui::prim::Button::Circle, "Back"};
        app.drawHintBar(hints, n);
    } else {
        const App::Hint hints[] = {
            {ui::prim::Button::Cross, "Open"},
            {ui::prim::Button::Square, "Theme"},
        };
        app.drawHintBar(hints, 2);
    }
}

}  // namespace rs
