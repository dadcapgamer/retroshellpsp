#include "frontend/scenes/home_scene.h"
#include "frontend/database/natural_order.h"
#include "frontend/app.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/chrome.h"
#include "frontend/ui/grid_window.h"
#include "frontend/ui/icons.h"
#include "frontend/ui/relative_time.h"
#include "frontend/ui/state_panel.h"
#include "frontend/ui/text_layout.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/osk.h"
#include "platform/psp/power.h"
#include "runtime/config.h"
#include "runtime/log.h"
#include "runtime/save_manager.h"

#include <pspctrl.h>
#include <pspgu.h>

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

/* Systems rail: five slots; the selected one sits in a framed card. */
constexpr int   RAIL_VISIBLE = 5;
constexpr float RAIL_PITCH = 84.f;
constexpr float RAIL_X0 = RS_SCREEN_W * .5f - 2.f * RAIL_PITCH;   /* slot 0 */
constexpr float RAIL_BOX_W = 80.f, RAIL_BOX_H = 98.f, RAIL_BOX_Y = 62.f;
constexpr float RAIL_ICON_CY = 104.f;     /* icon centre line */
constexpr float RAIL_LABEL_Y = 145.f;     /* caps top of the badge */

/* Continue Playing: exactly three cards on screen. */
constexpr int   CARDS_VISIBLE = 3;
constexpr float CARD_W = 136.f, CARD_H = 152.f, CARD_ART_H = 90.f;
constexpr float CARD_PITCH = 156.f;
constexpr float CARD_Y = 78.f;
constexpr int   RECENT_MAX = 6;

/* Library: list with thumbnails left, one preview right. */
constexpr float LIST_X = L::MARGIN, LIST_W = 252.f;
constexpr float LIST_TOP = 58.f, LIST_ROW = 26.f;
constexpr int   LIST_VISIBLE = 7;
constexpr float THUMB = 24.f;
constexpr float PREVIEW_X = 280.f, PREVIEW_Y = 58.f;
constexpr float PREVIEW_W = L::RIGHT - PREVIEW_X, PREVIEW_H = 118.f;

/* Game Detail: framed art and description left, title and actions right. */
constexpr float DETAIL_ART_X = L::MARGIN, DETAIL_ART_Y = 34.f;
constexpr float DETAIL_ART_W = 180.f, DETAIL_ART_H = 140.f;
constexpr float DETAIL_DESC_Y = 182.f, DETAIL_DESC_H = 56.f;
constexpr float DETAIL_X = 208.f;
constexpr float DETAIL_W = L::RIGHT - DETAIL_X;

constexpr float SHIFT = 24.f;             /* vertical layer travel */
constexpr float SLIDE = 36.f;             /* horizontal system-switch travel */
constexpr float SETTLE_SECONDS = 0.18f;

using ui::drawEllipsized;
using ui::drawWrapped;

/* Favorites leads the rail as an entry of its own, browsed exactly like a
 * console. It needs an id the per-system Library state (last selection,
 * view) can key on: outside db::System, inside the 0..31 library.json
 * accepts. */
constexpr int FAVORITES_RAIL = 16;
static_assert(FAVORITES_RAIL >= db::SYSTEM_COUNT, "must not alias a system");

const char* systemTitle(int systemId) {
    if (systemId == FAVORITES_RAIL) return "Favorites";
    return ui::systemName(systemId);
}

