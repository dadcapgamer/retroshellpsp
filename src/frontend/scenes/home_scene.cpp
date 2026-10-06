#include "frontend/scenes/home_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/grid_window.h"
#include "frontend/ui/relative_time.h"
#include "frontend/ui/state_panel.h"
#include "frontend/ui/text_layout.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "runtime/config.h"
#include "runtime/save_manager.h"

#include <pspctrl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>

namespace rs {

namespace {

/* --- layout (480x272, measured from the redesign mockups) ---------------- */
constexpr float MARGIN = 21.f;

constexpr int   RAIL_VISIBLE = 5;
constexpr float RAIL_PITCH = 84.f;
constexpr float RAIL_X0 = 78.f;           /* centre of the first slot */
constexpr float BOX_W = 72.f, BOX_H = 88.f, BOX_TOP = 62.f;
constexpr float RAIL_LABEL_Y = 132.f;

constexpr float CARD_W = 92.f, CARD_H = 80.f, CARD_PITCH = 107.f;
constexpr float CARD_X0 = 31.f, CARD_Y = 84.f;
constexpr int   RECENT_MAX = 5;

constexpr float LIST_X = 12.f, LIST_W = 256.f;
constexpr float LIST_TOP = 63.f, LIST_ROW = 19.f;
constexpr int   LIST_VISIBLE = 9;
constexpr float PREVIEW_X = 305.f, PREVIEW_Y = 67.f;
constexpr float PREVIEW_W = 145.f, PREVIEW_H = 109.f;

constexpr float SHIFT = 34.f;             /* vertical layer travel */
constexpr float SLIDE = 44.f;             /* horizontal system-switch travel */
constexpr float SETTLE_SECONDS = 0.18f;

using ui::drawEllipsized;
using ui::drawWrapped;

const char* systemTitle(int systemId) {
    static const char* NAMES[] = {
        "GAME BOY", "GAME BOY COLOR", "GAME BOY ADVANCE", "NES",
        "SUPER NINTENDO", "GENESIS", "MASTER SYSTEM", "GAME GEAR",
        "PC ENGINE",
    };
    return NAMES[rsClamp(systemId, 0, db::SYSTEM_COUNT - 1)];
}

const char* badge(int systemId) {
    return db::systemInfo(db::System(rsClamp(systemId, 0,
                                             db::SYSTEM_COUNT - 1))).badge;
}

u32 fade(u32 color, u32 alpha) {
    return rsWithAlpha(color, rsAlphaOf(color) * alpha / 255u);
}

float layerAlpha(float pos, float index) {
    return rsClamp(1.f - std::fabs(pos - index), 0.f, 1.f);
}

int listStart(int selected, int count) {
    return rsClamp(selected - LIST_VISIBLE / 2, 0,
                   rsClamp(count - LIST_VISIBLE, 0, count));
}

/* Cover-fit art into a rect, cropping to the destination aspect. */
void drawArtCover(gfx::Renderer& r, const gfx::Texture& art, float x, float y,
                  float w, float h, u32 color) {
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
}

/* The designed fallback: flat field, dot lattice, faded system icon and the
 * system abbreviation. Used whenever artwork is absent or not yet decoded,
 * so the slot is never blank and never shows a broken-image glyph. */
void drawPlaceholder(App& app, int systemId, float x, float y, float w,
                     float h, u32 a, bool withBadge) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    x = float(int(x)); y = float(int(y));
    w = float(int(w)); h = float(int(h));
    r.rect(x, y, w, h, fade(pal.tileBg, a));
    ui::prim::dotField(r, x, y, w, h, fade(pal.fallbackDot, a));
    ui::prim::outlineRect(r, x, y, w, h, 1.f, fade(pal.divider, a));
    const float icon = h >= 96.f ? 64.f : 48.f;
    const float iconY = y + (h - icon) * .5f - (withBadge ? 6.f : 0.f);
    ui::prim::iconSystem(r, systemId, x + (w - icon) * .5f, iconY, icon,
                         rsWithAlpha(pal.textSecondary, a * 150u / 255u),
                         pal.accent);
    if (withBadge)
        app.fonts().pixelTiny.draw(r, x + w * .5f, y + h - 16.f,
                                   badge(systemId),
                                   fade(pal.textSecondary, a),
                                   text::Align::Center);
}

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

/* "FAVORITES · MOST PLAYED · 12 GAMES": only what differs from the plain
 * A-Z list is mentioned, so the default header stays "42 GAMES". */
std::string librarySubtitle(const db::ViewState& view, const std::string& query,
                            int shown) {
    std::string out;
    auto add = [&](const std::string& part) {
        if (!out.empty()) out += " \xC2\xB7 ";
        out += part;
    };
    if (view.filter != db::ViewFilter::All) add(upper(db::filterName(view.filter)));
    if (!query.empty()) add("\"" + query + "\"");
    if (view.sort != db::ViewSort::NameAZ &&
        view.filter != db::ViewFilter::RecentlyAdded)
        add(upper(db::sortName(view.sort)));
    char count[24];
    std::snprintf(count, sizeof count, "%d GAME%s", shown, shown == 1 ? "" : "S");
    add(count);
    return out;
}

/* Publisher (else developer, else genre) and year, whichever exist, joined —
 * nothing is printed for a field the metadata does not have. */
std::string metaLine(const db::GameMeta& m) {
    std::string out = !m.publisher.empty() ? m.publisher
                    : !m.developer.empty() ? m.developer : m.genre;
    if (m.year > 0) {
        if (!out.empty()) out += " \xC2\xB7 ";
        out += std::to_string(m.year);
    }
    return out;
}

constexpr const char SEARCH_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -':.";
constexpr int SEARCH_CHAR_COUNT = int(sizeof SEARCH_CHARS) - 1;
constexpr size_t SEARCH_MAX = 16;

void drawStar(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    ui::prim::iconStar(r, float(int(cx)), float(int(cy)), radius, color);
}

}  // namespace

/* ---------------------------------------------------------------------- */
/* Data                                                                    */
/* ---------------------------------------------------------------------- */

void HomeScene::enter(App& app) {
    m_nav = nav::HomeNav{};
    m_overlay = Overlay::None;
    m_query.clear();
    m_listDirty = false;
    rebuildSystems(app);

    const auto& snap = app.snapshot();
    for (size_t i = 0; i < m_systems.size(); i++)
        if (m_systems[i] == snap.systemId) m_nav.systemPos = int(i);
    m_nav.continueIdx = snap.continueIdx;
    /* Detail and overlays never survive a trip into a game. */
    m_nav.layer = snap.layer == nav::Layer::Detail ? nav::Layer::Library
                                                   : snap.layer;

    syncActiveSystem();
    rebuildList(app);
    rebuildRecents(app);
    m_nav.clamp(int(m_systems.size()), gamesInCurrent(), int(m_recents.size()),
                m_systemTotal);

    m_now = power::localTimestamp();
    m_entrance.start(0.35f);
    m_layerPos.snap(m_nav.layer == nav::Layer::Continue ? -1.f
                    : m_nav.layer == nav::Layer::Library ? 1.f : 0.f);
    m_railPos.snap(float(m_nav.systemPos));
    m_railFirst.snap(float(nav::railFirst(m_nav.systemPos,
                                          int(m_systems.size()),
                                          RAIL_VISIBLE)));
    m_contScroll.snap(float(rsClamp(m_nav.continueIdx - 2, 0,
                                    rsClamp(int(m_recents.size()) - 4, 0, 8))));
    m_scroll.snap(float(listStart(m_nav.currentGame(), gamesInCurrent())));
    m_slideX.snap(0.f);
    m_trackedHash = 0;
    m_selectionSettle = 0.f;

    /* Follow-ups requested by a failed launch on the way back here. */
    auto& after = app.snapshot();
    if (after.rescanOnHome) {
        after.rescanOnHome = false;
        if (!app.scanner().running()) {
            app.scanner().start();
            app.toast("Rescanning library...");
        }
    }
    if (after.pickerHash) {
        const u32 hash = after.pickerHash;
        after.pickerHash = 0;
        if (const db::GameEntry* game = app.index().byHash(hash))
            openCorePicker(app, *game);
    }
}

void HomeScene::shutdown(App& app) { app.library().flush(); }

void HomeScene::rebuildSystems(App& app) {
    m_systems.clear();
    for (int s = 0; s < db::SYSTEM_COUNT; s++)
        if (!app.index().games(db::System(s)).empty()) m_systems.push_back(s);
    /* No games at all is a real state with its own screen (scanning / ROM
     * folder missing / nothing found), not a rail of empty systems. */
    m_empty = m_systems.empty();
    m_romRootMissing = m_empty && !fs::exists(fs::ROM_ROOT) &&
                       !fs::exists("ms0:/roms");
}

void HomeScene::syncActiveSystem() {
    if (m_systems.empty()) return;
    m_nav.systemPos = rsClamp(m_nav.systemPos, 0, int(m_systems.size()) - 1);
    m_nav.activeSystem = m_systems[size_t(m_nav.systemPos)];
}

