/** One primitive for every "something other than content" state: an error
 * with a way out, an empty list that says what to do, and a loading state.
 * Scenes compose it with the shared control legend so these states look like
 * part of the same firmware instead of one-off screens. */
#pragma once

#include "frontend/ui/prim.h"
#include "rs_common.h"

#include <string>

namespace rs {
class App;
}

namespace rs::ui {

struct StatePanel {
    enum class Kind : u8 { Error, Empty, Loading, Done };
    Kind        kind = Kind::Error;
    const char* title = "";
    std::string message;                 /* wrapped, at most four lines */
    const char* detail = nullptr;        /* optional dim line, e.g. a count */
    /* Optional inline action, drawn as "[glyph] label" under the message. */
    const char* actionLabel = nullptr;
    prim::Button actionButton = prim::Button::Triangle;
};

/* Centred in the content area between the status bar and the legend.
 * `alpha` scales every colour; `dy` shifts the block for layer motion. */
void drawStatePanel(App& app, const StatePanel& panel, u32 alpha = 255,
                    float dy = 0.f);

}  // namespace rs::ui
