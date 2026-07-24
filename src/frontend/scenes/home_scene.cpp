#include "frontend/scenes/home_scene.h"
#include "frontend/app.h"

#include <pspctrl.h>

#include <cmath>
#include <cstdio>

namespace rs {

namespace {

enum IconKind { ICON_BADGE, ICON_CLOCK, ICON_STAR, ICON_GEAR };

struct Category {
    const char* badge;   /* tile label when ICON_BADGE */
    const char* name;
    const char* romDir;  /* nullptr for non-system categories */
    u32 accentRgb;
    IconKind icon;
};

/* ROM directory names follow the layout in docs/ARCHITECTURE.md. */
constexpr Category CATS[] = {
    {nullptr, "Recently Played",  nullptr,          0x56B3B4, ICON_CLOCK},
    {nullptr, "Favorites",        nullptr,          0xE8B33C, ICON_STAR},
    {"GB",    "Game Boy",         "GameBoy",        0x7BB661, ICON_BADGE},
    {"GBC",   "Game Boy Color",   "GameBoyColor",   0x9B72CF, ICON_BADGE},
    {"GBA",   "Game Boy Advance", "GBA",            0x5C67C7, ICON_BADGE},
    {"NES",   "Nintendo",         "NES",            0xD9534F, ICON_BADGE},
    {"SNES",  "Super Nintendo",   "SNES",           0x8E7CC3, ICON_BADGE},
    {"MD",    "Genesis",          "Genesis",        0x4A90D9, ICON_BADGE},
    {"SMS",   "Master System",    "MasterSystem",   0xD64545, ICON_BADGE},
    {"GG",    "Game Gear",        "GameGear",       0x2FA4D9, ICON_BADGE},
    {"PCE",   "PC Engine",        "PCEngine",       0xF2A03D, ICON_BADGE},
    {nullptr, "Settings",         nullptr,          0x7A8494, ICON_GEAR},
};
constexpr int NUM_CATS = int(sizeof(CATS) / sizeof(CATS[0]));

constexpr float BAR_Y     = 92.f;   /* tile row center */
constexpr float TILE_STEP = 76.f;
}  // namespace

void HomeScene::enter(App&) {
    m_catPos.snap(float(m_catIdx));
    m_entrance.start(0.4f);
}

void HomeScene::update(App& app, float dt) {
    const auto& pad = app.pad();

    if (pad.navPressed(PSP_CTRL_LEFT) && m_catIdx > 0) m_catIdx--;
    if (pad.navPressed(PSP_CTRL_RIGHT) && m_catIdx < NUM_CATS - 1) m_catIdx++;
    m_catPos.to(float(m_catIdx));
    m_catPos.update(dt, 13.f);

    if (pad.isPressed(PSP_CTRL_CROSS)) m_pulse.start(0.28f);
    if (pad.isPressed(PSP_CTRL_SQUARE)) app.toggleTheme();

    m_entrance.update(dt);
    m_pulse.update(dt);
}

void HomeScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const float slideUp = (1.f - enter) * 18.f;
    const u32 enterA = u32(enter * 255.f);

    /* ---- category bar -------------------------------------------------- */
    const float centerX = RS_SCREEN_W / 2.f;
    for (int i = 0; i < NUM_CATS; i++) {
        const float off = (float(i) - m_catPos.v) * TILE_STEP;
        if (off < -centerX - TILE_STEP || off > centerX + TILE_STEP) continue;

        const float focus = rsClamp(1.f - std::fabs(float(i) - m_catPos.v),
                                    0.f, 1.f);
        /* Selection pop. */
        float pop = 0.f;
        if (i == m_catIdx && m_pulse.running())
            pop = std::sin(ui::easeOutCubic(m_pulse.t) * 3.14159f) * 4.f;

        const float size = 46.f + 16.f * focus + pop;
        const float x = centerX + off - size / 2.f;
        const float y = BAR_Y - size / 2.f + slideUp;

        /* Fade edges of the row out. */
        const float edgeFade =
            rsClamp(1.6f - std::fabs(off) / (centerX * 0.82f), 0.f, 1.f);
        const u32 a = u32(edgeFade * float(enterA) / 255.f * 255.f);

        const u32 accent = rsHex(CATS[i].accentRgb);
        const u32 tileColor =
            rsWithAlpha(rsLerpColor(pal.tileBg, pal.tileFocusBg, focus),
                        u32(float(rsAlphaOf(pal.tileBg)) +
                            focus * float(rsAlphaOf(pal.tileFocusBg) -
                                          rsAlphaOf(pal.tileBg))) *
                            a / 255u);

        ui::prim::roundedRect(r, x, y, size, size, 12.f, tileColor);
        if (focus > 0.55f) {
            const u32 ringA = u32((focus - 0.55f) / 0.45f * float(a));
            ui::prim::roundedOutline(r, x - 2.f, y - 2.f, size + 4.f,
                                     size + 4.f, 13.f,
                                     rsWithAlpha(accent, ringA));
        }

        const float cx = x + size / 2.f, cy = y + size / 2.f;
        const u32 iconColor = rsWithAlpha(
            rsLerpColor(pal.textSecondary, accent, 0.35f + 0.65f * focus), a);
        switch (CATS[i].icon) {
            case ICON_BADGE: {
                const auto& f = focus > 0.5f ? fonts.large : fonts.body;
                f.draw(r, cx, cy - f.lineHeight() / 2.f, CATS[i].badge,
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

    /* ---- focused category label ----------------------------------------- */
    const auto& cat = CATS[m_catIdx];
    fonts.large.drawShadow(r, centerX, 132.f + slideUp, cat.name,
                           rsWithAlpha(pal.textPrimary, enterA), pal.shadow,
                           text::Align::Center);

    /* ---- content panel (placeholder until the Phase 2 library lands) ---- */
    const float py = 168.f + slideUp;
    ui::prim::roundedRect(r, 60.f, py, 360.f, 64.f, 12.f,
                          rsWithAlpha(pal.panelBg,
                                      rsAlphaOf(pal.panelBg) * enterA / 255u));
    ui::prim::roundedOutline(r, 60.f, py, 360.f, 64.f, 12.f,
                             rsWithAlpha(pal.panelOutline,
                                         rsAlphaOf(pal.panelOutline) *
                                             enterA / 255u));

    char hint[96];
    const char* line1;
    if (cat.romDir) {
        line1 = "No games yet";
        std::snprintf(hint, sizeof hint, "Copy ROMs to ms0:/ROMS/%s/",
                      cat.romDir);
    } else if (cat.icon == ICON_GEAR) {
        line1 = "Settings";
        std::snprintf(hint, sizeof hint, "Themes, controls and system options");
    } else {
        line1 = "Nothing here yet";
        std::snprintf(hint, sizeof hint, "%s will appear once you play",
                      cat.icon == ICON_STAR ? "Favorites" : "Recent games");
    }
    fonts.body.draw(r, centerX, py + 12.f, line1,
                    rsWithAlpha(pal.textSecondary, enterA),
                    text::Align::Center);
    fonts.small.draw(r, centerX, py + 34.f, hint,
                     rsWithAlpha(pal.textDim, enterA), text::Align::Center);

    /* ---- hint bar -------------------------------------------------------- */
    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Open"},
        {ui::prim::Button::Square, "Theme"},
    };
    app.drawHintBar(hints, 2);
}

}  // namespace rs