int HomeScene::currentSystemId() const { return m_nav.activeSystem; }

/* Builds the current system's list for its filter/sort/search and puts the
 * highlight back on the game the user last had selected there, wherever that
 * game now sits. Selection is remembered by game, not by row, so changing the
 * view (or a rescan reshuffling the list) never moves the user off their
 * game when it is still visible. */
void HomeScene::rebuildList(App& app) {
    const int system = currentSystemId();
    const auto& all = app.index().games(db::System(system));
    m_view = app.library().view(system);
    m_visible = db::buildView(all, m_view, m_query, app.library());
    m_systemTotal = int(all.size());
    m_lastIndexGen = app.index().generation();
    const int found =
        db::indexOfHash(m_visible, app.library().lastSelected(system));
    m_nav.currentGame() = found >= 0 ? found : 0;
}

void HomeScene::applyView(App& app, db::ViewState view,
                          const std::string& query) {
    app.library().setView(currentSystemId(), view);
    m_query = query;
    rebuildList(app);
    m_nav.clamp(int(m_systems.size()), gamesInCurrent(), int(m_recents.size()),
                m_systemTotal);
    m_scroll.snap(float(listStart(m_nav.currentGame(), gamesInCurrent())));
    m_trackedHash = 0;
    noteSelection(app);
}

/* The list is stale after a favorite changed under the Favorites filter.
 * Rebuild but stay at the same row, so unfavoriting several in a row works. */
void HomeScene::refreshListKeepingPosition(App& app) {
    const int keep = m_nav.currentGame();
    rebuildList(app);
    m_nav.currentGame() = rsClamp(keep, 0, std::max(gamesInCurrent() - 1, 0));
    m_nav.clamp(int(m_systems.size()), gamesInCurrent(), int(m_recents.size()),
                m_systemTotal);
    m_trackedHash = 0;
    noteSelection(app);
}

void HomeScene::rebuildRecents(App& app) {
    m_recents.clear();
    m_recentsRevision = app.library().recentsRevision();
    for (u32 hash : app.library().recents()) {
        if (const db::GameEntry* game = app.index().byHash(hash))
            m_recents.push_back(game);
        if (int(m_recents.size()) == RECENT_MAX) break;
    }
}

const db::GameEntry* HomeScene::focusedGame() const {
    switch (m_nav.layer) {
        case nav::Layer::Continue:
            return m_nav.continueIdx < int(m_recents.size())
                       ? m_recents[size_t(m_nav.continueIdx)] : nullptr;
        case nav::Layer::Library:
            return m_nav.currentGame() < int(m_visible.size())
                       ? m_visible[size_t(m_nav.currentGame())] : nullptr;
        case nav::Layer::Detail:
            return &m_detailGame;
        case nav::Layer::Systems:
            break;
    }
    return nullptr;
}

void HomeScene::noteSelection(App& app) {
    if (m_nav.currentGame() < int(m_visible.size()))
        app.library().setLastSelected(
            currentSystemId(),
            m_visible[size_t(m_nav.currentGame())]->pathHash);
}

const db::GameMeta* HomeScene::cachedMeta(u32 hash) const {
    for (const auto& e : m_metaCache)
        if (e.hash == hash) return &e.meta;
    return nullptr;
}

/* Runs only after the highlight settles: it resolves the core (reads the
 * per-game config), decodes artwork and parses the optional metadata file,
 * all Memory Stick work that must never sit on the scrolling path. */
void HomeScene::hydrateSelection(App& app) {
    const db::GameEntry* g = focusedGame();
    if (!g || m_hydratedHash == g->pathHash) return;
    m_selCore = app.cores().defaultFor(g->system);
    m_selMultiCore = app.cores().hasChoice(g->system);
    if (cfg::get().showArt) app.boxart().get(*g);
    if (const db::GameMeta* cached = cachedMeta(g->pathHash)) {
        m_selMeta = *cached;
    } else {
        m_selMeta = db::loadMeta(*g);
        MetaEntry& slot = m_metaCache[m_metaNext];
        m_metaNext = (m_metaNext + 1) % 8;
        slot.hash = g->pathHash;
        slot.meta = m_selMeta;
    }
    m_hydratedHash = g->pathHash;
}

/* ---------------------------------------------------------------------- */
/* Input                                                                   */
/* ---------------------------------------------------------------------- */

void HomeScene::switchSystem(App& app, int dir) {
    if (!m_nav.moveSystem(dir, int(m_systems.size()))) return;
    syncActiveSystem();
    m_query.clear();   /* a search belongs to the system it was typed in */
    rebuildList(app);
    m_nav.clamp(int(m_systems.size()), gamesInCurrent(), int(m_recents.size()),
                m_systemTotal);
    m_scroll.snap(float(listStart(m_nav.currentGame(), gamesInCurrent())));
    if (m_nav.layer == nav::Layer::Library) {
        m_slideX.snap(float(dir) * SLIDE);
        m_slideX.to(0.f);
    }
    noteSelection(app);
}

void HomeScene::launch(App& app, const db::GameEntry& game) {
    app.library().flush();
    app.snapshot().layer = m_nav.layer;
    app.snapshot().systemId = currentSystemId();
    app.snapshot().continueIdx = m_nav.continueIdx;
    if (app.cores().needsChoice(game)) {
        openCorePicker(app, game);
    } else {
        app.launchGame(game);
        checkLaunchError(app, game);
    }
}

/* A launch that was refused before any scene change leaves its reason on the
 * App; show it as an error state with the one action that can fix it. */
void HomeScene::checkLaunchError(App& app, const db::GameEntry& game) {
    char raw[96];
    if (app.takeLaunchError(raw, sizeof raw)) openError(app, game, raw);
}

void HomeScene::openError(App& app, const db::GameEntry& game,
                          const char* raw) {
    m_notice = launch::classify(raw);
    m_noticeMessage = m_notice.message;
    m_noticeGame = game;
    const size_t cores = app.cores().coresFor(game.system).size();
    if (m_notice.kind == launch::Kind::CoreMissing && cores == 0) {
        m_noticeMessage = std::string("No emulator is installed for ") +
            db::systemInfo(game.system).displayName +
            ". Copy a core package into ms0:/RETROSHELL/cores and restart.";
        m_notice.action = launch::Action::None;
    } else if (m_notice.action == launch::Action::ChooseCore) {
        const size_t needed = m_notice.kind == launch::Kind::CoreMissing ? 1 : 2;
        if (cores < needed) m_notice.action = launch::Action::None;
    }
    m_overlay = Overlay::Error;
    m_overlayFade.start(0.16f);
}

void HomeScene::updateError(App& app) {
    const auto& pad = app.pad();
    if (pad.isPressed(PSP_CTRL_TRIANGLE) &&
        m_notice.action != launch::Action::None) {
        const launch::Action action = m_notice.action;
        const db::GameEntry game = m_noticeGame;
        m_overlay = Overlay::None;
        if (action == launch::Action::Rescan) {
            if (!app.scanner().running()) app.scanner().start();
            app.toast("Rescanning library...");
        } else {
            openCorePicker(app, game);
        }
        return;
    }
    if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_CROSS))
        m_overlay = Overlay::None;
}

void HomeScene::toggleFavorite(App& app, const db::GameEntry& game) {
    app.library().toggleFavorite(game.pathHash);
    if (m_view.filter == db::ViewFilter::Favorites) m_listDirty = true;
    /* The Options row and the star already show the new state; a toast would
     * also cover the bottom of the popup. */
    if (m_overlay == Overlay::None)
        app.toast(app.library().isFavorite(game.pathHash)
                      ? "Added to Favorites" : "Removed from Favorites");
}

void HomeScene::openSettings(App& app) {
    app.library().flush();
    app.snapshot().layer = m_nav.layer;
    app.snapshot().systemId = currentSystemId();
    app.snapshot().continueIdx = m_nav.continueIdx;
    app.switchScene(std::make_unique<SettingsScene>());
}

void HomeScene::openOptions(App& app, const db::GameEntry& game) {
    (void)app;
    m_optionsGame = game;
    m_optionsRow = 0;
    m_confirmDelete = false;
    m_optionsHasSave = save::hasAnySave(game);
    m_overlay = Overlay::Options;
    m_overlayFade.start(0.16f);
}

void HomeScene::openDetail(App& app, const db::GameEntry& game) {
    m_detailGame = game;
    m_detailRow = 0;
    m_detailExpanded = false;
    m_detailHasSave = save::hasAnySave(game);
    if (!m_nav.openDetail()) return;
    m_trackedHash = game.pathHash;
    m_hydratedHash = 0;
    hydrateSelection(app);       /* explicit action: no settle delay */
}

