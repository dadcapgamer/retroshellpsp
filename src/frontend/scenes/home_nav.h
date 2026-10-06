/** Home navigation model: the spatial layers of the shell and the rules for
 * moving between them. Pure data and transitions — no rendering, no input
 * device — so the host test suite can pin the behaviour the redesign brief
 * specifies:
 *
 *   Horizontal = change system, vertical = move within the current layer,
 *   Up from Systems = Continue Playing, Down/X = Library, O = back,
 *   Start = Home, and the selected game is remembered per system.
 */
#pragma once

#include "rs_common.h"

#include <array>

namespace rs::nav {

enum class Layer : u8 { Systems, Continue, Library, Detail };

constexpr int MAX_SYSTEMS = 16;

/* First rail slot to show so the selected system stays on screen with the
 * rail clamped at both ends (the mockups show GB..SNES with GBC selected). */
inline int railFirst(int selected, int total, int visible) {
    if (total <= visible) return 0;
    return rsClamp(selected - visible / 2, 0, total - visible);
}

struct HomeNav {
    Layer layer = Layer::Systems;
    int   systemPos = 0;       /* index into the visible-systems list */
    int   activeSystem = 0;    /* db::System id of that slot; the scene
                                * refreshes it whenever systemPos changes */
    int   continueIdx = 0;
    Layer detailFrom = Layer::Library;   /* where O leaves Game Detail to */
    /* Indexed by system id, not rail slot, so a system gaining or losing its
     * first game never shifts another system's remembered selection. */
    std::array<int, MAX_SYSTEMS> gameIdx{};

    /* Left/Right on the rail and inside the Library both end up here, so the
     * selection memory and clamping rules cannot diverge between the two. */
    bool moveSystem(int dir, int systemCount) {
        if (systemCount <= 0) return false;
        const int next = rsClamp(systemPos + dir, 0, systemCount - 1);
        if (next == systemPos) return false;
        systemPos = next;
        return true;
    }

    int& currentGame() {
        return gameIdx[size_t(rsClamp(activeSystem, 0, MAX_SYSTEMS - 1))];
    }
    int currentGame() const {
        return gameIdx[size_t(rsClamp(activeSystem, 0, MAX_SYSTEMS - 1))];
    }

    bool moveGame(int dir, int gameCount) {
        if (gameCount <= 0) return false;
        const int next = rsClamp(currentGame() + dir, 0, gameCount - 1);
        if (next == currentGame()) return false;
        currentGame() = next;
        return true;
    }

    bool moveContinue(int dir, int recentCount) {
        if (recentCount <= 0) return false;
        const int next = rsClamp(continueIdx + dir, 0, recentCount - 1);
        if (next == continueIdx) return false;
        continueIdx = next;
        return true;
    }

    /* Continue Playing is omitted entirely when nothing was played. */
    bool openContinue(int recentCount) {
        if (layer != Layer::Systems || recentCount <= 0) return false;
        continueIdx = rsClamp(continueIdx, 0, recentCount - 1);
        layer = Layer::Continue;
        return true;
    }

    bool openLibrary(int gameCount) {
        if (layer != Layer::Systems || gameCount <= 0) return false;
        layer = Layer::Library;
        return true;
    }

    /* Detail opens from either list layer and returns to where it came from. */
    bool openDetail() {
        if (layer != Layer::Library && layer != Layer::Continue) return false;
        detailFrom = layer;
        layer = Layer::Detail;
        return true;
    }

    /* O: Detail -> where it opened, Library/Continue -> Systems. */
    bool back() {
        switch (layer) {
            case Layer::Detail:   layer = detailFrom; return true;
            case Layer::Library:
            case Layer::Continue: layer = Layer::Systems; return true;
            case Layer::Systems:  return false;
        }
        return false;
    }

    /* Start: Home from anywhere. */
    bool home() {
        if (layer == Layer::Systems) return false;
        layer = Layer::Systems;
        return true;
    }

    /* Keeps indices valid after the library changes under the user. */
    void clamp(int systemCount, int gamesInCurrent, int recentCount,
               int gamesInSystem = -1) {
        /* A filtered Library can show zero rows while the system still has
         * games; the Library layer is only abandoned when the system itself
         * has none. */
        if (gamesInSystem < 0) gamesInSystem = gamesInCurrent;
        systemPos = systemCount > 0 ? rsClamp(systemPos, 0, systemCount - 1) : 0;
        currentGame() = gamesInCurrent > 0
            ? rsClamp(currentGame(), 0, gamesInCurrent - 1) : 0;
        continueIdx = recentCount > 0
            ? rsClamp(continueIdx, 0, recentCount - 1) : 0;
        if (layer == Layer::Continue && recentCount <= 0)
            layer = Layer::Systems;
        if (layer == Layer::Library && gamesInSystem <= 0)
            layer = Layer::Systems;
        /* Detail holds its own copy of the game, so it survives a rescan. */
    }
};

}  // namespace rs::nav
