#include "src/frontend/scenes/home_nav.h"

#include <cassert>

using rs::nav::HomeNav;
using rs::nav::Layer;

int main() {
    /* Rail window: clamped at both ends, selected stays on screen. */
    assert(rs::nav::railFirst(0, 9, 5) == 0);
    assert(rs::nav::railFirst(1, 9, 5) == 0);      /* mockup: GBC selected */
    assert(rs::nav::railFirst(2, 9, 5) == 0);
    assert(rs::nav::railFirst(3, 9, 5) == 1);
    assert(rs::nav::railFirst(8, 9, 5) == 4);
    assert(rs::nav::railFirst(2, 3, 5) == 0);      /* fewer systems than slots */
    assert(rs::nav::railFirst(0, 0, 5) == 0);
    for (int sel = 0; sel < 9; sel++) {
        const int first = rs::nav::railFirst(sel, 9, 5);
        assert(sel >= first && sel < first + 5);
    }

    HomeNav n;
    assert(n.layer == Layer::Systems);

    /* Up from Systems reaches Continue only when something was played. */
    assert(!n.openContinue(0));
    assert(n.layer == Layer::Systems);
    assert(n.openContinue(3));
    assert(n.layer == Layer::Continue);
    assert(n.moveContinue(1, 3) && n.moveContinue(1, 3));
    assert(!n.moveContinue(1, 3));                 /* clamped, no wrap */
    assert(n.continueIdx == 2);
    assert(n.back() && n.layer == Layer::Systems);
    assert(!n.back());                             /* O on Systems is a no-op */

    /* Library needs games; Left/Right switches system without leaving it. */
    n.activeSystem = 1;
    assert(!n.openLibrary(0));
    assert(n.openLibrary(25));
    assert(n.layer == Layer::Library);
    assert(n.moveGame(1, 25) && n.moveGame(1, 25));
    assert(n.currentGame() == 2);

    assert(n.moveSystem(1, 3));                    /* GBC -> GBA, in place */
    n.activeSystem = 2;
    assert(n.layer == Layer::Library);
    assert(n.currentGame() == 0);
    assert(n.moveGame(1, 42));
    assert(n.moveSystem(-1, 3));                   /* and back */
    n.activeSystem = 1;
    assert(n.currentGame() == 2);                  /* remembered per system */
    n.activeSystem = 2;
    assert(n.currentGame() == 1);

    n.systemPos = 0;
    assert(!n.moveSystem(-1, 3));                  /* clamped at the left end */
    n.systemPos = 2;
    assert(!n.moveSystem(1, 3));                   /* and at the right end */

    /* Detail returns to wherever it opened from. */
    assert(n.openDetail() && n.layer == Layer::Detail);
    assert(n.back() && n.layer == Layer::Library);
    assert(n.home() && n.layer == Layer::Systems);
    assert(!n.home());

    n.layer = Layer::Systems;
    assert(n.openContinue(2));
    assert(n.openDetail());
    assert(n.back() && n.layer == Layer::Continue);
    assert(n.back() && n.layer == Layer::Systems);
    assert(!n.openDetail());                       /* not from Systems */

    /* Start goes Home from every layer. */
    n.layer = Layer::Library;
    assert(n.home() && n.layer == Layer::Systems);

    /* Library shrinking under the user keeps indices valid, and an emptied
     * Continue layer closes instead of showing nothing. */
    n.activeSystem = 2;
    n.currentGame() = 40;
    n.layer = Layer::Library;
    n.clamp(3, 10, 2);
    assert(n.currentGame() == 9 && n.layer == Layer::Library);
    n.clamp(3, 0, 2);
    assert(n.layer == Layer::Systems);
    n.layer = Layer::Continue;
    n.continueIdx = 4;
    n.clamp(3, 10, 0);
    assert(n.layer == Layer::Systems && n.continueIdx == 0);
    n.layer = Layer::Detail;
    n.clamp(3, 0, 0);
    assert(n.layer == Layer::Detail);              /* holds its own copy */

    /* Out-of-range system ids never index outside the table. */
    n.activeSystem = 999;
    n.currentGame() = 3;
    n.activeSystem = -5;
    (void)n.currentGame();
    /* A filtered Library may show zero rows while the system still has
     * games: the Library layer must survive, the highlight resets to row 0. */
    {
        HomeNav f;
        f.activeSystem = 1;
        assert(f.openLibrary(25));
        f.currentGame() = 7;
        f.clamp(3, /*gamesInView=*/0, 0, /*gamesInSystem=*/25);
        assert(f.layer == Layer::Library && f.currentGame() == 0);
        f.clamp(3, 0, 0, /*gamesInSystem=*/0);     /* system emptied by rescan */
        assert(f.layer == Layer::Systems);
        /* Without the extra argument the old single-count behaviour holds. */
        HomeNav g;
        g.activeSystem = 1;
        assert(g.openLibrary(5));
        g.clamp(3, 0, 0);
        assert(g.layer == Layer::Systems);
    }

    return 0;
}