void HomeScene::activateOption(App& app, int row) {
    const db::GameEntry game = m_optionsGame;
    if (row != OPT_DELETE_SAVE) m_confirmDelete = false;
    switch (row) {
        case OPT_PLAY:
            m_overlay = Overlay::None;
            launch(app, game);
            break;
        case OPT_FAVORITE:
            toggleFavorite(app, game);
            break;
        case OPT_DETAILS:
            m_overlay = Overlay::None;
            openDetail(app, game);
            break;
        case OPT_CHEATS:
            app.toast("No cheats available for this game");
            break;
        case OPT_MANUAL:
            app.toast("No manual available for this game");
            break;
        case OPT_DELETE_SAVE:
            if (!m_optionsHasSave) {
                app.toast("No save data to delete");
            } else if (!m_confirmDelete) {
                m_confirmDelete = true;      /* second X confirms */
            } else {
                const int removed = save::deleteAll(game);
                m_confirmDelete = false;
                m_optionsHasSave = false;
                m_detailHasSave = false;
                char msg[48];
                std::snprintf(msg, sizeof msg, "Deleted %d save file%s",
                              removed, removed == 1 ? "" : "s");
                app.toast(msg);
            }
            break;
        case OPT_REMOVE_RECENT: {
            if (!app.library().removeRecent(game.pathHash)) {
                app.toast("Not in Continue Playing");
                break;
            }
            rebuildRecents(app);
            m_nav.clamp(int(m_systems.size()), gamesInCurrent(),
                        int(m_recents.size()));
            app.toast("Removed from Continue Playing");
            m_overlay = Overlay::None;
            break;
        }
        case OPT_BACK:
        default:
            m_overlay = Overlay::None;
            break;
    }
}

void HomeScene::activateDetail(App& app, int row) {
    const db::GameEntry& game = m_detailGame;
    switch (row) {
        case DET_PLAY:     launch(app, game); break;
        case DET_FAVORITE: toggleFavorite(app, game); break;
        case DET_DETAILS:  m_detailExpanded = !m_detailExpanded; break;
        case DET_CHEATS:   app.toast("No cheats available for this game"); break;
        case DET_MANUAL:   app.toast("No manual available for this game"); break;
        case DET_RETURN:   m_nav.back(); break;
    }
}

void HomeScene::updateSystems(App& app) {
    const auto& pad = app.pad();
    if (pad.navPressed(PSP_CTRL_LEFT) || pad.isPressed(PSP_CTRL_LTRIGGER))
        switchSystem(app, -1);
    if (pad.navPressed(PSP_CTRL_RIGHT) || pad.isPressed(PSP_CTRL_RTRIGGER))
        switchSystem(app, 1);
    if (m_empty) {
        /* Nothing to browse yet: X scans, triangle opens Settings. */
        if (pad.isPressed(PSP_CTRL_CROSS) && !app.scanner().running()) {
            app.scanner().start();
            app.toast("Scanning for games...");
        }
        if (pad.isPressed(PSP_CTRL_TRIANGLE)) openSettings(app);
        return;
    }
    if (pad.isPressed(PSP_CTRL_UP))
        m_nav.openContinue(int(m_recents.size()));
    if (pad.isPressed(PSP_CTRL_DOWN) || pad.isPressed(PSP_CTRL_CROSS)) {
        if (m_nav.openLibrary(m_systemTotal)) {
            m_scroll.snap(float(listStart(m_nav.currentGame(),
                                          gamesInCurrent())));
            m_slideX.snap(0.f);
        } else {
            app.toast("No games for this system yet");
        }
    }
    if (pad.isPressed(PSP_CTRL_TRIANGLE)) openSettings(app);
}

void HomeScene::updateContinue(App& app) {
    const auto& pad = app.pad();
    const int count = int(m_recents.size());
    if (pad.navPressed(PSP_CTRL_LEFT) || pad.isPressed(PSP_CTRL_LTRIGGER))
        m_nav.moveContinue(-1, count);
    if (pad.navPressed(PSP_CTRL_RIGHT) || pad.isPressed(PSP_CTRL_RTRIGGER))
        m_nav.moveContinue(1, count);
    if (pad.isPressed(PSP_CTRL_DOWN) || pad.isPressed(PSP_CTRL_CIRCLE)) {
        m_nav.back();
        return;
    }
    if (pad.isPressed(PSP_CTRL_START)) {
        m_nav.home();
        return;
    }
    const db::GameEntry* game = focusedGame();
    if (!game) return;
    if (pad.isPressed(PSP_CTRL_CROSS)) launch(app, *game);
    else if (pad.isPressed(PSP_CTRL_TRIANGLE)) openOptions(app, *game);
    else if (pad.isPressed(PSP_CTRL_SQUARE)) toggleFavorite(app, *game);
}

void HomeScene::updateLibrary(App& app) {
    const auto& pad = app.pad();
    const int count = gamesInCurrent();
    bool moved = false;
    if (pad.navPressed(PSP_CTRL_UP)) moved |= m_nav.moveGame(-1, count);
    if (pad.navPressed(PSP_CTRL_DOWN)) moved |= m_nav.moveGame(1, count);
    if (moved) noteSelection(app);

    /* Horizontal input changes system without leaving the Library. */
    if (pad.navPressed(PSP_CTRL_LEFT) || pad.isPressed(PSP_CTRL_LTRIGGER))
        switchSystem(app, -1);
    if (pad.navPressed(PSP_CTRL_RIGHT) || pad.isPressed(PSP_CTRL_RTRIGGER))
        switchSystem(app, 1);

    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        m_query.clear();
        if (m_nav.back()) rebuildList(app);
        return;
    }
    if (pad.isPressed(PSP_CTRL_SELECT)) {
        openViewMenu(app);
        return;
    }
    if (pad.isPressed(PSP_CTRL_START)) {
        m_query.clear();
        if (m_nav.home()) rebuildList(app);
        return;
    }
    const db::GameEntry* game = focusedGame();
    if (!game) return;
    /* X opens the game's detail view; Play is its first action, so launching
     * is X, X — never an accident from scrolling a list. */
    if (pad.isPressed(PSP_CTRL_CROSS)) openDetail(app, *game);
    else if (pad.isPressed(PSP_CTRL_SQUARE)) toggleFavorite(app, *game);
    else if (pad.isPressed(PSP_CTRL_TRIANGLE)) openOptions(app, *game);
}

void HomeScene::updateDetail(App& app) {
    const auto& pad = app.pad();
    if (pad.navPressed(PSP_CTRL_UP) && m_detailRow > 0) m_detailRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_detailRow < DET_COUNT - 1)
        m_detailRow++;
    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        if (m_detailExpanded) m_detailExpanded = false;
        else m_nav.back();
        return;
    }
    if (pad.isPressed(PSP_CTRL_START)) {
        m_nav.home();
        return;
    }
    if (pad.isPressed(PSP_CTRL_SQUARE)) toggleFavorite(app, m_detailGame);
    if (pad.isPressed(PSP_CTRL_TRIANGLE) && m_detailExpanded && m_selMultiCore)
        openCorePicker(app, m_detailGame);
    if (pad.isPressed(PSP_CTRL_CROSS)) activateDetail(app, m_detailRow);
}

void HomeScene::updateOptions(App& app) {
    const auto& pad = app.pad();
    const int prev = m_optionsRow;
    if (pad.navPressed(PSP_CTRL_UP) && m_optionsRow > 0) m_optionsRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_optionsRow < OPT_COUNT - 1)
        m_optionsRow++;
    if (m_optionsRow != prev) m_confirmDelete = false;
    if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_TRIANGLE)) {
        m_overlay = Overlay::None;
        return;
    }
    if (pad.isPressed(PSP_CTRL_CROSS)) activateOption(app, m_optionsRow);
}

void HomeScene::openCorePicker(App& app, const db::GameEntry& game) {
    m_pickerCores = app.cores().coresFor(game.system);
    if (m_pickerCores.empty()) {
        app.launchGame(game);   /* reports "no emulator installed" */
        checkLaunchError(app, game);
        return;
    }
    m_pickerGame = game;
    m_pickerCurrent = app.cores().resolve(game);
    m_overlay = Overlay::CorePicker;
    m_overlayFade.start(0.16f);
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
    if (pad.isPressed(PSP_CTRL_CIRCLE)) m_overlay = Overlay::None;
    if (pad.isPressed(PSP_CTRL_CROSS)) {
        m_overlay = Overlay::None;
        /* GameSession persists the pick once the core actually boots. */
        app.library().flush();
        const db::GameEntry game = m_pickerGame;
        app.launchGame(game, m_pickerCores[size_t(m_pickerIdx)]);
        checkLaunchError(app, game);
    }
}

/* --- View menu: filter / sort / search ------------------------------------ */

int HomeScene::viewRows(int out[5]) const {
    int n = 0;
    out[n++] = VR_SHOW;
    out[n++] = VR_SORT;
    out[n++] = VR_SEARCH;
    if (!m_query.empty()) out[n++] = VR_CLEAR;
    out[n++] = VR_BACK;
    return n;
}

void HomeScene::openViewMenu(App&) {
    m_viewRow = 0;
    m_overlay = Overlay::ViewMenu;
    m_overlayFade.start(0.16f);
}

