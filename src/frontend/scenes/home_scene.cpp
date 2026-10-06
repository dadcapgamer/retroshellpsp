#include "frontend/scenes/home_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/chrome.h"
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

/* --- layout (480x272; shared grid in ui/chrome.h) ------------------------ */
namespace L = ui::layout;
using ui::fade;

/* Systems rail: five slots, selected slot in a flat container. */
constexpr int   RAIL_VISIBLE = 5;
constexpr float RAIL_PITCH = 88.f;
constexpr float RAIL_X0 = RS_SCREEN_W * .5f - 2.f * RAIL_PITCH;   /* slot 0 */
constexpr float RAIL_BOX_W = 80.f, RAIL_BOX_H = 100.f, RAIL_BOX_Y = 58.f;
constexpr float RAIL_ICON_CY = 98.f;      /* icon centre line */
constexpr float RAIL_LABEL_Y = 137.f;     /* caps top of the badge */

/* Continue Playing: exactly three cards on screen. */
constexpr int   CARDS_VISIBLE = 3;
constexpr float CARD_W = 136.f, CARD_ART_H = 102.f, CARD_PITCH = 156.f;
constexpr float CARD_Y = 72.f;
constexpr int   RECENT_MAX = 6;

/* Library: list left, one preview right. */
constexpr float LIST_X = L::MARGIN, LIST_W = 268.f;
constexpr float LIST_TOP = 60.f, LIST_ROW = L::ROW_H;
constexpr int   LIST_VISIBLE = 9;
constexpr float PREVIEW_X = 300.f, PREVIEW_Y = 60.f;
constexpr float PREVIEW_W = L::RIGHT - PREVIEW_X, PREVIEW_H = 116.f;

/* Game Detail: art left, title and actions right. */
constexpr float DETAIL_ART_X = L::MARGIN, DETAIL_ART_Y = L::CONTENT_TOP;
constexpr float DETAIL_ART_W = 148.f, DETAIL_ART_H = 112.f;
constexpr float DETAIL_X = 180.f;
constexpr float DETAIL_W = L::RIGHT - DETAIL_X;
constexpr float DETAIL_ACTIONS_Y = 116.f;
constexpr float DETAIL_ACTION_W = 212.f;
constexpr float PLAY_H = 26.f;

constexpr float SHIFT = 24.f;             /* vertical layer travel */
constexpr float SLIDE = 36.f;             /* horizontal system-switch travel */
constexpr float SETTLE_SECONDS = 0.18f;

using ui::drawEllipsized;
using ui::drawWrapped;

const char* systemTitle(int systemId) { return ui::systemName(systemId); }

const char* badge(int systemId) {
    return db::systemInfo(db::System(rsClamp(systemId, 0,
                                             db::SYSTEM_COUNT - 1))).badge;
}

float layerAlpha(float pos, float index) {
    return rsClamp(1.f - std::fabs(pos - index), 0.f, 1.f);
}

int listStart(int selected, int count) {
    return rsClamp(selected - LIST_VISIBLE / 2, 0,
                   rsClamp(count - LIST_VISIBLE, 0, count));
}

/* First card of the three on screen: the focus stays in the middle slot
 * except at either end of the row. */
int continueFirst(int selected, int count) {
    return rsClamp(selected - 1, 0, rsClamp(count - CARDS_VISIBLE, 0, count));
}

/* Cover-fit art into a rect, cropping to the destination aspect. Cards are
 * uniform tiles, so they crop; the single previews contain-fit instead. */
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

constexpr const char SEARCH_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -':.";
constexpr int SEARCH_CHAR_COUNT = int(sizeof SEARCH_CHARS) - 1;
constexpr size_t SEARCH_MAX = 16;

/* Layers paint inside the content band so vertical travel never crosses the
 * header or legend; any inner clip is intersected with it and restored to it,
 * never reset to the full screen. */
