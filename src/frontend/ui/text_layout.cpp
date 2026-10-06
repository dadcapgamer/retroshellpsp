#include "frontend/ui/text_layout.h"

#include <cstdio>
#include <cstring>

namespace rs::ui {

void drawEllipsized(const text::Font& font, gfx::Renderer& r, float x,
                    float y, float maxWidth, const std::string& value,
                    u32 color, text::Align align, bool bold) {
    auto put = [&](const char* t) {
        if (bold) font.drawBold(r, x, y, t, color, align);
        else font.draw(r, x, y, t, color, align);
    };
    if (font.measure(value.c_str()) <= maxWidth) {
        put(value.c_str());
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
            put(output);
            return;
        }
    }
    put("...");
}

void drawWrapped(const text::Font& font, gfx::Renderer& r, float x, float y,
                 float maxWidth, float step, int maxLines,
                 const std::string& value, u32 color, text::Align align) {
    if (value.empty() || maxLines <= 0) return;
    const char* p = value.c_str();
    char line[160] = {};
    int len = 0;
    int lines = 0;
    while (*p && lines < maxLines) {
        while (*p == ' ') ++p;
        const char* word = p;
        while (*p && *p != ' ') ++p;
        const int wordLen = int(p - word);
        if (wordLen <= 0) break;

        char candidate[160];
        const int addSpace = len > 0 ? 1 : 0;
        const int nextLen = rsClamp(len + addSpace + wordLen, 0, 159);
        std::memcpy(candidate, line, size_t(len));
        if (addSpace) candidate[len] = ' ';
        std::memcpy(candidate + len + addSpace, word,
                    size_t(nextLen - len - addSpace));
        candidate[nextLen] = '\0';

        if (len > 0 && font.measure(candidate) > maxWidth) {
            font.draw(r, x, y + float(lines) * step, line, color, align);
            lines++;
            len = 0;
            line[0] = '\0';
            if (lines >= maxLines) return;
        }
        if (len > 0) line[len++] = ' ';
        const int copy = rsClamp(wordLen, 0, 159 - len);
        std::memcpy(line + len, word, size_t(copy));
        len += copy;
        line[len] = '\0';
    }
    if (len > 0 && lines < maxLines) {
        const bool more = *p != '\0';
        if (more && lines == maxLines - 1)
            drawEllipsized(font, r, x, y + float(lines) * step, maxWidth,
                           std::string(line) + " ...", color);
        else
            font.draw(r, x, y + float(lines) * step, line, color, align);
    }
}

int wrappedLineCount(const text::Font& font, float maxWidth, int maxLines,
                     const std::string& value) {
    if (value.empty() || maxLines <= 0) return 0;
    const char* p = value.c_str();
    std::string line;
    int lines = 0;
    while (*p && lines < maxLines) {
        while (*p == ' ') ++p;
        const char* word = p;
        while (*p && *p != ' ') ++p;
        if (p == word) break;
        const std::string next =
            line.empty() ? std::string(word, size_t(p - word))
                         : line + " " + std::string(word, size_t(p - word));
        if (!line.empty() && font.measure(next.c_str()) > maxWidth) {
            lines++;
            line.assign(word, size_t(p - word));
        } else {
            line = next;
        }
    }
    if (!line.empty() && lines < maxLines) lines++;
    return lines;
}

}  // namespace rs::ui