void HomeScene::updateViewMenu(App& app) {
    const auto& pad = app.pad();
    int rows[5];
    const int n = viewRows(rows);
    m_viewRow = rsClamp(m_viewRow, 0, n - 1);
    if (pad.navPressed(PSP_CTRL_UP) && m_viewRow > 0) m_viewRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_viewRow < n - 1) m_viewRow++;
    if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_SELECT) ||
        pad.isPressed(PSP_CTRL_START)) {
        m_overlay = Overlay::None;
        return;
    }

    const int row = rows[rsClamp(m_viewRow, 0, n - 1)];
    int dir = 0;
    if (pad.navPressed(PSP_CTRL_LEFT)) dir = -1;
    if (pad.navPressed(PSP_CTRL_RIGHT)) dir = 1;
    const bool confirm = pad.isPressed(PSP_CTRL_CROSS);
    if (confirm && (row == VR_SHOW || row == VR_SORT)) dir = 1;

    if (row == VR_SHOW && dir) {
        db::ViewState v = m_view;
        const int count = int(db::ViewFilter::Count);
        v.filter = db::ViewFilter((int(v.filter) + dir + count) % count);
        applyView(app, v, m_query);
    } else if (row == VR_SORT && dir &&
               m_view.filter != db::ViewFilter::RecentlyAdded) {
        db::ViewState v = m_view;
        const int count = int(db::ViewSort::Count);
        v.sort = db::ViewSort((int(v.sort) + dir + count) % count);
        applyView(app, v, m_query);
    } else if (confirm) {
        if (row == VR_SEARCH) {
            openSearch(app);
        } else if (row == VR_CLEAR) {
            applyView(app, m_view, std::string());
            m_viewRow = 0;
        } else if (row == VR_BACK) {
            m_overlay = Overlay::None;
        }
    }
}

void HomeScene::openSearch(App& app) {
    m_searchBuf = m_query;
    m_searchChar = 0;
    m_searchTouched = false;
    m_overlay = Overlay::Search;
    m_overlayFade.start(0.16f);
    recountSearch(app);
}

static std::string searchPreview(const std::string& buf, bool touched, int ch) {
    std::string q = buf;
    if (touched) q.push_back(SEARCH_CHARS[ch]);
    while (!q.empty() && q.back() == ' ') q.pop_back();
    return q;
}

void HomeScene::recountSearch(App& app) {
    const std::string q = searchPreview(m_searchBuf, m_searchTouched,
                                        m_searchChar);
    m_searchCount = int(db::buildView(
        app.index().games(db::System(currentSystemId())), m_view, q,
        app.library()).size());
}

/* Initials-style entry: up/down pick a letter, right locks it in, left
 * deletes. L/R shoulders jump five letters. There is no on-screen keyboard
 * because the D-pad is the whole input device. */
void HomeScene::updateSearch(App& app) {
    const auto& pad = app.pad();
    bool changed = false;
    auto step = [&](int delta) {
        m_searchChar = (m_searchChar + delta + SEARCH_CHAR_COUNT * 8) %
                       SEARCH_CHAR_COUNT;
        m_searchTouched = true;
        changed = true;
    };
    if (pad.navPressed(PSP_CTRL_UP)) step(1);
    if (pad.navPressed(PSP_CTRL_DOWN)) step(-1);
    if (pad.navPressed(PSP_CTRL_RTRIGGER)) step(5);
    if (pad.navPressed(PSP_CTRL_LTRIGGER)) step(-5);
    if (pad.navPressed(PSP_CTRL_RIGHT) && m_searchBuf.size() < SEARCH_MAX) {
        m_searchBuf.push_back(SEARCH_CHARS[m_searchChar]);
        m_searchTouched = false;
        changed = true;
    }
    if (pad.navPressed(PSP_CTRL_LEFT)) {
        if (m_searchTouched) m_searchTouched = false;
        else if (!m_searchBuf.empty()) m_searchBuf.pop_back();
        changed = true;
    }
    if (pad.isPressed(PSP_CTRL_SQUARE)) {
        m_searchBuf.clear();
        m_searchTouched = false;
        changed = true;
    }
    if (changed) recountSearch(app);
    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        m_overlay = Overlay::ViewMenu;
        return;
    }
    if (pad.isPressed(PSP_CTRL_CROSS)) {
        const std::string q =
            searchPreview(m_searchBuf, m_searchTouched, m_searchChar);
        m_overlay = Overlay::None;
        applyView(app, m_view, q);
    }
}