const char* badge(int systemId) {
    if (systemId == FAVORITES_RAIL) return "FAV";
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

/* "Favorites · Most played · 12 games": only what differs from the plain
 * A-Z list is mentioned, so the default header stays "42 games". */
std::string librarySubtitle(const db::ViewState& view, const std::string& query,
                            int shown) {
    std::string out;
    auto add = [&](const std::string& part) {
        if (!out.empty()) out += " \xC2\xB7 ";
        out += part;
    };
    if (view.filter != db::ViewFilter::All) add(db::filterName(view.filter));
    if (!query.empty()) add("\"" + query + "\"");
    if (view.sort != db::ViewSort::NameAZ &&
        view.filter != db::ViewFilter::RecentlyAdded)
        add(db::sortName(view.sort));
    char count[24];
    std::snprintf(count, sizeof count, "%d game%s", shown, shown == 1 ? "" : "s");
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
    /* Without a remembered position, start on the first console rather
     * than Favorites, which may still be empty. */
    if (m_systems.size() > 1) m_nav.systemPos = 1;
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

void HomeScene::shutdown(App& app) {
    closeStates();
    app.library().flush();
}

void HomeScene::rebuildSystems(App& app) {
    m_systems.clear();
    bool anyGames = false;
    for (int s = 0; s < db::SYSTEM_COUNT; s++) {
        if (app.index().games(db::System(s)).empty()) continue;
        anyGames = true;
        /* Systems turned off in Settings → Systems stay indexed but hidden. */
        if (cfg::systemEnabled(db::systemInfo(db::System(s)).coreId))
            m_systems.push_back(s);
    }
    /* No games at all is a real state with its own screen (scanning / ROM
     * folder missing / nothing found), not a rail of empty systems. */
    m_allHidden = anyGames && m_systems.empty();
    m_empty = m_systems.empty();
    if (!m_empty) m_systems.insert(m_systems.begin(), FAVORITES_RAIL);
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
std::vector<const db::GameEntry*> HomeScene::railGames(App& app) const {
    std::vector<const db::GameEntry*> out;
    const int system = currentSystemId();
    if (system != FAVORITES_RAIL) {
        const auto& games = app.index().games(db::System(system));
        out.reserve(games.size());
        for (const db::GameEntry& g : games) out.push_back(&g);
        return out;
    }
    /* Favorites from every console still on the rail, merged into one A-Z
     * list in the order GameIndex uses within a system. */
    for (int s : m_systems) {
        if (s == FAVORITES_RAIL) continue;
        for (const db::GameEntry& g : app.index().games(db::System(s)))
            if (app.library().isFavorite(g.pathHash)) out.push_back(&g);
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const db::GameEntry* a, const db::GameEntry* b) {
                         if (db::naturalNameLess(a->title, b->title))
                             return true;
                         if (db::naturalNameLess(b->title, a->title))
                             return false;
                         return db::naturalNameLess(a->name, b->name);
                     });
    return out;
}

void HomeScene::rebuildList(App& app) {
    const int system = currentSystemId();
    const auto all = railGames(app);
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
            if (cfg::systemEnabled(db::systemInfo(game->system).coreId))
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
    /* What a launch will really use: the game's own choice, else the
     * system default from Settings. */
    m_selCore = app.cores().resolve(*g);
    m_selCoreChosen = app.cores().overrideFor(*g) != nullptr;
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
    if (m_view.filter == db::ViewFilter::Favorites ||
        currentSystemId() == FAVORITES_RAIL)
        m_listDirty = true;
    /* In Game Detail the Favorites row already shows the new state; the
     * lists only have a small star, so they get a toast. */
    if (m_overlay == Overlay::None && m_nav.layer != nav::Layer::Detail)
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

void HomeScene::openDetail(App& app, const db::GameEntry& game) {
    m_detailGame = game;
    m_detailRow = 0;
    m_detailScroll = 0.f;
    m_confirmDelete = false;
    m_detailHasSave = save::hasAnySave(game);
    m_detailInRecents = false;
    for (u32 h : app.library().recents())
        if (h == game.pathHash) m_detailInRecents = true;
    closeStates();
    /* Slot headers only (five small reads), for the Save States count. */
    save::querySlots(game, m_stateSlots);
    m_detailStateCount = 0;
    for (const auto& slot : m_stateSlots)
        if (slot.exists) m_detailStateCount++;
    if (!m_nav.openDetail()) return;
    m_trackedHash = game.pathHash;
    m_hydratedHash = 0;
    hydrateSelection(app);       /* explicit action: no settle delay */
}

int HomeScene::detailActions(int out[8]) const {
    int n = 0;
    out[n++] = DA_PLAY;
    out[n++] = DA_STATES;
    out[n++] = DA_FAVORITE;
    if (m_selMultiCore) out[n++] = DA_EMULATOR;
    if (m_detailInRecents) out[n++] = DA_REMOVE_RECENT;
    return n;
}

void HomeScene::activateDetail(App& app, int action) {
    const db::GameEntry game = m_detailGame;
    switch (action) {
        case DA_PLAY:     launch(app, game); break;
        case DA_STATES:   openStates(app); break;
        case DA_FAVORITE: toggleFavorite(app, game); break;
        case DA_EMULATOR: openCorePicker(app, game, /*assign=*/true); break;
        case DA_REMOVE_RECENT:
            if (app.library().removeRecent(game.pathHash)) {
                rebuildRecents(app);
                m_detailInRecents = false;
                app.toast("Removed from Continue Playing");
                int rows[8];
                m_detailRow = rsClamp(m_detailRow, 0, detailActions(rows) - 1);
            }
            break;
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
    else if (pad.isPressed(PSP_CTRL_TRIANGLE)) openDetail(app, *game);
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
    /* X plays; Triangle opens the game's Detail view. */
    if (pad.isPressed(PSP_CTRL_CROSS)) launch(app, *game);
    else if (pad.isPressed(PSP_CTRL_SQUARE)) toggleFavorite(app, *game);
    else if (pad.isPressed(PSP_CTRL_TRIANGLE)) openDetail(app, *game);
}

void HomeScene::updateDetail(App& app) {
    const auto& pad = app.pad();
    if (m_states) {
        updateStates(app, m_dt);
        return;
    }
    int actions[8];
    const int n = detailActions(actions);
    m_detailRow = rsClamp(m_detailRow, 0, n - 1);
    const int prev = m_detailRow;
    if (pad.navPressed(PSP_CTRL_UP) && m_detailRow > 0) m_detailRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_detailRow < n - 1) m_detailRow++;
    (void)prev;
    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        m_nav.back();
        return;
    }
    if (pad.isPressed(PSP_CTRL_START)) {
        m_nav.home();
        return;
    }
    if (pad.isPressed(PSP_CTRL_SQUARE)) toggleFavorite(app, m_detailGame);
    if (pad.isPressed(PSP_CTRL_CROSS)) activateDetail(app, actions[m_detailRow]);
}

/* --- Save States (Game Detail) ------------------------------------------ */

void HomeScene::openStates(App& app) {
    (void)app;
    save::querySlots(m_detailGame, m_stateSlots);   /* explicit action: one read */
    m_states = true;
    m_confirmStateDelete = false;
    m_confirmDelete = false;
    m_stateIdx = 0;
    for (int i = 0; i < save::SLOTS; i++)
        if (m_stateSlots[i].exists) { m_stateIdx = i; break; }
    m_previewSlot = -1;
    m_previewSettle = 0.f;                     /* first preview: immediately */
}

void HomeScene::closeStates() {
    m_states = false;
    gfx::Renderer::freeTexture(m_preview);
    m_previewSlot = -1;
}

/* Reads the selected slot's preview: the full-size frame when the state was
 * saved by this build, else the header's small thumbnail. Runs once per
 * settled selection, never while the cursor is moving. */
void HomeScene::loadStatePreview(App& app) {
    (void)app;
    gfx::Renderer::freeTexture(m_preview);
    m_previewSlot = m_stateIdx;
    m_previewW = m_previewH = 0;
    m_previewLegacy = false;
    if (!m_stateSlots[m_stateIdx].exists) return;
    std::vector<u16> px;
    int w = 0, h = 0;
    if (save::loadPreview(m_detailGame, m_stateIdx, px, w, h)) {
        if (gfx::Renderer::createTexture(m_preview, w, h, GU_PSM_5650, px.data(),
                                         /*dynamic=*/true)) {
            m_previewW = w;
            m_previewH = h;
        }
        return;
    }
    static u16 thumb[save::THUMB_W * save::THUMB_H];
    if (save::loadThumb(m_detailGame, m_stateIdx, thumb) &&
        gfx::Renderer::createTexture(m_preview, save::THUMB_W, save::THUMB_H,
                                     GU_PSM_5650, thumb, /*dynamic=*/true)) {
        m_previewW = save::THUMB_W;
        m_previewH = save::THUMB_H;
        m_previewLegacy = true;
    }
}

void HomeScene::updateStates(App& app, float dt) {
    const auto& pad = app.pad();
    /* Below the slots, and only when there is something to delete, sits
     * Delete All Save Data — kept off Game Detail so it is never one stray
     * press away. */
    const int last = save::SLOTS - 1 + (m_detailHasSave ? 1 : 0);
    const int prev = m_stateIdx;
    m_stateIdx = rsClamp(m_stateIdx, 0, last);
    if (pad.navPressed(PSP_CTRL_UP) && m_stateIdx > 0) m_stateIdx--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_stateIdx < last) m_stateIdx++;
    if (m_stateIdx != prev) {
        m_confirmStateDelete = false;
        m_confirmDelete = false;
        m_previewSettle = SETTLE_SECONDS;
    }
    if (m_stateIdx == save::SLOTS) {
        if (pad.isPressed(PSP_CTRL_CIRCLE)) {
            closeStates();
            return;
        }
        if (pad.isPressed(PSP_CTRL_START)) {
            closeStates();
            m_nav.home();
            return;
        }
        if (pad.isPressed(PSP_CTRL_CROSS)) {
            if (!m_confirmDelete) {
                m_confirmDelete = true;      /* second X deletes */
            } else {
                const int removed = save::deleteAll(m_detailGame);
                m_confirmDelete = false;
                m_detailHasSave = false;
                save::querySlots(m_detailGame, m_stateSlots);
                m_detailStateCount = 0;
                m_stateIdx = 0;
                m_previewSlot = -1;
                m_previewSettle = 0.f;
                char msg[48];
                std::snprintf(msg, sizeof msg, "Deleted %d save file%s",
                              removed, removed == 1 ? "" : "s");
                app.toast(msg);
            }
        }
        return;
    }
    if (m_previewSlot != m_stateIdx) {
        m_previewSettle -= dt;
        if (m_previewSettle <= 0.f) loadStatePreview(app);
    }

    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        closeStates();
        return;
    }
    if (pad.isPressed(PSP_CTRL_START)) {
        closeStates();
        m_nav.home();
        return;
    }
    const save::SlotInfo& slot = m_stateSlots[m_stateIdx];
    if (pad.isPressed(PSP_CTRL_TRIANGLE) && slot.exists) {
        if (!m_confirmStateDelete) {
            m_confirmStateDelete = true;          /* second press deletes */
        } else {
            save::deleteState(m_detailGame, m_stateIdx);
            m_confirmStateDelete = false;
            save::querySlots(m_detailGame, m_stateSlots);
            m_detailStateCount = 0;
            for (const auto& s2 : m_stateSlots)
                if (s2.exists) m_detailStateCount++;
            m_detailHasSave = save::hasAnySave(m_detailGame);
            m_previewSlot = -1;
            m_previewSettle = 0.f;
            app.toast("State deleted");
        }
    }
    if (pad.isPressed(PSP_CTRL_CROSS) && slot.exists) {
        /* Resume with the emulator that wrote the state: states are
         * core-specific, whatever the game's default is today. */
        const CoreInfo* core = app.cores().find(slot.coreName);
        if (!core || !core->serves(m_detailGame.system)) {
            char msg[64];
            std::snprintf(msg, sizeof msg, "%s is not installed",
                          app.cores().displayName(slot.coreName));
            app.toast(msg);
            return;
        }
        /* A native emulator keeps its own states and cannot read RetroShell's.
         * A RetroShell state under a native core's name was written by the
         * retired in-process build of that emulator (FrogGBA's PRX). */
        if (core->isNative()) {
            char msg[96];
            std::snprintf(msg, sizeof msg,
                          "Made by an older %s; it can't be loaded",
                          core->label());
            app.toast(msg);
            RS_LOGW("states: slot %d belongs to native '%s'; refused",
                    m_stateIdx, slot.coreName);
            return;
        }
        app.library().flush();
        app.snapshot().layer = m_nav.layer;
        app.snapshot().systemId = currentSystemId();
        app.snapshot().continueIdx = m_nav.continueIdx;
        app.snapshot().loadStateSlot = m_stateIdx;
        const db::GameEntry game = m_detailGame;
        closeStates();
        app.launchGame(game, core);
        /* GameSession consumes the request; a launch refused before any
         * scene change must not leave it behind for the next game. */
        char raw[96];
        if (app.takeLaunchError(raw, sizeof raw)) {
            app.snapshot().loadStateSlot = -1;
            openError(app, game, raw);
        }
    }
}

