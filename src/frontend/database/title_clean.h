/** Display-title cleanup for real-world ROM file names.
 *
 *   "Pokemon - Crystal Version (USA, Europe) (Rev 1)"  ->  "Pokémon Crystal"
 *   "Legend of Zelda, The - Link's Awakening DX (USA)" ->  "The Legend of
 *                                                          Zelda: Link's Awakening DX"
 *
 * The bracketed dump tags are not discarded: they come back as `variant`
 * ("USA, Europe, Rev 1"), which the library uses to tell duplicates apart and
 * the detail view shows as the file's provenance. The original file name is
 * never modified; it stays on the GameEntry for metadata/art lookup.
 *
 * Pure, header-only and free of PSP dependencies so the host tests can pin it.
 */
#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace rs::db {

struct CleanTitle {
    std::string title;     /* what lists and headers show                     */
    std::string variant;   /* "USA, Rev 1" — empty when the name had no tags  */
};

namespace detail {

inline bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

inline void collapseSpaces(std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool prevSpace = true;   /* also trims the leading edge */
    for (char c : s) {
        if (isSpace(c)) {
            if (!prevSpace) out.push_back(' ');
            prevSpace = true;
        } else {
            out.push_back(c);
            prevSpace = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    s.swap(out);
}

/* Tags that carry no information for a person browsing a list: the "!"
 * verified-dump marker, single-letter dump codes (b1, a2, h, o), and bare
 * language lists such as "En,Fr,De". */
inline bool noiseTag(const std::string& tag) {
    if (tag.empty() || tag == "!") return true;
    if (tag.size() <= 3 && std::isalpha(static_cast<unsigned char>(tag[0]))) {
        bool rest = true;
        for (size_t i = 1; i < tag.size(); i++)
            rest = rest && std::isdigit(static_cast<unsigned char>(tag[i]));
        if (rest) return true;
    }
    /* "En,Fr,De" / "En,Ja": every comma part is exactly two letters. */
    size_t start = 0;
    bool languageList = true;
    for (size_t i = 0; i <= tag.size(); i++) {
        if (i == tag.size() || tag[i] == ',') {
            const size_t len = i - start;
            if (len != 2 ||
                !std::isalpha(static_cast<unsigned char>(tag[start])) ||
                !std::isalpha(static_cast<unsigned char>(tag[start + 1])))
                languageList = false;
            start = i + 1;
        }
    }
    return languageList;
}

/* Replaces whole-word `from` with `to` (ASCII word boundaries). */
inline void replaceWord(std::string& s, const char* from, const char* to) {
    const std::string f = from;
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) {
        const bool leftOk = pos == 0 || !std::isalnum(
            static_cast<unsigned char>(s[pos - 1]));
        const size_t end = pos + f.size();
        const bool rightOk = end >= s.size() || !std::isalnum(
            static_cast<unsigned char>(s[end]));
        if (leftOk && rightOk) {
            s.replace(pos, f.size(), to);
            pos += std::char_traits<char>::length(to);
        } else {
            pos = end;
        }
    }
}

inline bool startsWith(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

/* "Legend of Zelda, The - A Link" -> "The Legend of Zelda - A Link". */
inline void moveTrailingArticle(std::string& s) {
    static const char* ARTICLES[] = {"The", "An", "A"};
    for (const char* article : ARTICLES) {
        const std::string pattern = std::string(", ") + article;
        size_t pos = 0;
        while ((pos = s.find(pattern, pos)) != std::string::npos) {
            const size_t end = pos + pattern.size();
            const bool boundary = end == s.size() || s[end] == ' ' ||
                                  s[end] == ':' || s[end] == '-';
            if (boundary && pos > 0) {
                const std::string head = s.substr(0, pos);
                const std::string tail = s.substr(end);
                s = std::string(article) + " " + head + tail;
                return;
            }
            pos = end;
        }
    }
}

}  // namespace detail

inline CleanTitle cleanTitle(const std::string& stem) {
    CleanTitle out;
    std::string s = stem;
    for (char& c : s)
        if (c == '_') c = ' ';

    /* Pull every (...) and [...] group out of the name. */
    std::string body;
    std::vector<std::string> tags;
    for (size_t i = 0; i < s.size();) {
        const char open = s[i];
        const char close = open == '(' ? ')' : open == '[' ? ']' : 0;
        if (close) {
            int depth = 1;
            size_t j = i + 1;
            while (j < s.size() && depth > 0) {
                if (s[j] == open) depth++;
                else if (s[j] == close) depth--;
                j++;
            }
            if (depth == 0) {
                tags.push_back(detail::trim(s.substr(i + 1, j - i - 2)));
                body.push_back(' ');
                i = j;
                continue;
            }
        }
        body.push_back(s[i++]);
    }
    detail::collapseSpaces(body);
    while (!body.empty() && (body.back() == '-' || body.back() == ' '))
        body.pop_back();
    while (!body.empty() && (body.front() == '-' || body.front() == ' '))
        body.erase(body.begin());

    if (body.empty()) {
        /* Nothing but tags (or nothing at all): keep the raw stem rather than
         * invent a title, so the row is never blank. */
        out.title = detail::trim(stem);
        return out;
    }

    detail::moveTrailingArticle(body);

    const bool pokemon = detail::startsWith(body, "Pokemon") ||
                         detail::startsWith(body, "Pok\xC3\xA9mon");
    detail::replaceWord(body, "Pokemon", "Pok\xC3\xA9mon");
    if (pokemon) {
        /* "Pokémon - Crystal Version" is called "Pokémon Crystal". */
        detail::replaceWord(body, "-", "");
        const std::string version = " Version";
        if (body.size() > version.size() &&
            body.compare(body.size() - version.size(), version.size(),
                         version) == 0)
            body.resize(body.size() - version.size());
    } else {
        /* " - " is a subtitle separator in dump naming; ": " reads naturally. */
        for (size_t pos = 0;
             (pos = body.find(" - ", pos)) != std::string::npos;) {
            body.replace(pos, 3, ": ");
            pos += 2;
        }
    }
    detail::collapseSpaces(body);
    out.title = body;

    std::string variant;
    for (const std::string& tag : tags) {
        if (detail::noiseTag(tag)) continue;
        if (!variant.empty()) variant += ", ";
        variant += tag;
    }
    out.variant = variant;
    return out;
}

}  // namespace rs::db