void HomeScene::update(App& app, float dt) {
    /* A finished background scan invalidates every held GameEntry*. */
    if (app.index().generation() != m_lastIndexGen) {
        const int previousSystem = currentSystemId();
        rebuildSystems(app);
        for (size_t i = 0; i < m_systems.size(); i++)
            if (m_systems[i] == previousSystem) m_nav.systemPos = int(i);
        syncActiveSystem();
        rebuildList(app);
        rebuildRecents(app);
        m_nav.clamp(int(m_systems.size()), gamesInCurrent(),
                    int(m_recents.size()), m_systemTotal);
        m_trackedHash = 0;
    }
    if (app.library().recentsRevision() != m_recentsRevision) {
        rebuildRecents(app);
        m_nav.clamp(int(m_systems.size()), gamesInCurrent(),
                    int(m_recents.size()), m_systemTotal);
    }
    if (m_listDirty && m_nav.layer == nav::Layer::Library &&
        m_overlay == Overlay::None) {
        m_listDirty = false;
        refreshListKeepingPosition(app);
    }

    m_nowTimer -= dt;
    if (m_nowTimer <= 0.f) {
        m_nowTimer = 1.f;
        m_now = power::localTimestamp();
    }

    if (m_overlay == Overlay::CorePicker) updatePicker(app);
    else if (m_overlay == Overlay::Options) updateOptions(app);
    else if (m_overlay == Overlay::ViewMenu) updateViewMenu(app);
    else if (m_overlay == Overlay::Search) updateSearch(app);
    else if (m_overlay == Overlay::Error) updateError(app);
    else switch (m_nav.layer) {
        case nav::Layer::Systems:  updateSystems(app); break;
        case nav::Layer::Continue: updateContinue(app); break;
        case nav::Layer::Library:  updateLibrary(app); break;
        case nav::Layer::Detail:   updateDetail(app); break;
    }

    app.snapshot().layer = m_nav.layer;
    app.snapshot().systemId = currentSystemId();
    app.snapshot().continueIdx = m_nav.continueIdx;
    app.library().setLocation(currentSystemId(), int(m_nav.layer),
                              m_nav.continueIdx);

    /* Focused-game tracking: cached metadata applies instantly, everything
     * stick-backed waits for the highlight to stop moving. */
    const db::GameEntry* focus = focusedGame();
    const u32 hash = focus ? focus->pathHash : 0;
    if (hash != m_trackedHash) {
        m_trackedHash = hash;
        m_hydratedHash = 0;
        m_selCore = nullptr;
        m_selMeta = db::GameMeta{};
        if (const db::GameMeta* cached = cachedMeta(hash)) m_selMeta = *cached;
        m_selectionSettle = hash ? SETTLE_SECONDS : 0.f;
    }
    if (m_selectionSettle > 0.f) {
        m_selectionSettle -= dt;
        if (m_selectionSettle <= 0.f) hydrateSelection(app);
    }
    /* Warm Continue artwork one image per frame while that layer is near. */
    if (cfg::get().showArt && !m_recents.empty() &&
        std::fabs(m_layerPos.v + 1.f) < .6f) {
        const int warm = int(app.time() * 18.f) % int(m_recents.size());
        app.boxart().get(*m_recents[size_t(warm)]);
    }

    /* Motion targets. */
    const float layerTarget = m_nav.layer == nav::Layer::Continue ? -1.f
                            : m_nav.layer == nav::Layer::Library  ? 1.f
                            : m_nav.layer == nav::Layer::Detail   ? 2.f : 0.f;
    m_layerPos.to(layerTarget);
    m_layerPos.update(dt, 14.f);
    m_railPos.to(float(m_nav.systemPos));
    m_railPos.update(dt, 14.f);
    m_railFirst.to(float(nav::railFirst(m_nav.systemPos,
                                        int(m_systems.size()), RAIL_VISIBLE)));
    m_railFirst.update(dt, 14.f);
    m_contScroll.to(float(rsClamp(m_nav.continueIdx - 2, 0,
                                  rsClamp(int(m_recents.size()) - 4, 0, 8))));
    m_contScroll.update(dt, 14.f);
    m_scroll.to(float(listStart(m_nav.currentGame(), gamesInCurrent())));
    if (std::fabs(m_scroll.target - m_scroll.v) > 12.f)
        m_scroll.snap(m_scroll.target);
    m_scroll.update(dt, 16.f);
    m_slideX.update(dt, 16.f);
    m_entrance.update(dt);
    m_overlayFade.update(dt);
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/* ---------------------------------------------------------------------- */

void HomeScene::drawSystems(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int n = int(m_systems.size());
    if (m_empty) {
        ui::StatePanel panel;
        char progress[40];
        if (app.scanner().running()) {
            panel.kind = ui::StatePanel::Kind::Loading;
            panel.title = "Scanning for games";
            panel.message = "Looking through ms0:/ROMS. This only happens "
                            "when the library changes.";
            std::snprintf(progress, sizeof progress, "%d files checked",
                          app.scanner().progress());
            panel.detail = progress;
        } else if (m_romRootMissing) {
            panel.kind = ui::StatePanel::Kind::Empty;
            panel.title = "ROM folder not found";
            panel.message = "Create ms0:/ROMS on the Memory Stick, copy your "
                            "games into it, then scan.";
            panel.actionLabel = "Scan for games";
            panel.actionButton = ui::prim::Button::Cross;
        } else {
            panel.kind = ui::StatePanel::Kind::Empty;
            panel.title = "No games found";
            panel.message = "Copy ROMs into ms0:/ROMS (sub-folders are fine), "
                            "then scan. GB, GBC, GBA, NES, SNES, Genesis, "
                            "SMS, Game Gear and PC Engine are supported.";
            panel.actionLabel = "Scan for games";
            panel.actionButton = ui::prim::Button::Cross;
        }
        ui::drawStatePanel(app, panel, a, dy);
        return;
    }

    const float pad = n < RAIL_VISIBLE
        ? float(RAIL_VISIBLE - n) * RAIL_PITCH * .5f : 0.f;
    auto slotX = [&](float index) {
        return RAIL_X0 + pad + (index - m_railFirst.v) * RAIL_PITCH;
    };
    const float selX = slotX(m_railPos.v);

    r.setScissor(36, 0, RS_SCREEN_W - 72, RS_SCREEN_H);
    for (int i = 0; i < n; i++) {
        const float x = slotX(float(i));
        if (x < 12.f || x > RS_SCREEN_W - 12.f) continue;
        const int system = m_systems[size_t(i)];
        const float strength =
            1.f - rsClamp(std::fabs(float(i) - m_railPos.v), 0.f, 1.f);
        const bool active = strength > .5f;
        const float size = active ? 64.f : 48.f;
        const float iconY = (active ? 66.f : 74.f) + dy;
        ui::prim::iconSystem(r, system, x - size * .5f, iconY, size,
                             rsWithAlpha(rsHex(0xFFFFFF), a), pal.accent);
        fonts.pixelSmall.draw(r, x, RAIL_LABEL_Y + dy, badge(system),
                              fade(pal.textPrimary, a), text::Align::Center);
    }
    r.resetScissor();

    /* Selection: a flat outlined box that glides to the active slot, with a
     * dot beneath it — no card, no fill. */
    ui::prim::outlineRect(r, selX - BOX_W * .5f, BOX_TOP + dy, BOX_W, BOX_H,
                          2.f, fade(pal.railOutline, a));
    ui::prim::circle(r, float(int(selX)), 160.f + dy, 2.5f,
                     fade(pal.textPrimary, a));

    const u32 leftA = m_nav.systemPos > 0 ? a : a * 70u / 255u;
    const u32 rightA = m_nav.systemPos + 1 < n ? a : a * 70u / 255u;
    ui::prim::chevron(r, ui::prim::Dir::Left, 19.f, 106.f + dy, 6.f, 2.f,
                      fade(pal.textPrimary, leftA));
    ui::prim::chevron(r, ui::prim::Dir::Right, 461.f, 106.f + dy, 6.f, 2.f,
                      fade(pal.textPrimary, rightA));

    const int sys = currentSystemId();
    fonts.pixel.drawBold(r, RS_SCREEN_W * .5f, 176.f + dy, systemTitle(sys),
                     fade(pal.textPrimary, a), text::Align::Center);
    char count[32];
    std::snprintf(count, sizeof count, "%d GAME%s", m_systemTotal,
                  m_systemTotal == 1 ? "" : "S");
    fonts.pixelSmall.draw(r, RS_SCREEN_W * .5f, 204.f + dy, count,
                          fade(pal.textSecondary, a), text::Align::Center);
}

void HomeScene::drawContinue(App& app, u32 a, float dy) {
    if (a <= 2u || m_recents.empty()) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int n = int(m_recents.size());

    fonts.pixelMedium.drawBold(r, MARGIN, 53.f + dy, "CONTINUE PLAYING",
                           fade(pal.textPrimary, a));

    /* A single recent game collapses to one compact, centred resume item. */
    auto cardX = [&](int i) {
        if (n == 1) return (RS_SCREEN_W - CARD_W) * .5f;
        return CARD_X0 + (float(i) - m_contScroll.v) * CARD_PITCH;
    };

    r.setScissor(0, 0, RS_SCREEN_W, RS_SCREEN_H);
    for (int i = 0; i < n; i++) {
        const float x = float(int(cardX(i)));
        if (x + CARD_W < 28.f || x > RS_SCREEN_W) continue;
        const db::GameEntry& g = *m_recents[size_t(i)];
        const bool selected = i == m_nav.continueIdx;
        const float y = CARD_Y + dy;
        if (const gfx::Texture* art = app.boxart().peek(g))
            drawArtCover(r, *art, x, y, CARD_W, CARD_H,
                         rsWithAlpha(rsHex(0xFFFFFF), a));
        else
            drawPlaceholder(app, int(g.system), x, y, CARD_W, CARD_H, a, true);
        if (selected)
            ui::prim::outlineRect(r, x - 3.f, y - 3.f, CARD_W + 6.f,
                                  CARD_H + 6.f, 3.f,
                                  fade(pal.accent, a));
        const float cx = x + CARD_W * .5f;
        const float textW = CARD_PITCH - 8.f;
        r.setScissor(int(cx - textW * .5f), int(y + CARD_H), int(textW), 40);
        drawEllipsized(fonts.pixelTiny, r, cx, y + CARD_H + 6.f, textW,
                       g.shown(), fade(pal.textPrimary, a), text::Align::Center);
        char when[24], meta[48];
        ui::formatRelative(app.library().lastPlayed(g.pathHash), m_now, when,
                           sizeof when);
        std::snprintf(meta, sizeof meta, "%s \xC2\xB7 %s",
                      db::systemInfo(g.system).badge, when);
        fonts.pixelTiny.draw(r, cx, y + CARD_H + 26.f, meta,
                             fade(pal.textSecondary, a), text::Align::Center);
        r.setScissor(0, 0, RS_SCREEN_W, RS_SCREEN_H);
    }
    r.resetScissor();

    if (n > 1) {
        ui::prim::chevron(r, ui::prim::Dir::Left, 14.f, 128.f + dy, 6.f, 2.f,
                          fade(pal.textPrimary,
                               m_nav.continueIdx > 0 ? a : a * 70u / 255u));
        ui::prim::chevron(r, ui::prim::Dir::Right, 466.f, 128.f + dy, 6.f, 2.f,
                          fade(pal.textPrimary,
                               m_nav.continueIdx + 1 < n ? a
                                                         : a * 70u / 255u));
    }
}

void HomeScene::drawLibrary(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int sys = currentSystemId();
    const int count = gamesInCurrent();

    /* Slide the whole body with the system switch; fade as it travels. */
    const float dx = m_slideX.v;
    const u32 body = u32(float(a) * (1.f - rsClamp(std::fabs(dx) / SLIDE,
                                                   0.f, 1.f) * .6f));

    /* Header: system icon, name, count and the L/R neighbour hints. */
    ui::prim::iconSystem(r, sys, 13.f + dx, 3.f + dy, 48.f,
                         rsWithAlpha(rsHex(0xFFFFFF), body), pal.accent);
    fonts.pixelMedium.drawBold(r, 64.f + dx, 14.f + dy, systemTitle(sys),
                           fade(pal.textPrimary, body));
    const std::string header = librarySubtitle(m_view, m_query, count);
    drawEllipsized(fonts.pixelSmall, r, 64.f + dx, 34.f + dy, 252.f, header,
                   fade(pal.textSecondary, body));

    const int n = int(m_systems.size());
    if (m_nav.systemPos > 0) {
        ui::prim::buttonGlyph(r, ui::prim::Button::L1, 326.f, 41.f + dy, 6.f,
                              fade(pal.textPrimary, a));
        fonts.pixelTiny.draw(
            r, 340.f, 36.f + dy,
            badge(m_systems[size_t(m_nav.systemPos - 1)]),
            fade(pal.textSecondary, a));
    }
    if (m_nav.systemPos + 1 < n) {
        ui::prim::buttonGlyph(r, ui::prim::Button::R1, 456.f, 41.f + dy, 6.f,
                              fade(pal.textPrimary, a));
        fonts.pixelTiny.draw(
            r, 441.f, 36.f + dy,
            badge(m_systems[size_t(m_nav.systemPos + 1)]),
            fade(pal.textSecondary, a), text::Align::Right);
    }

    if (count == 0) {
        /* The system has games, but this view of them is empty: say why and
         * how to change it. */
        ui::StatePanel panel;
        panel.kind = ui::StatePanel::Kind::Empty;
        panel.actionButton = ui::prim::Button::Select;
        panel.actionLabel = "Change view";
        std::string message;
        if (!m_query.empty()) {
            panel.title = "No matches";
            message = "Nothing in this system matches \"" + m_query + "\".";
        } else if (m_view.filter == db::ViewFilter::Favorites) {
            panel.title = "No favorites yet";
            message = "Press the square button on any game to add it here.";
        } else {
            panel.title = "No games to show";
            message = "Rescan the library from Settings.";
            panel.actionLabel = nullptr;
        }
        panel.message = message;
        ui::drawStatePanel(app, panel, body, dy + 14.f);
        return;
    }

    /* List: flat text rows, one blue selection bar, star for favorites. */
    const bool textOnly = !cfg::get().showArt;
    /* Text-only collapses the preview column entirely: rows widen to the
     * safe area and no empty image slot is left behind. */
    const float listW = textOnly ? RS_SCREEN_W - 2.f * LIST_X : LIST_W;
    const float listH = LIST_ROW * float(LIST_VISIBLE);
    const float scroll = rsClamp(m_scroll.v, 0.f,
                                 float(rsClamp(count - LIST_VISIBLE, 0, count)));
    const ui::GridWindow win =
        ui::visibleGridWindow(count, 1, LIST_VISIBLE, scroll);
    r.setScissor(0, int(LIST_TOP + dy), int(LIST_X + listW + 6.f),
                 int(listH));
    for (int i = win.first; i < win.pastLast; i++) {
        const float y = float(int(LIST_TOP + (float(i) - scroll) * LIST_ROW)) + dy;
        const db::GameEntry& g = *m_visible[size_t(i)];
        const bool selected = i == m_nav.currentGame();
        const float textY = y + (LIST_ROW - fonts.pixelSmall.lineHeight()) * .5f;
        if (selected) {
            r.rect(LIST_X + dx, y, listW, LIST_ROW, fade(pal.selectBg, body));
            ui::prim::chevron(r, ui::prim::Dir::Right, 24.f + dx,
                              y + LIST_ROW * .5f, 5.f, 2.f,
                              fade(pal.selectText, body));
        }
        const u32 ink = fade(selected ? pal.selectText : pal.textPrimary, body);
        drawEllipsized(fonts.pixelSmall, r, 36.f + dx, textY, listW - 64.f,
                       g.shown(), ink);
        if (app.library().isFavorite(g.pathHash))
            drawStar(r, LIST_X + listW - 14.f + dx, y + LIST_ROW * .5f, 6.5f,
                     ink);
    }
    r.resetScissor();

    /* Divider and, for long libraries, a position marker. */
    if (!textOnly)
        r.rect(LIST_X + listW + 6.f, LIST_TOP + dy, 1.f, listH,
               fade(pal.divider, a));
    if (count > LIST_VISIBLE) {
        const float track = listH;
        const float thumb = rsClamp(track * float(LIST_VISIBLE) / float(count),
                                    10.f, track);
        const float t = rsClamp(float(m_nav.currentGame()) /
                                    float(count - 1), 0.f, 1.f);
        r.rect(LIST_X + listW + 5.f, LIST_TOP + dy + (track - thumb) * t, 3.f,
               thumb, fade(pal.textSecondary, a));
    }
    if (textOnly) return;

    /* Preview column: artwork or the designed fallback, title, one meta line. */
    const db::GameEntry& sel = *m_visible[size_t(m_nav.currentGame())];
    const float px = PREVIEW_X + dx, py = PREVIEW_Y + dy;
    if (const gfx::Texture* art = app.boxart().peek(sel)) {
        drawArtCover(r, *art, px, py, PREVIEW_W, PREVIEW_H,
                     rsWithAlpha(rsHex(0xFFFFFF), body));
        ui::prim::outlineRect(r, px, py, PREVIEW_W, PREVIEW_H, 1.f,
                              fade(pal.divider, body));
    } else {
        drawPlaceholder(app, int(sel.system), px, py, PREVIEW_W, PREVIEW_H,
                        body, false);
    }
    r.setScissor(int(px), int(py + PREVIEW_H), int(PREVIEW_W), 48);
    drawEllipsized(fonts.pixelBody, r, px, py + PREVIEW_H + 9.f, PREVIEW_W,
                   sel.shown(), fade(pal.textPrimary, body), text::Align::Left,
                   true);
    /* Whatever metadata exists, nothing for what does not: a game with no
     * sidecar file just shows its system. */
    const std::string meta = metaLine(m_selMeta);
    drawEllipsized(fonts.pixelSmall, r, px, py + PREVIEW_H + 30.f, PREVIEW_W,
                   meta.empty() ? std::string(db::systemInfo(sel.system).displayName)
                                : meta,
                   fade(pal.textSecondary, body));
    r.resetScissor();
}

void HomeScene::drawDetail(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const db::GameEntry& g = m_detailGame;
    const bool favorite = app.library().isFavorite(g.pathHash);

    /* Artwork block (flat, 116x80) and title block. */
    constexpr float AX = 18.f, AY = 13.f, AW = 116.f, AH = 80.f;
    if (const gfx::Texture* art = app.boxart().peek(g)) {
        drawArtCover(r, *art, AX, AY + dy, AW, AH,
                     rsWithAlpha(rsHex(0xFFFFFF), a));
        ui::prim::outlineRect(r, AX, AY + dy, AW, AH, 1.f, fade(pal.divider, a));
    } else {
        drawPlaceholder(app, int(g.system), AX, AY + dy, AW, AH, a, true);
    }

    const float tx = 152.f;
    const std::string& title = g.shown();
    r.setScissor(int(tx), int(AY + dy), int(RS_SCREEN_W - tx - MARGIN), 90);
    drawEllipsized(fonts.pixelBody, r, tx, 18.f + dy,
                   RS_SCREEN_W - tx - MARGIN - (favorite ? 18.f : 0.f), title,
                   fade(pal.textPrimary, a), text::Align::Left, true);
    if (favorite) {
        const float w = std::fmin(fonts.pixelBody.measure(title.c_str()),
                                  RS_SCREEN_W - tx - MARGIN - 18.f);
        drawStar(r, tx + w + 12.f, 25.f + dy, 6.f, fade(pal.accent, a));
    }
    /* Facts only: every line exists because the data does. */
    std::string lines[4];
    int lineCount = 0;
    lines[lineCount++] = db::systemInfo(g.system).displayName;
    if (const std::string meta = metaLine(m_selMeta); !meta.empty())
        lines[lineCount++] = meta;
    if (!m_selMeta.genre.empty() && !m_selMeta.publisher.empty())
        lines[lineCount++] = m_selMeta.genre;
    if (const u32 seconds = app.library().playSeconds(g.pathHash)) {
        char played[32];
        ui::formatPlaytime(seconds, played, sizeof played);
        lines[lineCount++] = std::string("Played ") + played;
    }
    for (int i = 0; i < lineCount; i++)
        drawEllipsized(fonts.pixelSmall, r, tx, 41.f + float(i) * 15.f + dy,
                       RS_SCREEN_W - tx - MARGIN, lines[i],
                       fade(pal.textSecondary, a));
    r.resetScissor();

    /* Action list. */
    const char* labels[DET_COUNT] = {
        "Play",
        favorite ? "Remove from Favorites" : "Add to Favorites",
        "Game Details", "Cheats", "View Manual",
        m_nav.detailFrom == nav::Layer::Continue ? "Return" : "Return to Library",
    };
    constexpr float ROW_X = 11.f, ROW_W = 224.f, ROW_TOP = 99.f, ROW_STEP = 21.f;
    for (int i = 0; i < DET_COUNT; i++) {
        const float y = ROW_TOP + float(i) * ROW_STEP + dy;
        const bool selected = i == m_detailRow;
        const float textY = y + (20.f - fonts.pixelSmall.lineHeight()) * .5f;
        if (selected) {
            r.rect(ROW_X, y, ROW_W, 20.f, fade(pal.selectBg, a));
            ui::prim::chevron(r, ui::prim::Dir::Right, 23.f, y + 10.f, 5.f, 2.f,
                              fade(pal.selectText, a));
        }
        fonts.pixelSmall.draw(r, selected ? 40.f : 28.f, textY, labels[i],
                              fade(selected ? pal.selectText : pal.textPrimary,
                                   a));
    }
    r.rect(254.f, ROW_TOP + dy, 1.f, ROW_STEP * float(DET_COUNT) - 1.f,
           fade(pal.divider, a));

    /* Description, or the expanded stats for "Game Details". */
    constexpr float DX = 269.f, DW = 190.f, DY = 104.f, STEP = 15.f;
    if (m_detailExpanded) {
        char buf[64];
        float y = DY + dy;
        auto stat = [&](const char* key, const char* value) {
            fonts.pixelTiny.draw(r, DX, y, key, fade(pal.textSecondary, a));
            drawEllipsized(fonts.pixelTiny, r, DX + 76.f, y, DW - 76.f, value,
                           fade(pal.textPrimary, a));
            y += STEP;
        };
        ui::formatRelative(app.library().lastPlayed(g.pathHash), m_now, buf,
                           sizeof buf);
        stat("Last played", buf);
        if (const u32 seconds = app.library().playSeconds(g.pathHash)) {
            ui::formatPlaytime(seconds, buf, sizeof buf);
            stat("Play time", buf);
        }
        std::snprintf(buf, sizeof buf, "%d", app.library().playCount(g.pathHash));
        stat("Play count", buf);
        stat("Core", m_selCore ? m_selCore->name.c_str() : "Auto");
        if (g.size >= 1024u * 1024u)
            std::snprintf(buf, sizeof buf, "%.1f MB", double(g.size) / 1048576.0);
        else
            std::snprintf(buf, sizeof buf, "%u KB", unsigned(g.size / 1024u));
        stat("ROM size", buf);
        stat("Save data", m_detailHasSave ? "Present" : "None");
        if (!g.variant.empty()) stat("Version", g.variant.c_str());
        /* The cleaned title hides the file's real name; keep it findable. */
        stat("File", g.name.c_str());
    } else {
        drawWrapped(fonts.pixelTiny, r, DX, DY + dy, DW, STEP, 8,
                    m_selMeta.description.empty()
                        ? std::string("No description available.")
                        : m_selMeta.description,
                    fade(pal.textSecondary, a));
    }
}

void HomeScene::drawBackdrop(App& app, u32 alpha) {
    app.renderer().rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
                        rsWithAlpha(app.pal().dim, alpha * 150u / 255u));
}