void HomeScene::openCorePicker(App& app, const db::GameEntry& game,
                               bool assign) {
    m_pickerCores = app.cores().coresFor(game.system);
    if (m_pickerCores.empty()) {
        app.launchGame(game);   /* reports "no emulator installed" */
        checkLaunchError(app, game);
        return;
    }
    m_pickerGame = game;
    m_pickerAssign = assign;
    m_pickerCurrent = app.cores().resolve(game);
    m_pickerDefault = app.cores().defaultFor(game.system);
    m_pickerChoice = app.cores().overrideFor(game);
    m_overlay = Overlay::CorePicker;
    m_overlayFade.start(0.16f);
    m_pickerIdx = 0;
    const CoreInfo* focus = assign ? m_pickerChoice : m_pickerCurrent;
    for (size_t i = 0; i < m_pickerCores.size(); i++)
        if (m_pickerCores[i] == focus) m_pickerIdx = int(i) + (assign ? 1 : 0);
}

void HomeScene::updatePicker(App& app) {
    const auto& pad = app.pad();
    const int rows = int(m_pickerCores.size()) + (m_pickerAssign ? 1 : 0);
    if (pad.navPressed(PSP_CTRL_UP) && m_pickerIdx > 0) m_pickerIdx--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_pickerIdx < rows - 1)
        m_pickerIdx++;
    if (pad.isPressed(PSP_CTRL_CIRCLE)) m_overlay = Overlay::None;
    if (!pad.isPressed(PSP_CTRL_CROSS)) return;
    m_overlay = Overlay::None;
    if (m_pickerAssign) {
        /* Only this game; the system default stays as set in Settings. */
        const CoreInfo* chosen =
            m_pickerIdx == 0 ? nullptr : m_pickerCores[size_t(m_pickerIdx - 1)];
        CoreRegistry::setOverride(m_pickerGame, chosen);
        char msg[96];
        if (chosen)
            std::snprintf(msg, sizeof msg, "This game uses %s",
                          chosen->label());
        else
            std::snprintf(msg, sizeof msg, "This game uses the default (%s)",
                          m_pickerDefault ? m_pickerDefault->label() : "none");
        app.toast(msg);
        m_hydratedHash = 0;   /* re-read the Emulator row */
        hydrateSelection(app);
        return;
    }
    app.library().flush();
    const db::GameEntry game = m_pickerGame;
    app.launchGame(game, m_pickerCores[size_t(m_pickerIdx)]);
    checkLaunchError(app, game);
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
    /* The PSP's own keyboard, over the Library. Confirm applies the search;
     * cancel goes back to the View menu. */
#ifndef RS_AUTOPILOT   /* scripted input cannot reach the system keyboard */
    m_overlay = Overlay::None;
    struct Ctx { HomeScene* scene; App* app; } ctx{this, &app};
    std::string text;
    const osk::Result result = osk::run(
        app.renderer(), "Search games", m_query, int(SEARCH_MAX), text,
        [](void* p) {
            auto* c = static_cast<Ctx*>(p);
            c->scene->draw(*c->app);
        },
        &ctx);
    /* The button that closed the keyboard must not reach the Library. */
    app.padMutable().resetAfterResume();
    if (result == osk::Result::Accepted) {
        while (!text.empty() && text.back() == ' ') text.pop_back();
        while (!text.empty() && text.front() == ' ') text.erase(0, 1);
        applyView(app, m_view, text);
        return;
    }
    if (result == osk::Result::Cancelled) {
        m_overlay = Overlay::ViewMenu;
        m_overlayFade.start(0.16f);
        return;
    }