void clipContent(gfx::Renderer& r, float x, float y, float w, float h) {
    const float top = L::HEADER_RULE_Y + 1.f, bottom = L::FOOTER_RULE_Y;
    const float y0 = rsClamp(y, top, bottom), y1 = rsClamp(y + h, top, bottom);
    r.setScissor(int(x), int(y0), int(w), int(y1 - y0));
}
void restoreContent(gfx::Renderer& r) {
    clipContent(r, 0.f, 0.f, RS_SCREEN_W, RS_SCREEN_H);
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
    m_contScroll.snap(float(continueFirst(m_nav.continueIdx,
                                          int(m_recents.size()))));
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
    m_titleFade.start(0.14f);
    m_titleDir = dir;
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
            ui::systemName(int(game.system)) +
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
    /* X plays; the full Game Detail view is one row into Options. */
    if (pad.isPressed(PSP_CTRL_CROSS)) launch(app, *game);
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
    m_contScroll.to(float(continueFirst(m_nav.continueIdx,
                                        int(m_recents.size()))));
    m_contScroll.update(dt, 14.f);
    m_scroll.to(float(listStart(m_nav.currentGame(), gamesInCurrent())));
    if (std::fabs(m_scroll.target - m_scroll.v) > 12.f)
        m_scroll.snap(m_scroll.target);
    m_scroll.update(dt, 16.f);
    m_slideX.update(dt, 16.f);
    m_entrance.update(dt);
    m_titleFade.update(dt);
    m_overlayFade.update(dt);
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/* ---------------------------------------------------------------------- */

namespace {
/* Line-box top that puts a face's caps at `capsTop`. */
float capsAt(const text::Font& f, float capsTop) {
    return f.centerY(capsTop, f.capHeight());
}

/* Up to three chips in a row, stopping before one would overflow. */
float chipRow(App& app, float x, float y, float maxW, const char* const* items,
              int count, u32 alpha) {
    float cx = x;
    for (int i = 0; i < count && i < 3; i++) {
        if (!items[i] || !*items[i]) continue;
        const float w = ui::chipWidth(app, items[i]);
        if (cx + w > x + maxW) break;
        ui::chip(app, cx, y, items[i], alpha);
        cx += w + 4.f;
    }
    return cx - x;
}

/* Panel geometry shared by every popup: centred in the content band. */
struct PanelBox { float x, y, w, h; };
PanelBox centeredPanel(float w, float h, float t) {
    const float top = L::HEADER_RULE_Y, bottom = L::FOOTER_RULE_Y;
    return {float(int((RS_SCREEN_W - w) * .5f)),
            float(int(top + (bottom - top - h) * .5f + (1.f - t) * 6.f)), w, h};
}

/* Panel heading: overline label, then a title line. Returns its height. */
constexpr float PANEL_HEAD = 36.f;
void panelHeading(App& app, const PanelBox& b, const char* overline,
                  const std::string& title, const char* right, u32 a) {
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    auto& r = app.renderer();
    ui::label(app, b.x + 10.f, capsAt(fonts.monoTiny, b.y + 9.f), overline,
              fade(pal.textMuted, a));
    if (right)
        ui::label(app, b.x + b.w - 10.f, capsAt(fonts.monoTiny, b.y + 9.f),
                  right, fade(pal.textMuted, a), text::Align::Right);
    drawEllipsized(fonts.bodyStrong, r, b.x + 10.f,
                   capsAt(fonts.bodyStrong, b.y + 20.f), b.w - 20.f, title,
                   fade(pal.textPrimary, a));
}
}  // namespace

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
                            "games into it, then rescan.";
            panel.actionLabel = "Rescan Library";
            panel.actionButton = ui::prim::Button::Cross;
        } else {
            panel.kind = ui::StatePanel::Kind::Empty;
            panel.title = "No games found";
            panel.message = "Copy ROMs into ms0:/ROMS (sub-folders are fine), "
                            "then rescan.";
            panel.detail = "GB, GBC, GBA, NES, SNES, Genesis, SMS, Game Gear, PC Engine";
            panel.actionLabel = "Rescan Library";
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
    const float selX = ui::snap(slotX(m_railPos.v));

    /* The selected slot sits in a small flat container that glides with the
     * selection; the accent underline beneath its badge is the focus. */
    ui::pixelRect(r, selX - RAIL_BOX_W * .5f, RAIL_BOX_Y + dy, RAIL_BOX_W,
                  RAIL_BOX_H, 3, fade(pal.surface, a));

    clipContent(r, L::MARGIN + 20.f, 0.f,
                RS_SCREEN_W - 2.f * (L::MARGIN + 20.f), RS_SCREEN_H);
    for (int i = 0; i < n; i++) {
        const float x = ui::snap(slotX(float(i)));
        if (x < 0.f || x > RS_SCREEN_W) continue;
        const int system = m_systems[size_t(i)];
        const bool active = std::fabs(float(i) - m_railPos.v) < .5f;
        const float size = active ? 64.f : 48.f;
        const u32 iconA = active ? a : a * 140u / 255u;
        ui::prim::iconSystem(r, system, x - size * .5f,
                             RAIL_ICON_CY - size * .5f + dy, size,
                             rsWithAlpha(rsHex(0xFFFFFF), iconA), pal.accent);
        const float labelY = capsAt(fonts.mono, RAIL_LABEL_Y) + dy;
        if (active)
            fonts.mono.drawBold(r, x, labelY, badge(system),
                                fade(pal.textPrimary, a), text::Align::Center);
        else
            fonts.mono.draw(r, x, labelY, badge(system),
                            fade(pal.textMuted, a), text::Align::Center);
    }
    restoreContent(r);
    ui::focusUnderline(app, selX, RAIL_BOX_Y + RAIL_BOX_H - 11.f + dy, 24.f, a);

    const u32 leftA = m_nav.systemPos > 0 ? a : a * 50u / 255u;
    const u32 rightA = m_nav.systemPos + 1 < n ? a : a * 50u / 255u;
    ui::prim::chevron(r, ui::prim::Dir::Left, L::MARGIN + 6.f,
                      RAIL_ICON_CY + dy, 5.f, 1.5f, fade(pal.textMuted, leftA));
    ui::prim::chevron(r, ui::prim::Dir::Right, L::RIGHT - 6.f,
                      RAIL_ICON_CY + dy, 5.f, 1.5f, fade(pal.textMuted, rightA));

    /* Name and count crossfade in from the direction of travel. */
    const float tf = ui::easeOutCubic(m_titleFade.t);
    const u32 ta = u32(float(a) * tf);
    const float tdx = ui::snap((1.f - tf) * 6.f * float(m_titleDir));
    const int sys = currentSystemId();
    fonts.display.draw(r, RS_SCREEN_W * .5f + tdx,
                       capsAt(fonts.display, 180.f) + dy, systemTitle(sys),
                       fade(pal.textPrimary, ta), text::Align::Center);
    char count[32];
    std::snprintf(count, sizeof count, "%d GAME%s", m_systemTotal,
                  m_systemTotal == 1 ? "" : "S");
    fonts.mono.draw(r, RS_SCREEN_W * .5f + tdx, capsAt(fonts.mono, 202.f) + dy,
                    count, fade(pal.textMuted, ta), text::Align::Center);
}