void HomeScene::drawOptions(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    drawBackdrop(app, a);

    constexpr float PX = 218.f, PW = 192.f, ROW = 22.f, PAD = 6.f;
    constexpr float PY = 33.f;
    const float ph = PAD * 2.f + ROW * float(OPT_COUNT) + 6.f;
    const float py = PY + (1.f - t) * 8.f;
    r.rect(PX, py, PW, ph, fade(pal.menuBg, a));
    ui::prim::outlineRect(r, PX, py, PW, ph, 2.f, fade(pal.railOutline, a));

    const db::GameEntry& game = m_optionsGame;
    const bool favorite = app.library().isFavorite(game.pathHash);
    bool inRecents = false;
    for (u32 h : app.library().recents())
        if (h == game.pathHash) inRecents = true;

    const char* labels[OPT_COUNT] = {
        "Play",
        favorite ? "Remove from Favorites" : "Add to Favorites",
        "Game Details", "Cheats", "View Manual",
        m_confirmDelete ? "Press X to confirm" : "Delete Save Data",
        "Remove from Recent", "Back",
    };
    for (int i = 0; i < OPT_COUNT; i++) {
        /* A divider separates Back from the game actions. */
        const float y = py + PAD + float(i) * ROW + (i == OPT_BACK ? 6.f : 0.f);
        if (i == OPT_BACK)
            r.rect(PX + 8.f, y - 4.f, PW - 16.f, 1.f, fade(pal.divider, a));
        const bool selected = i == m_optionsRow;
        const bool disabled = (i == OPT_DELETE_SAVE && !m_optionsHasSave) ||
                              (i == OPT_REMOVE_RECENT && !inRecents);
        const float textY = y + (ROW - 2.f - fonts.pixelSmall.lineHeight()) * .5f
                            + 1.f;
        if (selected) {
            r.rect(PX + 6.f, y, PW - 12.f, ROW - 2.f, fade(pal.selectBg, a));
            ui::prim::chevron(r, ui::prim::Dir::Right, PX + 17.f, y + (ROW - 2.f) * .5f,
                              5.f, 2.f, fade(pal.selectText, a));
        }
        u32 ink = selected ? pal.selectText : pal.textPrimary;
        if (disabled) ink = pal.textSecondary;
        if (i == OPT_BACK) {
            ui::prim::buttonGlyph(r, ui::prim::Button::Circle, PX + 32.f,
                                  y + (ROW - 2.f) * .5f, 6.f, fade(ink, a));
            fonts.pixelSmall.draw(r, PX + 50.f, textY, labels[i], fade(ink, a));
        } else {
            fonts.pixelSmall.draw(r, selected ? PX + 30.f : PX + 14.f, textY,
                                  labels[i],
                                  fade(ink, disabled ? a * 160u / 255u : a));
        }
    }
}

