/** Text layout helpers shared by every scene: ellipsized single lines and
 * word-wrapped paragraphs. Titles in a real ROM library are arbitrarily
 * long, so nothing in the shell draws an unbounded string. */
#pragma once

#include "frontend/text/font.h"
#include "platform/psp/gu_renderer.h"

#include <string>

namespace rs::ui {

/* Draws `value` on one line, trimming with "..." at a UTF-8 boundary so it
 * fits `maxWidth`. */
void drawEllipsized(const text::Font& font, gfx::Renderer& r, float x, float y,
                    float maxWidth, const std::string& value, u32 color,
                    text::Align align = text::Align::Left, bool bold = false);

/* Word-wraps `value` into at most `maxLines` lines `step` apart; the last
 * line is ellipsized if the text does not fit. For Align::Center, `x` is the
 * centre of the block. */
void drawWrapped(const text::Font& font, gfx::Renderer& r, float x, float y,
                 float maxWidth, float step, int maxLines,
                 const std::string& value, u32 color,
                 text::Align align = text::Align::Left);

/* Number of lines drawWrapped would use (capped at maxLines). */
int wrappedLineCount(const text::Font& font, float maxWidth, int maxLines,
                     const std::string& value);

}  // namespace rs::ui