void HomeScene::drawContinue(App& app, u32 a, float dy) {
    if (a <= 2u || m_recents.empty()) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int n = int(m_recents.size());

    fonts.mono.drawBold(r, L::MARGIN, capsAt(fonts.mono, 40.f) + dy,
                        "CONTINUE PLAYING", fade(pal.textPrimary, a));
    fonts.small.draw(r, L::MARGIN, capsAt(fonts.small, 55.f) + dy,
                     "Recent games across all systems.",
                     fade(pal.textMuted, a));
    if (n > CARDS_VISIBLE) {
        char pos[32];
        std::snprintf(pos, sizeof pos, "%d / %d", m_nav.continueIdx + 1, n);
        ui::label(app, L::RIGHT, capsAt(fonts.monoTiny, 41.f) + dy, pos,
                  fade(pal.textMuted, a), text::Align::Right);
    }

    const int shown = n < CARDS_VISIBLE ? n : CARDS_VISIBLE;
    const float groupW = float(shown) * CARD_W +
                         float(shown - 1) * (CARD_PITCH - CARD_W);
    const float x0 = float(int((RS_SCREEN_W - groupW) * .5f));
    const float chipY = CARD_Y + CARD_ART_H + 8.f;

    for (int i = 0; i < n; i++) {
        const float x = ui::snap(x0 + (float(i) - m_contScroll.v) * CARD_PITCH);
        if (x + CARD_W <= 0.f || x >= RS_SCREEN_W) continue;
        const db::GameEntry& g = *m_recents[size_t(i)];
        const bool selected = i == m_nav.continueIdx;
        const u32 ca = selected ? a : a * 190u / 255u;
        const float y = CARD_Y + dy;
        if (const gfx::Texture* art = app.boxart().peek(g))
            drawArtCover(r, *art, x, y, CARD_W, CARD_ART_H,
                         rsWithAlpha(rsHex(0xFFFFFF), ca));
        else
            ui::artFallback(app, int(g.system), x, y, CARD_W, CARD_ART_H, ca);
        if (selected) ui::focusFrame(app, x, y, CARD_W, CARD_ART_H, a);

        const float cy = chipY + dy;
        ui::chip(app, x, cy, db::systemInfo(g.system).badge, ca);
        char when[24];
        ui::formatRelative(app.library().lastPlayed(g.pathHash), m_now, when,
                           sizeof when);
        fonts.monoTiny.draw(r, x + CARD_W, fonts.monoTiny.centerY(cy, ui::CHIP_H),
                            when, fade(pal.textMuted, ca), text::Align::Right);
        const text::Font& face = selected ? fonts.bodyStrong : fonts.body;
        drawEllipsized(face, r, x, capsAt(face, cy + ui::CHIP_H + 10.f),
                       CARD_W, g.shown(),
                       fade(selected ? pal.textPrimary : pal.textSecondary, a));
    }
}