void HomeScene::drawPicker(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    drawBackdrop(app, a);

    constexpr int VISIBLE = 6;
    constexpr float ROW = 22.f, PAD = 6.f, PW = 288.f;
    const int visible = rsClamp(int(m_pickerCores.size()), 1, VISIBLE);
    const int first = rsClamp(m_pickerIdx - 2, 0,
                              int(m_pickerCores.size()) - visible);
    const float ph = PAD * 2.f + 40.f + ROW * float(visible);
    const float px = (RS_SCREEN_W - PW) * .5f;
    const float py = (RS_SCREEN_H - ph) * .5f - (1.f - t) * 8.f;
    r.rect(px, py, PW, ph, fade(pal.menuBg, a));
    ui::prim::outlineRect(r, px, py, PW, ph, 2.f, fade(pal.railOutline, a));

    fonts.pixelTiny.draw(r, px + 14.f, py + PAD + 4.f, "RUN WITH",
                         fade(pal.accent, a));
    r.setScissor(int(px + 14.f), int(py + PAD + 18.f), int(PW - 28.f), 14);
    fonts.pixelSmall.draw(r, px + 14.f, py + PAD + 18.f,
                          m_pickerGame.shown().c_str(),
                          fade(pal.textSecondary, a));
    r.resetScissor();

    float y = py + PAD + 40.f;
    for (int i = first; i < first + visible; i++, y += ROW) {
        const CoreInfo& c = *m_pickerCores[size_t(i)];
        const bool selected = i == m_pickerIdx;
        const u32 ink = selected ? pal.selectText : pal.textPrimary;
        if (selected)
            r.rect(px + 6.f, y, PW - 12.f, ROW - 2.f, fade(pal.selectBg, a));
        const float textY = y + (ROW - 2.f - fonts.pixelSmall.lineHeight()) * .5f
                            + 1.f;
        const char* status = c.isNative()
            ? "NATIVE" : (c.psp1000Safe ? "ANY PSP" : "TESTING");
        fonts.pixelSmall.draw(r, px + 16.f, textY,
                              (std::string(&c == m_pickerCurrent ? "* " : "") +
                               c.name).c_str(), fade(ink, a));
        fonts.pixelTiny.draw(r, px + PW - 14.f, textY + 1.f, status,
                             fade(selected ? pal.selectText : pal.textSecondary,
                                  a),
                             text::Align::Right);
    }
}

void HomeScene::drawViewMenu(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    drawBackdrop(app, a);

    int rows[5];
    const int n = viewRows(rows);
    constexpr float PW = 280.f, ROW = 22.f, PAD = 6.f, HEAD = 22.f;
    const float ph = PAD * 2.f + HEAD + ROW * float(n);
    const float px = (RS_SCREEN_W - PW) * .5f;
    const float py = float(int((RS_SCREEN_H - ph) * .5f)) - (1.f - t) * 8.f - 8.f;
    r.rect(px, py, PW, ph, fade(pal.menuBg, a));
    ui::prim::outlineRect(r, px, py, PW, ph, 2.f, fade(pal.railOutline, a));
    fonts.pixelTiny.draw(r, px + 14.f, py + PAD + 6.f, "VIEW",
                         fade(pal.accent, a));
    fonts.pixelTiny.draw(r, px + PW - 14.f, py + PAD + 6.f,
                         systemTitle(currentSystemId()),
                         fade(pal.textSecondary, a), text::Align::Right);

    for (int i = 0; i < n; i++) {
        const float y = py + PAD + HEAD + float(i) * ROW;
        const bool selected = i == m_viewRow;
        const int row = rows[i];
        const u32 ink = selected ? pal.selectText : pal.textPrimary;
        const u32 sub = selected ? pal.selectText : pal.textSecondary;
        const float textY = y + (ROW - 2.f - fonts.pixelSmall.lineHeight()) * .5f
                            + 1.f;
        if (selected)
            r.rect(px + 6.f, y, PW - 12.f, ROW - 2.f, fade(pal.selectBg, a));
        const char* label = row == VR_SHOW ? "Show" : row == VR_SORT ? "Sort by"
                          : row == VR_SEARCH ? "Search"
                          : row == VR_CLEAR ? "Clear search" : "Back";
        fonts.pixelSmall.draw(r, px + 16.f, textY, label, fade(ink, a));
        std::string value;
        if (row == VR_SHOW) value = db::filterName(m_view.filter);
        else if (row == VR_SORT)
            value = m_view.filter == db::ViewFilter::RecentlyAdded
                        ? "Newest first" : db::sortName(m_view.sort);
        else if (row == VR_SEARCH)
            value = m_query.empty() ? std::string("Press X") : "\"" + m_query + "\"";
        const bool adjustable =
            row == VR_SHOW || (row == VR_SORT &&
                               m_view.filter != db::ViewFilter::RecentlyAdded);
        if (!value.empty()) {
            const float vx = px + PW - (adjustable ? 28.f : 16.f);
            drawEllipsized(fonts.pixelSmall, r, vx, textY, 150.f, value,
                           fade(sub, a), text::Align::Right);
            if (adjustable && selected) {
                ui::prim::chevron(r, ui::prim::Dir::Left,
                                  vx - fonts.pixelSmall.measure(value.c_str()) -
                                      10.f, y + (ROW - 2.f) * .5f, 4.f, 2.f,
                                  fade(ink, a));
                ui::prim::chevron(r, ui::prim::Dir::Right, px + PW - 16.f,
                                  y + (ROW - 2.f) * .5f, 4.f, 2.f, fade(ink, a));
            }
        }
    }
}