#endif
    /* The system keyboard could not start: RetroShell's letter wheel. */
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
    m_searchCount = int(db::buildView(railGames(app), m_view, q,
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
    m_dt = dt;
    /* Emulator Settings "Apply & Restart": straight back into the game.
     * Done here, not in enter(), because a scene switch requested while the
     * previous transition is still finishing would be dropped. */
    if (const u32 hash = app.snapshot().relaunchHash) {
        app.snapshot().relaunchHash = 0;
        const db::GameEntry* game = app.index().byHash(hash);
        const CoreInfo* core = app.cores().find(app.snapshot().relaunchCore);
        if (game && core) {
            const db::GameEntry copy = *game;
            app.launchGame(copy, core);
            char raw[96];
            if (app.takeLaunchError(raw, sizeof raw)) {
                app.snapshot().loadStateSlot = -1;
                app.snapshot().settingsRestart = false;
                openError(app, copy, raw);
            }
            return;
        }
        app.snapshot().loadStateSlot = -1;
        app.snapshot().settingsRestart = false;
    }
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
    if (m_listDirty && m_nav.layer != nav::Layer::Detail &&
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
    /* Library row thumbnails come from the worker; ask for this system's
     * set whenever the Library is (or is about to be) on screen. */
    if (cfg::get().showArt && m_layerPos.v > .3f && !m_systems.empty() &&
        currentSystemId() != FAVORITES_RAIL)
        app.thumbs().requestSystem(currentSystemId());
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

/* Up to three chips in a row (the first is the system's filled chip),
 * stopping before one would overflow. */
float chipRow(App& app, float x, float y, float maxW, const char* const* items,
              int count, u32 alpha) {
    float cx = x;
    for (int i = 0; i < count && i < 3; i++) {
        if (!items[i] || !*items[i]) continue;
        const float w = ui::chipWidth(app, items[i]);
        if (cx + w > x + maxW) break;
        ui::chip(app, cx, y, items[i], alpha, /*system=*/i == 0);
        cx += w + 5.f;
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
    ui::label(app, b.x + 10.f, capsAt(fonts.tiny, b.y + 9.f), overline,
              fade(pal.textMuted, a));
    if (right)
        ui::label(app, b.x + b.w - 10.f, capsAt(fonts.tiny, b.y + 9.f),
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
        } else if (m_allHidden) {
            panel.kind = ui::StatePanel::Kind::Empty;
            panel.title = "All systems are turned off";
            panel.message = "Turn a system back on in Settings, under Systems.";
            panel.actionLabel = "Settings";
            panel.actionButton = ui::prim::Button::Triangle;
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

    /* The selected system sits in a framed card that glides with the
     * selection: a deep accent wash inside the focus frame. */
    const float boxX = selX - RAIL_BOX_W * .5f, boxY = RAIL_BOX_Y + dy;
    ui::pixelRect(r, boxX, boxY, RAIL_BOX_W, RAIL_BOX_H, 3,
                  fade(rsWithAlpha(pal.accent, 46), a));
    ui::pixelFrame(r, boxX, boxY, RAIL_BOX_W, RAIL_BOX_H, 2, 3,
                   fade(pal.focusEdge, a));

    clipContent(r, L::MARGIN + 22.f, 0.f,
                RS_SCREEN_W - 2.f * (L::MARGIN + 22.f), RS_SCREEN_H);
    for (int i = 0; i < n; i++) {
        const float x = ui::snap(slotX(float(i)));
        if (x < 0.f || x > RS_SCREEN_W) continue;
        const int system = m_systems[size_t(i)];
        const bool active = std::fabs(float(i) - m_railPos.v) < .5f;
        const float size = active ? 64.f : 48.f;
        const u32 iconInk = rsWithAlpha(rsHex(0xFFFFFF),
                                        active ? a : a * 200u / 255u);
        /* The star is monochrome, so it takes the theme's text ink (the
         * same as its label) instead of the consoles' full-colour art. */
        if (system == FAVORITES_RAIL)
            ui::prim::iconFavorite(
                r, x - size * .5f, RAIL_ICON_CY - size * .5f + dy, size,
                fade(active ? pal.textPrimary : pal.textSecondary, a));
        else
            ui::prim::iconSystem(r, system, x - size * .5f,
                                 RAIL_ICON_CY - size * .5f + dy, size,
                                 iconInk, pal.accent);
        const text::Font& face = active ? fonts.bodyStrong : fonts.body;
        face.draw(r, x, capsAt(face, RAIL_LABEL_Y) + dy, badge(system),
                  fade(active ? pal.textPrimary : pal.textSecondary, a),
                  text::Align::Center);
    }
    restoreContent(r);

    /* L1 / R1 at the rail's ends: the shoulders change system everywhere
     * (Home, Library), so the hint sits where the systems are. Dimmed at
     * either end of the rail. */
    const u32 leftA = m_nav.systemPos > 0 ? a : a * 50u / 255u;
    const u32 rightA = m_nav.systemPos + 1 < n ? a : a * 50u / 255u;
    {
        using ui::prim::Button;
        const float lw = ui::prim::buttonGlyphWidth(Button::L1);
        const float rw = ui::prim::buttonGlyphWidth(Button::R1);
        ui::prim::buttonGlyph(r, Button::L1, L::MARGIN + lw * .5f,
                              RAIL_ICON_CY + dy, 6.f,
                              fade(pal.textSecondary, leftA));
        ui::prim::buttonGlyph(r, Button::R1, L::RIGHT - rw * .5f,
                              RAIL_ICON_CY + dy, 6.f,
                              fade(pal.textSecondary, rightA));
    }

    /* Name and count crossfade in from the direction of travel. */
    const float tf = ui::easeOutCubic(m_titleFade.t);
    const u32 ta = u32(float(a) * tf);
    const float tdx = ui::snap((1.f - tf) * 6.f * float(m_titleDir));
    const int sys = currentSystemId();
    fonts.display.draw(r, RS_SCREEN_W * .5f + tdx,
                       capsAt(fonts.display, 186.f) + dy,
                       upper(systemTitle(sys)).c_str(),
                       fade(pal.textPrimary, ta), text::Align::Center, 4.f);
    char count[32];
    std::snprintf(count, sizeof count, "%d GAME%s", m_systemTotal,
                  m_systemTotal == 1 ? "" : "S");
    fonts.small.draw(r, RS_SCREEN_W * .5f + tdx, capsAt(fonts.small, 209.f) + dy,
                     count, fade(pal.textSecondary, ta), text::Align::Center,
                     2.f);
}

void HomeScene::drawContinue(App& app, u32 a, float dy) {
    if (a <= 2u || m_recents.empty()) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int n = int(m_recents.size());

    fonts.title.draw(r, L::MARGIN, capsAt(fonts.title, 41.f) + dy,
                     "CONTINUE PLAYING", fade(pal.textPrimary, a),
                     text::Align::Left, 1.f);
    fonts.small.draw(r, L::MARGIN, capsAt(fonts.small, 60.f) + dy,
                     "Recent games across all systems.",
                     fade(pal.textSecondary, a));
    if (n > 1) {
        /* Position, flanked by the shoulders that also move through it. */
        using ui::prim::Button;
        char pos[32];
        std::snprintf(pos, sizeof pos, "%d / %d", m_nav.continueIdx + 1, n);
        const float cy = 46.f + dy;
        const float rw = ui::prim::buttonGlyphWidth(Button::R1);
        const float lw = ui::prim::buttonGlyphWidth(Button::L1);
        float x = L::RIGHT - rw;
        ui::prim::buttonGlyph(r, Button::R1, x + rw * .5f, cy, 6.f,
                              fade(pal.textSecondary, a));
        x -= 8.f;
        fonts.small.draw(r, x, fonts.small.centerY(cy - 5.f, 10.f), pos,
                         fade(pal.textMuted, a), text::Align::Right);
        x -= fonts.small.measure(pos) + 8.f + lw;
        ui::prim::buttonGlyph(r, Button::L1, x + lw * .5f, cy, 6.f,
                              fade(pal.textSecondary, a));
    }

    const int shown = n < CARDS_VISIBLE ? n : CARDS_VISIBLE;
    const float groupW = float(shown) * CARD_W +
                         float(shown - 1) * (CARD_PITCH - CARD_W);
    const float x0 = float(int((RS_SCREEN_W - groupW) * .5f));

    for (int i = 0; i < n; i++) {
        const float x = ui::snap(x0 + (float(i) - m_contScroll.v) * CARD_PITCH);
        if (x + CARD_W <= 0.f || x >= RS_SCREEN_W) continue;
        const db::GameEntry& g = *m_recents[size_t(i)];
        const bool selected = i == m_nav.continueIdx;
        const float y = CARD_Y + dy;
        /* Card: art across the top, then the system chip, title and when. */
        ui::card(app, x, y, CARD_W, CARD_H, a);
        const float ax = x + 1.f, ay = y + 1.f, aw = CARD_W - 2.f;
        if (const gfx::Texture* art = app.boxart().peek(g))
            drawArtCover(r, *art, ax, ay, aw, CARD_ART_H,
                         rsWithAlpha(rsHex(0xFFFFFF), a));
        else
            ui::artFallback(app, int(g.system), ax, ay, aw, CARD_ART_H, a);
        r.rect(ax, ay + CARD_ART_H, aw, 1.f, fade(pal.line, a));

        const float tx = x + 9.f;
        const float cy = ay + CARD_ART_H + 9.f;
        ui::chip(app, tx, cy, db::systemInfo(g.system).badge, a, true);
        drawEllipsized(fonts.bodyStrong, r, tx,
                       capsAt(fonts.bodyStrong, cy + ui::CHIP_H + 9.f),
                       CARD_W - 18.f, g.shown(), fade(pal.textPrimary, a));
        char when[24], line[48];
        ui::formatRelative(app.library().lastPlayed(g.pathHash), m_now, when,
                           sizeof when);
        std::snprintf(line, sizeof line, "Last played %s", when);
        drawEllipsized(fonts.small, r, tx,
                       capsAt(fonts.small, cy + ui::CHIP_H + 26.f),
                       CARD_W - 18.f, line, fade(pal.textSecondary, a));
        if (selected) ui::focusFrame(app, x, y, CARD_W, CARD_H, a);
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

    /* Sub-header: system chip, "<System> Library", count on the right. */
    constexpr float HEAD_Y = 35.f;
    const float chipW = ui::chip(app, L::MARGIN + dx, HEAD_Y + dy, badge(sys),
                                 body, true);
    const std::string heading = sys == FAVORITES_RAIL
        ? std::string(systemTitle(sys))
        : std::string(systemTitle(sys)) + " Library";
    const float headX = L::MARGIN + chipW + 10.f + dx;
    fonts.body.draw(r, headX, fonts.body.centerY(HEAD_Y + dy, ui::CHIP_H),
                    heading.c_str(), fade(pal.textPrimary, body));
    const float countX = headX + fonts.body.measure(heading.c_str()) + 10.f;

    /* Right: the shoulders and the systems they lead to. */
    float right = L::RIGHT;
    {
        using ui::prim::Button;
        const int sysCount = int(m_systems.size());
        const float cy = HEAD_Y + ui::CHIP_H * .5f + dy;
        const float ty = fonts.small.centerY(HEAD_Y + dy, ui::CHIP_H);
        if (m_nav.systemPos + 1 < sysCount) {
            const float gw = ui::prim::buttonGlyphWidth(Button::R1);
            ui::prim::buttonGlyph(r, Button::R1, right - gw * .5f, cy, 6.f,
                                  fade(pal.textSecondary, a));
            right -= gw + 6.f;
            const char* next = badge(m_systems[size_t(m_nav.systemPos + 1)]);
            fonts.small.draw(r, right, ty, next, fade(pal.textMuted, a),
                             text::Align::Right);
            right -= fonts.small.measure(next) + 14.f;
        }
        if (m_nav.systemPos > 0) {
            const char* prev = badge(m_systems[size_t(m_nav.systemPos - 1)]);
            fonts.small.draw(r, right, ty, prev, fade(pal.textMuted, a),
                             text::Align::Right);
            right -= fonts.small.measure(prev) + 6.f;
            const float gw = ui::prim::buttonGlyphWidth(Button::L1);
            ui::prim::buttonGlyph(r, Button::L1, right - gw * .5f, cy, 6.f,
                                  fade(pal.textSecondary, a));
            right -= gw;
        }
    }
    drawEllipsized(fonts.small, r, countX,
                   fonts.small.centerY(HEAD_Y + dy, ui::CHIP_H),
                   right - 12.f - countX, librarySubtitle(m_view, m_query, count),
                   fade(pal.textMuted, body));

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
        } else if (m_view.filter == db::ViewFilter::Favorites ||
                   sys == FAVORITES_RAIL) {
            panel.title = "No favorites yet";
            message = "Press the square button on any game to add it here.";
            if (m_view.isDefault()) panel.actionLabel = nullptr;
        } else {
            panel.title = "No games to show";
            message = "Rescan the library from Settings.";
            panel.actionLabel = nullptr;
        }
        panel.message = message;
        ui::drawStatePanel(app, panel, body, dy + 14.f);
        return;
    }

    /* List: a thumbnail (or the system glyph) per row, hairlines between,
     * one accent fill with a chevron. Text-only mode drops thumbnails and the
     * preview, and the list takes the full width. */
    const bool textOnly = !cfg::get().showArt;
    const float listW = textOnly ? L::RIGHT - LIST_X - 8.f : LIST_W;
    const float listH = LIST_ROW * float(LIST_VISIBLE);
    const float scroll = rsClamp(m_scroll.v, 0.f,
                                 float(rsClamp(count - LIST_VISIBLE, 0, count)));
    const ui::GridWindow win =
        ui::visibleGridWindow(count, 1, LIST_VISIBLE, scroll);
    const int selRow = m_nav.currentGame();
    clipContent(r, 0.f, LIST_TOP + dy, LIST_X + listW + 1.f, listH);
    for (int i = win.first; i < win.pastLast; i++) {
        const float y =
            ui::snap(LIST_TOP + (float(i) - scroll) * LIST_ROW) + dy;
        const db::GameEntry& g = *m_visible[size_t(i)];
        const bool selected = i == selRow;
        const bool favorite = app.library().isFavorite(g.pathHash);
        const float x = LIST_X + dx;
        if (selected) ui::focusFill(app, x, y, listW, LIST_ROW, body);
        else if (i + 1 != selRow && i + 1 < count)
            ui::rowRule(app, x, y + LIST_ROW - 1.f, listW, body);
        float tx = x + 10.f;
        if (!textOnly) {
            const float ty = y + (LIST_ROW - THUMB) * .5f;
            if (const gfx::Texture* t = app.thumbs().get(g.pathHash))
                r.sprite(*t, 0.f, 0.f, THUMB, THUMB, x + 6.f, ty, THUMB, THUMB,
                         rsWithAlpha(rsHex(0xFFFFFF), body));
            else
                ui::prim::iconSystem(r, int(g.system), x + 6.f, ty, THUMB,
                                     rsWithAlpha(rsHex(0xFFFFFF), body),
                                     pal.accent);
            tx = x + 6.f + THUMB + 12.f;
        }
        const text::Font& face = selected ? fonts.bodyStrong : fonts.body;
        const u32 ink = fade(selected ? pal.onAccent : pal.textPrimary, body);
        float right = x + listW - 10.f;
        if (selected) {
            ui::prim::chevron(r, ui::prim::Dir::Right, right - 2.f,
                              y + LIST_ROW * .5f, 4.f, 1.5f, ink);
            right -= 14.f;
        }
        if (favorite) {
            ui::icon(r, ui::Icon::Star, right - ui::ICON_SIZE,
                     y + (LIST_ROW - ui::ICON_SIZE) * .5f,
                     fade(selected ? pal.onAccent : pal.textSecondary, body));
            right -= ui::ICON_SIZE + 8.f;
        }
        drawEllipsized(face, r, tx, face.centerY(y, LIST_ROW), right - tx,
                       g.shown(), ink);
    }
    restoreContent(r);

    /* Position marker for long libraries: hairline track, short thumb. */
    if (count > LIST_VISIBLE) {
        const float trackX = LIST_X + listW + 4.f;
        r.rect(trackX, LIST_TOP + dy, 1.f, listH, fade(pal.line, a));
        const float thumb = rsClamp(listH * float(LIST_VISIBLE) / float(count),
                                    12.f, listH);
        const float t = rsClamp(float(selRow) / float(count - 1), 0.f, 1.f);
        r.rect(trackX - 1.f, ui::snap(LIST_TOP + dy + (listH - thumb) * t), 3.f,
               ui::snap(thumb), fade(pal.textMuted, a));
    }
    if (textOnly) return;

    /* Preview: framed artwork (or the branded fallback), title, at most
     * three chips and one meta line of what is known. */
    const db::GameEntry& sel = *m_visible[size_t(selRow)];
    const float px = PREVIEW_X + dx, py = PREVIEW_Y + dy;
    ui::card(app, px, py, PREVIEW_W, PREVIEW_H, body);
    if (const gfx::Texture* art = app.boxart().peek(sel))
        ui::artWell(app, *art, px + 1.f, py + 1.f, PREVIEW_W - 2.f,
                    PREVIEW_H - 2.f, body);
    else
        ui::artFallback(app, int(sel.system), px + 1.f, py + 1.f,
                        PREVIEW_W - 2.f, PREVIEW_H - 2.f, body);
    drawEllipsized(fonts.title, r, px, capsAt(fonts.title, py + PREVIEW_H + 9.f),
                   PREVIEW_W, sel.shown(), fade(pal.textPrimary, body));
    char year[12] = "";
    if (m_selMeta.year > 0) std::snprintf(year, sizeof year, "%d", m_selMeta.year);
    const char* chips[3] = {badge(int(sel.system)), m_selMeta.genre.c_str(), year};
    chipRow(app, px, py + PREVIEW_H + 25.f, PREVIEW_W, chips, 3, body);
    drawMetaLine(app, sel, px, py + PREVIEW_H + 48.f, PREVIEW_W, body);
}

/* One quiet line under a title: ROM size, then the maker when known. */
void HomeScene::drawMetaLine(App& app, const db::GameEntry& g, float x,
                             float capsTop, float maxW, u32 a) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    char size[24];
    if (g.size >= 1024u * 1024u)
        std::snprintf(size, sizeof size, "%.0f MB", double(g.size) / 1048576.0);
    else
        std::snprintf(size, sizeof size, "%u KB", unsigned(g.size / 1024u));
    const u32 ink = fade(pal.textSecondary, a);
    const float iy = capsTop + 3.5f - ui::ICON_SIZE * .5f;
    ui::icon(r, ui::Icon::Card, x, iy, ink);
    float tx = x + ui::ICON_SIZE + 6.f;
    fonts.small.draw(r, tx, capsAt(fonts.small, capsTop), size, ink);
    tx += fonts.small.measure(size) + 14.f;
    const std::string maker = !m_selMeta.publisher.empty() ? m_selMeta.publisher
                                                           : m_selMeta.developer;
    if (!maker.empty() && tx + ui::ICON_SIZE + 6.f < x + maxW) {
        ui::icon(r, ui::Icon::Player, tx, iy, ink);
        tx += ui::ICON_SIZE + 6.f;
        drawEllipsized(fonts.small, r, tx, capsAt(fonts.small, capsTop),
                       x + maxW - tx, maker, ink);
    }
}

void HomeScene::drawDetail(App& app, u32 a, float dy) {
    if (a <= 2u) return;
    if (m_states) {
        drawStates(app, a, dy);
        return;
    }
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const db::GameEntry& g = m_detailGame;
    const bool favorite = app.library().isFavorite(g.pathHash);

    /* Left: framed artwork, and the description beneath it only when one
     * exists — without one the artwork takes the whole column, so nothing
     * on screen is a placeholder. */
    const bool hasDesc = !m_selMeta.description.empty();
    const float artH = hasDesc ? DETAIL_ART_H
                               : L::CONTENT_BOTTOM - DETAIL_ART_Y;
    ui::card(app, DETAIL_ART_X, DETAIL_ART_Y + dy, DETAIL_ART_W, artH, a);
    if (const gfx::Texture* art = app.boxart().peek(g))
        ui::artWell(app, *art, DETAIL_ART_X + 1.f, DETAIL_ART_Y + 1.f + dy,
                    DETAIL_ART_W - 2.f, artH - 2.f, a);
    else
        ui::artFallback(app, int(g.system), DETAIL_ART_X + 1.f,
                        DETAIL_ART_Y + 1.f + dy, DETAIL_ART_W - 2.f, artH - 2.f,
                        a);
    if (hasDesc) {
        ui::card(app, DETAIL_ART_X, DETAIL_DESC_Y + dy, DETAIL_ART_W,
                 DETAIL_DESC_H, a);
        const std::string& desc = m_selMeta.description;
        const int descLines =
            ui::wrappedLineCount(fonts.small, DETAIL_ART_W - 20.f, 3, desc);
        const float descTop = DETAIL_DESC_Y +
            (DETAIL_DESC_H - float(descLines) * 14.f + 7.f) * .5f;
        drawWrapped(fonts.small, r, DETAIL_ART_X + 10.f,
                    capsAt(fonts.small, ui::snap(descTop)) + dy,
                    DETAIL_ART_W - 20.f, 14.f, 3, desc,
                    fade(pal.textSecondary, a));
    }

    /* Right: title (two lines at most), system, chips. */
    const std::string& title = g.shown();
    const int titleLines =
        rsClamp(ui::wrappedLineCount(fonts.display, DETAIL_W, 2, title), 1, 2);
    drawWrapped(fonts.display, r, DETAIL_X, capsAt(fonts.display, 38.f) + dy,
                DETAIL_W, 20.f, 2, title, fade(pal.textPrimary, a));
    float y = 38.f + 11.f + float(titleLines - 1) * 20.f + 9.f;
    drawEllipsized(fonts.body, r, DETAIL_X, capsAt(fonts.body, y) + dy, DETAIL_W,
                   systemTitle(int(g.system)), fade(pal.textSecondary, a));
    y += 8.f + 9.f;
    char year[12] = "";
    if (m_selMeta.year > 0) std::snprintf(year, sizeof year, "%d", m_selMeta.year);
    const char* chips[3] = {badge(int(g.system)), m_selMeta.genre.c_str(), year};
    const float chipsW = chipRow(app, DETAIL_X, y + dy, DETAIL_W - 20.f, chips, 3, a);
    if (favorite)
        ui::icon(r, ui::Icon::Star, DETAIL_X + chipsW + 4.f,
                 y + (ui::CHIP_H - ui::ICON_SIZE) * .5f + dy,
                 fade(pal.textSecondary, a));
    y += ui::CHIP_H + 10.f;

    /* One quiet line of facts: when it was last played, how long, which
     * dump, how big — only what is known. */
    {
        std::string facts;
        auto add = [&](const std::string& part) {
            if (part.empty()) return;
            if (!facts.empty()) facts += " \xC2\xB7 ";
            facts += part;
        };
        char buf[40];
        if (const u64 last = app.library().lastPlayed(g.pathHash)) {
            char when[24];
            ui::formatRelative(last, m_now, when, sizeof when);
            std::snprintf(buf, sizeof buf, "Played %s", when);
            add(buf);
        }
        if (const u32 seconds = app.library().playSeconds(g.pathHash)) {
            ui::formatPlaytime(seconds, buf, sizeof buf);
            add(std::string(buf) + " total");
        }
        add(g.variant);
        if (g.size >= 1024u * 1024u)
            std::snprintf(buf, sizeof buf, "%.1f MB", double(g.size) / 1048576.0);
        else
            std::snprintf(buf, sizeof buf, "%u KB", unsigned(g.size / 1024u));
        add(buf);
        drawEllipsized(fonts.small, r, DETAIL_X, capsAt(fonts.small, y) + dy,
                       DETAIL_W, facts, fade(pal.textMuted, a));
        y += 7.f + 9.f;
    }

    /* Actions: everything you can do with this game, icon rows with
     * hairlines between and one accent fill. The list scrolls only when a
     * two-line title leaves less room than it needs. */
    int actions[8];
    const int n = detailActions(actions);
    const float actionsY = ui::snap(y) + dy;
    const int visible = rsClamp(int((L::CONTENT_BOTTOM - y) / L::ROW_H), 1, n);
    const int first = rsClamp(m_detailRow - visible + 1, 0, n - visible);
    char stateCount[16];
    std::snprintf(stateCount, sizeof stateCount, "%d / %d", m_detailStateCount,
                  save::SLOTS);
    for (int vi = 0; vi < visible; vi++) {
        const int i = first + vi;
        const int act = actions[i];
        const float ry = actionsY + float(vi) * L::ROW_H;
        ui::RowStyle style;
        style.focused = i == m_detailRow;
        style.chevron = act != DA_REMOVE_RECENT && act != DA_FAVORITE;
        style.strong = act == DA_PLAY;
        const char* label = "";
        std::string value;
        switch (act) {
            case DA_PLAY:
                label = "Play";
                style.icon = int(ui::Icon::Play);
                break;
            case DA_STATES:
                label = "Save States";
                style.icon = int(ui::Icon::Stack);
                value = stateCount;
                break;
            case DA_FAVORITE:
                label = favorite ? "Remove from Favorites" : "Add to Favorites";
                style.icon = int(ui::Icon::Star);
                break;
            case DA_EMULATOR:
                label = "Emulator";
                style.icon = int(ui::Icon::Gamepad);
                if (m_selCore)
                    value = m_selCoreChosen
                        ? std::string(m_selCore->label())
                        : std::string(m_selCore->label()) + " \xC2\xB7 Default";
                break;
            case DA_REMOVE_RECENT:
                label = "Remove from Continue";
                style.icon = int(ui::Icon::Close);
                break;
        }
        ui::menuRow(app, DETAIL_X, ry, DETAIL_W, L::ROW_H, label,
                    value.empty() ? nullptr : value.c_str(), style, a);
        if (!style.focused && vi + 1 < visible && i + 1 != m_detailRow)
            ui::rowRule(app, DETAIL_X, ry + L::ROW_H - 1.f, DETAIL_W, a);
    }
    if (n > visible) {
        const float trackH = L::ROW_H * float(visible);
        const float thumb = trackH * float(visible) / float(n);
        const float t = float(first) / float(n - visible);
        r.rect(L::RIGHT - 1.f, ui::snap(actionsY + (trackH - thumb) * t), 2.f,
               ui::snap(thumb), fade(pal.textMuted, a));
    }
}

/* Save States: the selected slot's frame large on the left — 1:1 whenever
 * it fits, so a GBA or Game Boy frame is pixel-exact — and the five slots on
 * the right with when and with which emulator each was saved. */
void HomeScene::drawStates(App& app, u32 a, float dy) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    constexpr float PX = L::MARGIN, PY = 34.f, PW = 272.f, PH = 204.f;
    constexpr float LX = PX + PW + 12.f, LW = L::RIGHT - LX;
    constexpr float SLOT_H = 28.f;
    const bool onDeleteAll = m_stateIdx >= save::SLOTS;

    ui::card(app, PX, PY + dy, PW, PH, a);
    const float ix = PX + 1.f, iy = PY + 1.f + dy, iw = PW - 2.f, ih = PH - 2.f;
    const save::SlotInfo& sel = m_stateSlots[onDeleteAll ? 0 : m_stateIdx];
    if (onDeleteAll) {
        /* Say plainly what goes, where the preview would be. */
        r.rect(ix, iy, iw, ih, fade(pal.surface2, a));
        const float cx = ix + iw * .5f;
        ui::pixelFrame(r, cx - 12.f, iy + 46.f, 25.f, 25.f, 2, 3,
                       fade(pal.danger, a));
        r.rect(cx - 1.f, iy + 52.f, 3.f, 8.f, fade(pal.danger, a));
        r.rect(cx - 1.f, iy + 62.f, 3.f, 3.f, fade(pal.danger, a));
        fonts.title.draw(r, cx, fonts.title.centerY(iy + 86.f, 10.f),
                         "Delete all save data", fade(pal.textPrimary, a),
                         text::Align::Center);
        drawWrapped(fonts.small, r, cx, fonts.small.centerY(iy + 108.f, 7.f),
                    iw - 40.f, 14.f, 3,
                    "Removes this game's battery save (your in-game "
                    "progress) and every save state. This cannot be undone.",
                    fade(pal.textSecondary, a), text::Align::Center);
    } else if (m_preview.valid() && m_previewSlot == m_stateIdx && m_previewW > 0) {
        r.rect(ix, iy, iw, ih, fade(rsHex(0x000000), a));
        /* Largest whole-number scale that fits keeps pixels square and
         * sharp; a frame larger than the well scales down smoothly. The old
         * small thumbnails are enlarged in whole steps, nearest. */
        float scale = std::floor(std::fmin(iw / float(m_previewW),
                                           ih / float(m_previewH)));
        const bool exact = scale >= 1.f;
        if (!exact)
            scale = std::fmin(iw / float(m_previewW), ih / float(m_previewH));
        const float w = ui::snap(float(m_previewW) * scale);
        const float h = ui::snap(float(m_previewH) * scale);
        const gfx::TexFilter previous = r.texFilter();
        r.setTexFilter(exact ? gfx::TexFilter::Nearest : gfx::TexFilter::Linear);
        r.sprite(m_preview, 0.f, 0.f, float(m_previewW), float(m_previewH),
                 ix + ui::snap((iw - w) * .5f), iy + ui::snap((ih - h) * .5f), w,
                 h, rsWithAlpha(rsHex(0xFFFFFF), a));
        r.setTexFilter(previous);
        if (m_previewLegacy)
            fonts.tiny.draw(r, ix + iw - 8.f, iy + ih - 14.f,
                            "Small preview: saved by an older version",
                            fade(pal.textMuted, a), text::Align::Right);
    } else {
        ui::artFallback(app, int(m_detailGame.system), ix, iy, iw, ih, a);
        if (!sel.exists)
            fonts.small.draw(r, ix + iw * .5f, iy + ih - 20.f, "Empty slot",
                             fade(pal.textSecondary, a), text::Align::Center);
    }

    fonts.title.draw(r, LX, capsAt(fonts.title, 38.f) + dy, "SAVE STATES",
                     fade(pal.textPrimary, a), text::Align::Left, 1.f);
    drawEllipsized(fonts.small, r, LX, capsAt(fonts.small, 56.f) + dy, LW,
                   m_detailGame.shown(), fade(pal.textSecondary, a));
    const float top = 68.f + dy;
    for (int i = 0; i < save::SLOTS; i++) {
        const save::SlotInfo& slot = m_stateSlots[i];
        const float y = top + float(i) * SLOT_H;
        const bool focused = i == m_stateIdx;
        if (focused) ui::focusFill(app, LX, y, LW, SLOT_H, a);
        else if (i + 1 < save::SLOTS && i + 1 != m_stateIdx)
            ui::rowRule(app, LX, y + SLOT_H - 1.f, LW, a);
        const bool confirming = focused && m_confirmStateDelete;
        const u32 ink = focused ? pal.onAccent : pal.textPrimary;
        const u32 sub = focused ? rsWithAlpha(pal.onAccent, 200)
                                : pal.textSecondary;
        char name[16];
        std::snprintf(name, sizeof name, "Slot %d", i + 1);
        const text::Font& face = focused ? fonts.bodyStrong : fonts.body;
        face.draw(r, LX + 10.f, capsAt(face, y + 6.f), name,
                  fade(slot.exists ? ink : (focused ? ink : pal.textMuted), a));
        if (slot.exists) {
            char when[24];
            if (slot.stamp) ui::formatRelative(slot.stamp, m_now, when, sizeof when);
            else std::snprintf(when, sizeof when, "Saved");
            fonts.small.draw(r, LX + LW - 10.f, capsAt(fonts.small, y + 7.f), when,
                             fade(sub, a), text::Align::Right);
            drawEllipsized(fonts.small, r, LX + 10.f, capsAt(fonts.small, y + 18.f),
                           LW - 20.f,
                           confirming ? std::string("Press triangle again to delete")
                                      : std::string(app.cores().displayName(
                                            slot.coreName)),
                           fade(sub, a));
        } else {
            fonts.small.draw(r, LX + 10.f, capsAt(fonts.small, y + 18.f), "Empty",
                             fade(focused ? sub : pal.textMuted, a));
        }
    }
    if (m_detailHasSave) {
        /* Last, set apart and in the danger colour. */
        const float y = top + SLOT_H * float(save::SLOTS) + 6.f;
        const bool focused = onDeleteAll;
        if (focused) ui::focusFill(app, LX, y, LW, L::ROW_H, a);
        else r.rect(LX + 6.f, y - 3.f, LW - 12.f, 1.f, fade(pal.line, a));
        const text::Font& face = focused ? fonts.bodyStrong : fonts.body;
        face.draw(r, LX + 10.f, face.centerY(y, L::ROW_H),
                  focused && m_confirmDelete ? "Press X again" : "Delete All Save Data",
                  fade(focused ? pal.onAccent : pal.danger, a));
    }
}

void HomeScene::drawPicker(App& app) {
    const float t = ui::easeOutCubic(m_overlayFade.t);
    const u32 a = u32(t * 255.f);
    ui::backdrop(app, a);

    constexpr int VISIBLE = 6;
    const int offset = m_pickerAssign ? 1 : 0;
    const int rows = int(m_pickerCores.size()) + offset;
    const int visible = rsClamp(rows, 1, VISIBLE);
    const int first = rsClamp(m_pickerIdx - 2, 0, rows - visible);
    const PanelBox b =
        centeredPanel(288.f, PANEL_HEAD + L::ROW_H * float(visible) + 6.f, t);
    ui::panel(app, b.x, b.y, b.w, b.h, a);
    panelHeading(app, b, m_pickerAssign ? "EMULATOR FOR THIS GAME" : "RUN WITH",
                 m_pickerGame.shown(), badge(int(m_pickerGame.system)), a);

    float y = b.y + PANEL_HEAD;
    for (int i = first; i < first + visible; i++, y += L::ROW_H) {
        char value[48];
        const char* label;
        bool selected = false;
        if (m_pickerAssign && i == 0) {
            label = "System default";
            std::snprintf(value, sizeof value, "%s%s",
                          m_pickerChoice ? "" : "Current \xC2\xB7 ",
                          m_pickerDefault ? m_pickerDefault->label() : "None");
        } else {
            const CoreInfo& c = *m_pickerCores[size_t(i - offset)];
            label = c.label();
            selected = m_pickerAssign ? &c == m_pickerChoice : &c == m_pickerCurrent;
            std::snprintf(value, sizeof value, "%s%s",
                          selected ? (m_pickerAssign ? "Current \xC2\xB7 "
                                                     : "In use \xC2\xB7 ")
                                   : "",
                          c.isNative() ? "Native"
                                       : (c.psp1000Safe ? "Any PSP" : "Testing"));
        }
        ui::RowStyle style;
        style.focused = i == m_pickerIdx;
        ui::menuRow(app, b.x + 3.f, y, b.w - 6.f, L::ROW_H, label, value, style,
                    a);
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
        if (row == VR_SHOW) value = db::filterName(m_view.filter);
        else if (row == VR_SORT)
            value = m_view.filter == db::ViewFilter::RecentlyAdded
                        ? "Newest first" : db::sortName(m_view.sort);
        else if (row == VR_SEARCH)
            value = m_query.empty() ? std::string("Press X")
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
    ui::label(app, b.x + b.w * .5f, capsAt(fonts.tiny, b.y + b.h - 15.f),
              "L / R SKIP FIVE  \xC2\xB7  SQUARE CLEARS", fade(pal.textMuted, a),
              text::Align::Center);
}

void HomeScene::drawError(App& app) {
    app.drawBackground();
    app.drawTopBar();
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
        const App::Hint hints[] = {
            {B::Cross, m_pickerAssign ? "Select" : "Play"},
            {B::Circle, "Cancel"}};
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
            App::Hint hints[4];
            int n = 0;
            if (!m_recents.empty()) hints[n++] = {B::DpadUp, "Continue"};
            hints[n++] = {B::Cross, "Library"};
            hints[n++] = {B::Triangle, "Settings"};
            app.drawHintBar(hints, n);
            break;
        }
        case nav::Layer::Continue: {
            const App::Hint hints[] = {
                {B::Cross, "Play"}, {B::Square, "Favorite"},
                {B::Triangle, "Details"}, {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 4);
            break;
        }
        case nav::Layer::Library: {
            const App::Hint hints[] = {
                {B::Cross, "Play"}, {B::Square, "Favorite"},
                {B::Triangle, "Details"}, {B::Select, "View"},
                {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 5);
            break;
        }
        case nav::Layer::Detail: {
            if (m_states && m_stateIdx >= save::SLOTS) {
                const App::Hint hints[] = {{B::Cross, "Delete"},
                                           {B::Circle, "Back"}};
                app.drawHintBar(hints, 2);
                break;
            }
            if (m_states) {
                const bool has = m_stateSlots[m_stateIdx].exists;
                App::Hint hints[3];
                int n = 0;
                if (has) hints[n++] = {B::Cross, "Load"};
                if (has) hints[n++] = {B::Triangle, "Delete"};
                hints[n++] = {B::Circle, "Back"};
                app.drawHintBar(hints, n);
                break;
            }
            const App::Hint hints[] = {
                {B::Cross, "Select"}, {B::Square, "Favorite"},
                {B::Circle, "Back"},
            };
            app.drawHintBar(hints, 3);
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

    app.drawTopBar();

    /* Layers draw inside the content band so travel never crosses the
     * header or the legend. */
    restoreContent(app.renderer());
    drawContinue(app, layerA(-1.f), offset(-1.f));
    drawSystems(app, layerA(0.f), offset(0.f));
    drawLibrary(app, layerA(1.f), offset(1.f));
    drawDetail(app, layerA(2.f), offset(2.f));
    app.renderer().resetScissor();

    drawLegend(app);

    if (m_overlay == Overlay::CorePicker) drawPicker(app);
    else if (m_overlay == Overlay::ViewMenu) drawViewMenu(app);
    else if (m_overlay == Overlay::Search) drawSearch(app);
}

}  // namespace rs