void HomeScene::drawLibrary(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int sys = currentSystemId();
    const int count = gamesInCurrent();

    /* The body slides with a system switch and fades as it travels. */
    const float dx = ui::snap(m_slideX.v);
    const u32 body = u32(float(a) * (1.f - rsClamp(std::fabs(dx) / SLIDE,
                                                   0.f, 1.f) * .6f));

    /* Sub-header: badge chip, system name, count; L/R neighbours right. */
    constexpr float HEAD_Y = 36.f;
    const float chipW = ui::chip(app, L::MARGIN + dx, HEAD_Y + dy, badge(sys),
                                 body);
    const float nameX = L::MARGIN + chipW + 8.f + dx;
    fonts.title.draw(r, nameX, fonts.title.centerY(HEAD_Y + dy, ui::CHIP_H),
                     systemTitle(sys), fade(pal.textPrimary, body));
    const float countX = nameX + fonts.title.measure(systemTitle(sys)) + 10.f;
    drawEllipsized(fonts.monoTiny, r, countX,
                   fonts.monoTiny.centerY(HEAD_Y + dy, ui::CHIP_H), 180.f,
                   librarySubtitle(m_view, m_query, count),
                   fade(pal.textMuted, body));

    const int n = int(m_systems.size());
    {
        using ui::prim::Button;
        const float cy = HEAD_Y + ui::CHIP_H * .5f + dy;
        const float ty = fonts.monoTiny.centerY(HEAD_Y + dy, ui::CHIP_H);
        float right = L::RIGHT;
        if (m_nav.systemPos + 1 < n) {
            const float gw = ui::prim::buttonGlyphWidth(Button::R1);
            ui::prim::buttonGlyph(r, Button::R1, right - gw * .5f, cy, 6.f,
                                  fade(pal.textSecondary, a));
            right -= gw + 5.f;
            const char* next = badge(m_systems[size_t(m_nav.systemPos + 1)]);
            fonts.monoTiny.draw(r, right, ty, next, fade(pal.textMuted, a),
                                text::Align::Right);
            right -= fonts.monoTiny.measure(next) + 12.f;
        }
        if (m_nav.systemPos > 0) {
            const char* prev = badge(m_systems[size_t(m_nav.systemPos - 1)]);
            fonts.monoTiny.draw(r, right, ty, prev, fade(pal.textMuted, a),
                                text::Align::Right);
            right -= fonts.monoTiny.measure(prev) + 5.f;
            const float gw = ui::prim::buttonGlyphWidth(Button::L1);
            ui::prim::buttonGlyph(r, Button::L1, right - gw * .5f, cy, 6.f,
                                  fade(pal.textSecondary, a));
        }
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

    /* List: compact rows, one accent fill, a star for favorites. Text-only
     * mode widens the list to the margin and drops the preview entirely. */
    const bool textOnly = !cfg::get().showArt;
    const float listW = textOnly ? L::RIGHT - LIST_X - 8.f : LIST_W;
    const float listH = LIST_ROW * float(LIST_VISIBLE);
    const float scroll = rsClamp(m_scroll.v, 0.f,
                                 float(rsClamp(count - LIST_VISIBLE, 0, count)));
    const ui::GridWindow win =
        ui::visibleGridWindow(count, 1, LIST_VISIBLE, scroll);
    clipContent(r, 0.f, LIST_TOP + dy, LIST_X + listW + 1.f, listH);
    for (int i = win.first; i < win.pastLast; i++) {
        const float y =
            ui::snap(LIST_TOP + (float(i) - scroll) * LIST_ROW) + dy;
        const db::GameEntry& g = *m_visible[size_t(i)];
        const bool selected = i == m_nav.currentGame();
        const bool favorite = app.library().isFavorite(g.pathHash);
        if (selected) ui::focusFill(app, LIST_X + dx, y, listW, LIST_ROW, body);
        const text::Font& face = selected ? fonts.bodyStrong : fonts.body;
        const u32 ink = fade(selected ? pal.onAccent : pal.textPrimary, body);
        drawEllipsized(face, r, LIST_X + 10.f + dx, face.centerY(y, LIST_ROW),
                       listW - 20.f - (favorite ? 16.f : 0.f), g.shown(), ink);
        if (favorite)
            ui::prim::iconStar(r, LIST_X + listW - 12.f + dx, y + 10.f, 5.f,
                               fade(selected ? pal.onAccent
                                             : pal.textSecondary, body));
    }
    restoreContent(r);

    /* Position marker for long libraries: hairline track, short thumb. */
    if (count > LIST_VISIBLE) {
        const float trackX = LIST_X + listW + 5.f;
        r.rect(trackX, LIST_TOP + dy, 1.f, listH, fade(pal.line, a));
        const float thumb = rsClamp(listH * float(LIST_VISIBLE) / float(count),
                                    12.f, listH);
        const float t = rsClamp(float(m_nav.currentGame()) / float(count - 1),
                                0.f, 1.f);
        r.rect(trackX - 1.f, ui::snap(LIST_TOP + dy + (listH - thumb) * t), 3.f,
               ui::snap(thumb), fade(pal.textMuted, a));
    }
    if (textOnly) return;

    /* Preview: one artwork (or the branded fallback), title, at most three
     * chips and one quiet line — only what is known. */
    const db::GameEntry& sel = *m_visible[size_t(m_nav.currentGame())];
    const float px = PREVIEW_X + dx, py = PREVIEW_Y + dy;
    if (const gfx::Texture* art = app.boxart().peek(sel))
        ui::artWell(app, *art, px, py, PREVIEW_W, PREVIEW_H, body);
    else
        ui::artFallback(app, int(sel.system), px, py, PREVIEW_W, PREVIEW_H,
                        body);
    drawEllipsized(fonts.title, r, px, capsAt(fonts.title, py + PREVIEW_H + 12.f),
                   PREVIEW_W, sel.shown(), fade(pal.textPrimary, body));
    char year[12] = "";
    if (m_selMeta.year > 0) std::snprintf(year, sizeof year, "%d", m_selMeta.year);
    std::string genre = upper(m_selMeta.genre);
    const char* chips[3] = {badge(int(sel.system)), genre.c_str(), year};
    chipRow(app, px, py + PREVIEW_H + 30.f, PREVIEW_W, chips, 3, body);
    const std::string maker = !m_selMeta.publisher.empty() ? m_selMeta.publisher
                                                           : m_selMeta.developer;
    drawEllipsized(fonts.small, r, px, capsAt(fonts.small, py + PREVIEW_H + 53.f),
                   PREVIEW_W,
                   maker.empty() ? std::string(systemTitle(int(sel.system)))
                                 : maker,
                   fade(pal.textMuted, body));
}

void HomeScene::drawDetail(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const db::GameEntry& g = m_detailGame;
    const bool favorite = app.library().isFavorite(g.pathHash);

    /* Left: artwork, then the description, quiet. */
    if (const gfx::Texture* art = app.boxart().peek(g))
        ui::artWell(app, *art, DETAIL_ART_X, DETAIL_ART_Y + dy, DETAIL_ART_W,
                    DETAIL_ART_H, a);
    else
        ui::artFallback(app, int(g.system), DETAIL_ART_X, DETAIL_ART_Y + dy,
                        DETAIL_ART_W, DETAIL_ART_H, a);
    drawWrapped(fonts.small, r, DETAIL_ART_X,
                capsAt(fonts.small, DETAIL_ART_Y + DETAIL_ART_H + 12.f) + dy,
                DETAIL_ART_W, 13.f, 6,
                m_selMeta.description.empty()
                    ? std::string("No description available.")
                    : m_selMeta.description,
                fade(pal.textMuted, a));

    /* Right: title (two lines at most), system, chips. */
    const std::string& title = g.shown();
    const int titleLines =
        rsClamp(ui::wrappedLineCount(fonts.display, DETAIL_W, 2, title), 1, 2);
    drawWrapped(fonts.display, r, DETAIL_X,
                capsAt(fonts.display, DETAIL_ART_Y + 2.f) + dy, DETAIL_W, 21.f,
                2, title, fade(pal.textPrimary, a));
    float y = DETAIL_ART_Y + 2.f + 11.f + float(titleLines - 1) * 21.f + 10.f;
    std::string sub = systemTitle(int(g.system));
    const std::string maker = !m_selMeta.publisher.empty() ? m_selMeta.publisher
                                                           : m_selMeta.developer;
    if (!maker.empty()) sub += " \xC2\xB7 " + maker;
    drawEllipsized(fonts.body, r, DETAIL_X, capsAt(fonts.body, y) + dy, DETAIL_W,
                   sub, fade(pal.textSecondary, a));
    y += 8.f + 9.f;
    char year[12] = "";
    if (m_selMeta.year > 0) std::snprintf(year, sizeof year, "%d", m_selMeta.year);
    const std::string genre = upper(m_selMeta.genre);
    const char* chips[3] = {badge(int(g.system)), genre.c_str(), year};
    const float chipsW = chipRow(app, DETAIL_X, y + dy, DETAIL_W - 20.f, chips, 3, a);
    if (favorite)
        ui::prim::iconStar(r, DETAIL_X + chipsW + 8.f, y + ui::CHIP_H * .5f + dy,
                           5.f, fade(pal.textSecondary, a));
    const float actionsY = std::fmax(DETAIL_ACTIONS_Y, y + ui::CHIP_H + 14.f) + dy;

    if (m_detailExpanded) {
        /* Game Details: facts only, every row exists because the data does. */
        char buf[64];
        float sy = actionsY;
        ui::label(app, DETAIL_X, capsAt(fonts.monoTiny, sy) , "GAME DETAILS",
                  fade(pal.textMuted, a));
        sy += 14.f;
        auto stat = [&](const char* key, const char* value) {
            fonts.monoTiny.draw(r, DETAIL_X, fonts.monoTiny.centerY(sy, 14.f),
                                key, fade(pal.textMuted, a));
            drawEllipsized(fonts.small, r, DETAIL_X + 84.f,
                           fonts.small.centerY(sy, 14.f), DETAIL_W - 84.f,
                           value, fade(pal.textPrimary, a));
            sy += 14.f;
        };
        ui::formatRelative(app.library().lastPlayed(g.pathHash), m_now, buf,
                           sizeof buf);
        stat("LAST PLAYED", buf);
        if (const u32 seconds = app.library().playSeconds(g.pathHash)) {
            ui::formatPlaytime(seconds, buf, sizeof buf);
            stat("PLAY TIME", buf);
        }
        std::snprintf(buf, sizeof buf, "%d", app.library().playCount(g.pathHash));
        stat("PLAY COUNT", buf);
        stat("EMULATOR", m_selCore ? m_selCore->name.c_str() : "Automatic");
        if (g.size >= 1024u * 1024u)
            std::snprintf(buf, sizeof buf, "%.1f MB", double(g.size) / 1048576.0);
        else
            std::snprintf(buf, sizeof buf, "%u KB", unsigned(g.size / 1024u));
        stat("ROM SIZE", buf);
        stat("SAVE DATA", m_detailHasSave ? "Present" : "None");
        if (!g.variant.empty()) stat("VERSION", g.variant.c_str());
        /* The cleaned title hides the file's real name; keep it findable. */
        stat("FILE", g.name.c_str());
        return;
    }

    /* Actions: Play is the one dominant control — taller, filled even at
     * rest, with the emulator it will use; the rest are plain rows. */
    {
        const bool focused = m_detailRow == DET_PLAY;
        const float px = DETAIL_X, py = actionsY;
        if (focused) ui::focusFill(app, px, py, DETAIL_ACTION_W, PLAY_H, a);
        else ui::pixelRect(r, px, py, DETAIL_ACTION_W, PLAY_H, 2,
                           fade(pal.surface2, a));
        const u32 ink = fade(focused ? pal.onAccent : pal.textPrimary, a);
        const float cy = py + PLAY_H * .5f;
        r.tri(px + 11.f, cy - 5.f, px + 11.f, cy + 5.f, px + 19.f, cy, ink);
        fonts.bodyStrong.draw(r, px + 27.f, fonts.bodyStrong.centerY(py, PLAY_H),
                              "Play", ink);
        if (m_selCore && m_selMultiCore)
            drawEllipsized(fonts.monoTiny, r, px + DETAIL_ACTION_W - 10.f,
                           fonts.monoTiny.centerY(py, PLAY_H), 110.f,
                           upper(m_selCore->name),
                           fade(focused ? pal.onAccent : pal.textMuted, a),
                           text::Align::Right);
    }
    const char* labels[DET_COUNT] = {
        "Play",
        favorite ? "Remove from Favorites" : "Add to Favorites",
        "Game Details",
        m_nav.detailFrom == nav::Layer::Continue ? "Return" : "Return to Library",
    };
    float ry = actionsY + PLAY_H + 6.f;
    for (int i = DET_FAVORITE; i < DET_COUNT; i++, ry += L::ROW_H) {
        ui::RowStyle style;
        style.focused = i == m_detailRow;
        ui::menuRow(app, DETAIL_X, ry, DETAIL_ACTION_W, L::ROW_H, labels[i],
                    nullptr, style, a);
    }
}

void HomeScene::drawOptions(App& app) {
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    ui::backdrop(app, a);

    const db::GameEntry& game = m_optionsGame;
    const bool favorite = app.library().isFavorite(game.pathHash);
    bool inRecents = false;
    for (u32 h : app.library().recents())
        if (h == game.pathHash) inRecents = true;

    constexpr float GAP = 7.f;     /* rule before Back */
    const PanelBox b = centeredPanel(
        248.f, PANEL_HEAD + L::ROW_H * float(OPT_COUNT) + GAP + 6.f, t);
    ui::panel(app, b.x, b.y, b.w, b.h, a);
    panelHeading(app, b, "OPTIONS", game.shown(), badge(int(game.system)), a);

    const char* labels[OPT_COUNT] = {
        "Play",
        favorite ? "Remove from Favorites" : "Add to Favorites",
        "Game Details",
        m_confirmDelete ? "Press X again to delete" : "Delete Save Data",
        "Remove from Continue", "Back",
    };
    for (int i = 0; i < OPT_COUNT; i++) {
        float y = b.y + PANEL_HEAD + float(i) * L::ROW_H;
        if (i == OPT_BACK) {
            app.renderer().rect(b.x + 10.f, y + 3.f, b.w - 20.f, 1.f,
                                fade(app.pal().line, a));
            y += GAP;
        }
        ui::RowStyle style;
        style.focused = i == m_optionsRow;
        style.disabled = (i == OPT_DELETE_SAVE && !m_optionsHasSave) ||
                         (i == OPT_REMOVE_RECENT && !inRecents);
        ui::menuRow(app, b.x + 3.f, y, b.w - 6.f, L::ROW_H, labels[i], nullptr,
                    style, a);
    }
}

void HomeScene::drawPicker(App& app) {
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    ui::backdrop(app, a);

    constexpr int VISIBLE = 6;
    const int visible = rsClamp(int(m_pickerCores.size()), 1, VISIBLE);
    const int first = rsClamp(m_pickerIdx - 2, 0,
                              int(m_pickerCores.size()) - visible);
    const PanelBox b =
        centeredPanel(288.f, PANEL_HEAD + L::ROW_H * float(visible) + 6.f, t);
    ui::panel(app, b.x, b.y, b.w, b.h, a);
    panelHeading(app, b, "RUN WITH", m_pickerGame.shown(),
                 badge(int(m_pickerGame.system)), a);

    float y = b.y + PANEL_HEAD;
    for (int i = first; i < first + visible; i++, y += L::ROW_H) {
        const CoreInfo& c = *m_pickerCores[size_t(i)];
        char value[32];
        std::snprintf(value, sizeof value, "%s%s",
                      &c == m_pickerCurrent ? "IN USE  " : "",
                      c.isNative() ? "NATIVE"
                                   : (c.psp1000Safe ? "ANY PSP" : "TESTING"));
        ui::RowStyle style;
        style.focused = i == m_pickerIdx;
        ui::menuRow(app, b.x + 3.f, y, b.w - 6.f, L::ROW_H, c.name.c_str(),
                    value, style, a);
    }
}

void HomeScene::drawViewMenu(App& app) {
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    ui::backdrop(app, a);

    int rows[5];
    const int n = viewRows(rows);
    const PanelBox b =
        centeredPanel(280.f, PANEL_HEAD + L::ROW_H * float(n) + 6.f, t);
    ui::panel(app, b.x, b.y, b.w, b.h, a);
    panelHeading(app, b, "VIEW", systemTitle(currentSystemId()),
                 badge(currentSystemId()), a);

    for (int i = 0; i < n; i++) {
        const int row = rows[i];
        const char* label = row == VR_SHOW ? "Show" : row == VR_SORT ? "Sort by"
                          : row == VR_SEARCH ? "Search"
                          : row == VR_CLEAR ? "Clear search" : "Back";
        std::string value;
        if (row == VR_SHOW) value = upper(db::filterName(m_view.filter));
        else if (row == VR_SORT)
            value = upper(m_view.filter == db::ViewFilter::RecentlyAdded
                              ? "Newest first" : db::sortName(m_view.sort));
        else if (row == VR_SEARCH)
            value = m_query.empty() ? std::string("PRESS X")
                                    : "\"" + m_query + "\"";
        ui::RowStyle style;
        style.focused = i == m_viewRow;
        style.adjustable =
            row == VR_SHOW || (row == VR_SORT &&
                               m_view.filter != db::ViewFilter::RecentlyAdded);
        ui::menuRow(app, b.x + 3.f, b.y + PANEL_HEAD + float(i) * L::ROW_H,
                    b.w - 6.f, L::ROW_H, label,
                    value.empty() ? nullptr : value.c_str(), style, a);
    }
}

void HomeScene::drawSearch(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    ui::backdrop(app, a);

    const PanelBox b = centeredPanel(300.f, 140.f, t);
    ui::panel(app, b.x, b.y, b.w, b.h, a);
    char count[40];
    std::snprintf(count, sizeof count, "%d MATCH%s", m_searchCount,
                  m_searchCount == 1 ? "" : "ES");
    panelHeading(app, b, "SEARCH", systemTitle(currentSystemId()), count, a);

    /* Text field: locked letters, then the pending one on the focus fill. */
    const float fx = b.x + 10.f, fy = b.y + PANEL_HEAD + 2.f;
    const float fw = b.w - 20.f, fh = 24.f;
    ui::pixelRect(r, fx, fy, fw, fh, 2, fade(pal.bg, a));
    ui::pixelFrame(r, fx, fy, fw, fh, 1, 2, fade(pal.line, a));
    const float ty = fonts.title.centerY(fy, fh);
    const float lockedW = fonts.title.measure(m_searchBuf.c_str());
    fonts.title.draw(r, fx + 8.f, ty, m_searchBuf.c_str(),
                     fade(pal.textPrimary, a));
    const char pending[2] = {SEARCH_CHARS[m_searchChar] == ' ' ? '_'
                                 : SEARCH_CHARS[m_searchChar], '\0'};
    const float cw = fonts.title.measure("M") + 2.f;
    if (m_searchTouched) {
        ui::focusFill(app, fx + 8.f + lockedW, fy + 4.f, cw, fh - 8.f, a);
        fonts.title.draw(r, fx + 8.f + lockedW + cw * .5f, ty, pending,
                         fade(pal.onAccent, a), text::Align::Center);
    } else if (std::fmod(app.time(), 1.f) < .6f) {
        r.rect(fx + 9.f + lockedW, fy + 5.f, 1.f, fh - 10.f,
               fade(pal.focusEdge, a));
    }

    /* Letter strip: the pending letter centred, neighbours either side, so
     * the D-pad's up/down has somewhere visible to go. */
    const float sy = fy + fh + 10.f;
    for (int d = -3; d <= 3; d++) {
        const int idx = (m_searchChar + d + SEARCH_CHAR_COUNT * 4) %
                        SEARCH_CHAR_COUNT;
        char one[2] = {SEARCH_CHARS[idx] == ' ' ? '_' : SEARCH_CHARS[idx], '\0'};
        const float cx = b.x + b.w * .5f + float(d) * 32.f;
        if (d == 0) {
            ui::focusFill(app, cx - 12.f, sy, 24.f, 24.f, a);
            fonts.title.draw(r, cx, fonts.title.centerY(sy, 24.f), one,
                             fade(pal.onAccent, a), text::Align::Center);
        } else {
            fonts.body.draw(r, cx, fonts.body.centerY(sy, 24.f), one,
                            fade(pal.textMuted,
                                 a * (d == -3 || d == 3 ? 110u : 220u) / 255u),
                            text::Align::Center);
        }
    }
    ui::label(app, b.x + b.w * .5f, capsAt(fonts.monoTiny, b.y + b.h - 15.f),
              "L / R SKIP FIVE  \xC2\xB7  SQUARE CLEARS", fade(pal.textMuted, a),
              text::Align::Center);
}

void HomeScene::drawError(App& app) {
    app.drawBackground();
    app.drawTopBar("GAME");
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
    ui::drawStatePanel(app, panel, a, (1.f - t) * 6.f);

    /* The action sits under the message, where the eye lands; the legend
     * only needs to say how to leave. */
    const App::Hint hints[] = {{ui::prim::Button::Circle, "Back"}};
    app.drawHintBar(hints, 1);
}

void HomeScene::drawLegend(App& app) {
    using B = ui::prim::Button;
    if (m_overlay == Overlay::CorePicker) {
        const App::Hint hints[] = {{B::Cross, "Play"}, {B::Circle, "Cancel"}};
        app.drawHintBar(hints, 2);
        return;
    }
    if (m_overlay == Overlay::Options) {
        const App::Hint hints[] = {{B::Cross, "Select"}, {B::Circle, "Close"}};
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
                    {B::Cross, "Rescan Library"}, {B::Triangle, "Settings"},
                };
                app.drawHintBar(hints, 2);
                break;
            }
            App::Hint hints[3];
            int n = 0;
            if (!m_recents.empty()) hints[n++] = {B::DpadUp, "Continue"};
            hints[n++] = {B::Cross, "Library"};
            hints[n++] = {B::Triangle, "Settings"};
            app.drawHintBar(hints, n);
            break;
        }
        case nav::Layer::Continue: {
            const App::Hint hints[] = {
                {B::Cross, "Resume"}, {B::Square, "Favorite"},
                {B::Triangle, "Options"}, {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 4);
            break;
        }
        case nav::Layer::Library: {
            const App::Hint hints[] = {
                {B::Cross, "Play"}, {B::Square, "Favorite"},
                {B::Triangle, "Options"}, {B::Select, "View"},
                {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 5);
            break;
        }
        case nav::Layer::Detail: {
            if (m_detailExpanded && m_selMultiCore) {
                const App::Hint hints[] = {
                    {B::Triangle, "Change Emulator"}, {B::Circle, "Back"},
                };
                app.drawHintBar(hints, 2);
            } else if (m_detailExpanded) {
                const App::Hint hints[] = {{B::Circle, "Back"}};
                app.drawHintBar(hints, 1);
            } else {
                const App::Hint hints[] = {
                    {B::Cross, "Select"}, {B::Square, "Favorite"},
                    {B::Circle, "Back"},
                };
                app.drawHintBar(hints, 3);
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
    auto offset = [&](float index) { return ui::snap((index - pos) * SHIFT); };

    /* One header for every layer; only its context label changes. */
    const char* context = m_nav.layer == nav::Layer::Library ? "LIBRARY"
                        : m_nav.layer == nav::Layer::Detail  ? "GAME"
                        : nullptr;
    app.drawTopBar(context);

    /* Layers draw inside the content band so travel never crosses the
     * header or the legend. */
    restoreContent(app.renderer());
    drawContinue(app, layerA(-1.f), offset(-1.f));
    drawSystems(app, layerA(0.f), offset(0.f));
    drawLibrary(app, layerA(1.f), offset(1.f));
    drawDetail(app, layerA(2.f), offset(2.f));
    app.renderer().resetScissor();

    drawLegend(app);

    if (m_overlay == Overlay::Options) drawOptions(app);
    else if (m_overlay == Overlay::CorePicker) drawPicker(app);
    else if (m_overlay == Overlay::ViewMenu) drawViewMenu(app);
    else if (m_overlay == Overlay::Search) drawSearch(app);
}

}  // namespace rs