void HomeScene::drawSearch(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    drawBackdrop(app, a);

    constexpr float PW = 300.f, PH = 132.f;
    const float px = (RS_SCREEN_W - PW) * .5f;
    const float py = float(int((RS_SCREEN_H - PH) * .5f)) - (1.f - t) * 8.f - 8.f;
    r.rect(px, py, PW, PH, fade(pal.menuBg, a));
    ui::prim::outlineRect(r, px, py, PW, PH, 2.f, fade(pal.railOutline, a));
    fonts.pixelTiny.draw(r, px + 14.f, py + 12.f, "SEARCH",
                         fade(pal.accent, a));
    char count[40];
    std::snprintf(count, sizeof count, "%d match%s", m_searchCount,
                  m_searchCount == 1 ? "" : "es");
    fonts.pixelTiny.draw(r, px + PW - 14.f, py + 12.f, count,
                         fade(pal.textSecondary, a), text::Align::Right);

    /* Text field: locked letters, then the pending one on an accent block. */
    const float fx = px + 14.f, fy = py + 30.f, fw = PW - 28.f, fh = 26.f;
    ui::prim::outlineRect(r, fx, fy, fw, fh, 1.f, fade(pal.divider, a));
    const float ty = fy + (fh - fonts.pixelBody.lineHeight()) * .5f;
    const float lockedW = fonts.pixelBody.measure(m_searchBuf.c_str());
    fonts.pixelBody.draw(r, fx + 8.f, ty, m_searchBuf.c_str(),
                         fade(pal.textPrimary, a));
    const char pending[2] = {SEARCH_CHARS[m_searchChar] == ' ' ? '_'
                                 : SEARCH_CHARS[m_searchChar], '\0'};
    const float cw = fonts.pixelBody.measure("M") + 2.f;
    if (m_searchTouched) {
        r.rect(fx + 8.f + lockedW, fy + 3.f, cw, fh - 6.f, fade(pal.selectBg, a));
        fonts.pixelBody.draw(r, fx + 9.f + lockedW, ty, pending,
                             fade(pal.selectText, a));
    } else if (std::fmod(app.time(), 1.f) < .6f) {
        r.rect(fx + 8.f + lockedW, fy + fh - 7.f, cw, 2.f,
               fade(pal.textSecondary, a));
    }

    /* Letter strip: the pending letter in the middle, neighbours either side,
     * so the D-pad's up/down has somewhere visible to go. */
    const float sy = py + 74.f;
    for (int d = -3; d <= 3; d++) {
        const int idx = (m_searchChar + d + SEARCH_CHAR_COUNT * 4) %
                        SEARCH_CHAR_COUNT;
        char one[2] = {SEARCH_CHARS[idx] == ' ' ? '_' : SEARCH_CHARS[idx], '\0'};
        const float cx = px + PW * .5f + float(d) * 34.f;
        if (d == 0) {
            r.rect(cx - 13.f, sy - 4.f, 26.f, 26.f, fade(pal.selectBg, a));
            fonts.pixelBody.draw(r, cx, sy + 1.f, one, fade(pal.selectText, a),
                                 text::Align::Center);
        } else {
            fonts.pixelSmall.draw(r, cx, sy + 3.f, one,
                                  fade(pal.textSecondary,
                                       a * (d == -3 || d == 3 ? 90u : 190u) /
                                           255u),
                                  text::Align::Center);
        }
    }
    fonts.pixelTiny.draw(r, px + PW * .5f, py + PH - 20.f,
                         "L / R skip five letters  -  Square clears",
                         fade(pal.textSecondary, a), text::Align::Center);
}

void HomeScene::drawError(App& app) {
    app.drawBackground();
    app.drawTopBar(false);
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);

    ui::StatePanel panel;
    panel.kind = ui::StatePanel::Kind::Error;
    panel.title = m_notice.title;
    panel.message = m_noticeMessage;
    /* Name the game, trimmed, so a stack of failures is distinguishable. */
    char game[48];
    std::snprintf(game, sizeof game, "%.40s", m_noticeGame.shown().c_str());
    panel.detail = game;
    const char* label = launch::actionLabel(m_notice.action);
    if (*label) {
        panel.actionLabel = label;
        panel.actionButton = ui::prim::Button::Triangle;
    }
    ui::drawStatePanel(app, panel, a, (1.f - t) * 8.f);

    /* The action sits under the message, where the eye lands; the legend
     * only needs to say how to leave. */
    const App::Hint hints[] = {{ui::prim::Button::Circle, "Back"}};
    app.drawHintBar(hints, 1);
}

void HomeScene::drawLegend(App& app) {
    using B = ui::prim::Button;
    const bool picker = m_overlay == Overlay::CorePicker;
    if (picker) {
        const App::Hint hints[] = {{B::Cross, "Play"}, {B::Circle, "Cancel"}};
        app.drawHintBar(hints, 2);
        return;
    }
    if (m_overlay == Overlay::ViewMenu) {
        const App::Hint hints[] = {
            {B::DpadLeftRight, "Change"}, {B::Cross, "Select"},
            {B::Circle, "Close"},
        };
        app.drawHintBar(hints, 3);
        return;
    }
    if (m_overlay == Overlay::Search) {
        const App::Hint hints[] = {
            {B::DpadUp, "Letter"}, {B::DpadLeftRight, "Add / Delete"},
            {B::Cross, "Search"}, {B::Circle, "Back"},
        };
        app.drawHintBar(hints, 4);
        return;
    }
    switch (m_nav.layer) {
        case nav::Layer::Systems: {
            if (m_empty) {
                const App::Hint hints[] = {
                    {B::Cross, "Scan"}, {B::Triangle, "Settings"},
                };
                app.drawHintBar(hints, 2);
                break;
            }
            App::Hint hints[4];
            int n = 0;
            if (!m_recents.empty()) hints[n++] = {B::DpadUp, "Continue"};
            hints[n++] = {B::DpadDown, "Library"};
            hints[n++] = {B::Triangle, "Settings"};
            hints[n++] = {B::Start, "Home"};
            app.drawHintBar(hints, n);
            break;
        }
        case nav::Layer::Continue: {
            const App::Hint hints[] = {
                {B::DpadDown, "Systems"}, {B::Triangle, "Options"},
                {B::Square, "Favorite"}, {B::Cross, "Resume"},
            };
            app.drawHintBar(hints, 4);
            break;
        }
        case nav::Layer::Library: {
            const App::Hint hints[] = {
                {B::Cross, "Details"}, {B::Square, "Favorite"},
                {B::Triangle, "Options"}, {B::Select, "View"},
                {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 5);
            break;
        }
        case nav::Layer::Detail: {
            if (m_detailExpanded && m_selMultiCore) {
                const App::Hint hints[] = {
                    {B::Cross, "Confirm"}, {B::Triangle, "Core"},
                    {B::Circle, "Back"},
                };
                app.drawHintBar(hints, 3);
            } else {
                const App::Hint hints[] = {
                    {B::Cross, "Confirm"}, {B::Circle, "Back"},
                };
                app.drawHintBar(hints, 2);
            }
            break;
        }
    }
}

void HomeScene::draw(App& app) {
    if (m_overlay == Overlay::Error) {
        drawError(app);
        return;
    }
    app.drawBackground();

    const float pos = m_layerPos.v;
    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 enterA = u32(enter * 255.f);
    auto layerA = [&](float index) {
        return u32(layerAlpha(pos, index) * float(enterA));
    };
    auto offset = [&](float index) { return (index - pos) * SHIFT; };

    /* Status cluster stays for every layer except the focused Detail view;
     * the wordmark belongs to Systems and Continue only. */
    if (pos < 1.5f) app.drawTopBar(pos < .5f);

    drawContinue(app, layerA(-1.f), offset(-1.f));
    drawSystems(app, layerA(0.f), offset(0.f));
    drawLibrary(app, layerA(1.f), offset(1.f));
    drawDetail(app, layerA(2.f), offset(2.f));

    drawLegend(app);

    if (m_overlay == Overlay::Options) drawOptions(app);
    else if (m_overlay == Overlay::CorePicker) drawPicker(app);
    else if (m_overlay == Overlay::ViewMenu) drawViewMenu(app);
    else if (m_overlay == Overlay::Search) drawSearch(app);
}

}  // namespace rs
