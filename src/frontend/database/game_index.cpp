#include "frontend/database/game_index.h"
#include "frontend/database/natural_order.h"
#include "frontend/database/title_clean.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace rs::db {

namespace {
constexpr u32 CACHE_MAGIC   = 0x58495352u;  /* "RSIX" */
constexpr u32 CACHE_VERSION = 2;
const char* CACHE_PATH = "ms0:/RETROSHELL/cache/index.bin";

void put32(std::vector<u8>& v, u32 x) {
    v.push_back(u8(x)); v.push_back(u8(x >> 8));
    v.push_back(u8(x >> 16)); v.push_back(u8(x >> 24));
}
void putStr(std::vector<u8>& v, const std::string& s) {
    put32(v, u32(s.size()));
    v.insert(v.end(), s.begin(), s.end());
}
bool get32(const u8*& p, const u8* end, u32& x) {
    if (end - p < 4) return false;
    x = u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
    p += 4;
    return true;
}
bool getStr(const u8*& p, const u8* end, std::string& s) {
    u32 n;
    if (!get32(p, end, n) || u32(end - p) < n || n > 1024) return false;
    s.assign(reinterpret_cast<const char*>(p), n);
    p += n;
    return true;
}
}  // namespace

bool extMatches(const char* list, const char* ext) {
    const size_t n = std::strlen(ext);
    if (n == 0) return false;
    for (const char* p = list; *p;) {
        const char* sep = std::strchr(p, '|');
        const size_t len = sep ? size_t(sep - p) : std::strlen(p);
        if (len == n && std::strncmp(p, ext, n) == 0) return true;
        if (!sep) break;
        p = sep + 1;
    }
    return false;
}

u32 fnv1a(const char* s) {
    u32 h = 2166136261u;
    while (*s) {
        h ^= u8(*s++);
        h *= 16777619u;
    }
    return h;
}

int GameIndex::totalCount() const {
    int n = 0;
    for (const auto& v : m_bySystem) n += int(v.size());
    return n;
}

const GameEntry* GameIndex::byHash(u32 pathHash) const {
    for (const auto& v : m_bySystem)
        for (const auto& g : v)
            if (g.pathHash == pathHash) return &g;
    return nullptr;
}

namespace {

std::string parentFolder(const std::string& path) {
    const size_t last = path.rfind('/');
    if (last == std::string::npos || last == 0) return {};
    const size_t prev = path.rfind('/', last - 1);
    return path.substr(prev == std::string::npos ? 0 : prev + 1,
                       last - (prev == std::string::npos ? 0 : prev + 1));
}

std::string fold(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

/* Duplicate ROMs, region variants and revisions all clean down to the same
 * title. Where that happens inside one system, append what tells them apart —
 * the dump tags first, then the folder, then the whole file name — so the
 * list never shows two identical rows. Unique titles stay clean. */
void disambiguate(std::vector<GameEntry>& games) {
    /* A unique game's label stays empty and shown() falls back to the title,
     * so a 2,000-game library does not store every title twice. */
    for (auto& g : games) g.label.clear();
    size_t i = 0;
    while (i < games.size()) {
        size_t j = i + 1;
        const std::string key = fold(games[i].title);
        while (j < games.size() && fold(games[j].title) == key) j++;
        if (j - i > 1) {
            for (size_t k = i; k < j; k++) {
                GameEntry& g = games[k];
                g.label = g.variant.empty()
                              ? g.title
                              : g.title + " (" + g.variant + ")";
            }
            for (int pass = 0; pass < 2; pass++) {
                bool collision = false;
                for (size_t a = i; a < j && !collision; a++)
                    for (size_t b = a + 1; b < j; b++)
                        if (fold(games[a].label) == fold(games[b].label)) {
                            collision = true;
                            break;
                        }
                if (!collision) break;
                for (size_t k = i; k < j; k++) {
                    GameEntry& g = games[k];
                    if (pass == 0) {
                        const std::string folder = parentFolder(g.path);
                        if (!folder.empty())
                            g.label = g.label + " [" + folder + "]";
                    } else {
                        g.label = g.name;
                    }
                }
            }
        }
        i = j;
    }
}

}  // namespace

void GameIndex::replaceAll(std::vector<GameEntry> all) {
    for (auto& v : m_bySystem) v.clear();
    for (auto& g : all) {
        CleanTitle clean = cleanTitle(g.name);
        g.title = std::move(clean.title);
        g.variant = std::move(clean.variant);
        m_bySystem[u8(g.system)].push_back(std::move(g));
    }
    for (auto& v : m_bySystem) {
        std::sort(v.begin(), v.end(),
                  [](const GameEntry& a, const GameEntry& b) {
                      if (naturalNameLess(a.title, b.title)) return true;
                      if (naturalNameLess(b.title, a.title)) return false;
                      return naturalNameLess(a.name, b.name);
                  });
        disambiguate(v);
    }
    m_generation++;   /* every prior GameEntry* is now dangling */
}

bool GameIndex::saveCache() const {
    std::vector<u8> out;
    out.reserve(size_t(totalCount()) * 96 + 16);
    put32(out, CACHE_MAGIC);
    put32(out, CACHE_VERSION);
    put32(out, u32(totalCount()));
    for (const auto& v : m_bySystem) {
        for (const auto& g : v) {
            put32(out, u32(g.system));
            put32(out, g.pathHash);
            put32(out, g.crc32);
            put32(out, g.size);
            put32(out, g.mtime);
            putStr(out, g.name);
            putStr(out, g.path);
            putStr(out, g.zipEntry);
            putStr(out, g.artPath);
        }
    }
    fs::mkdirs("ms0:/RETROSHELL/cache");
    return out.size() <= fs::DEFAULT_MAX_FILE &&
           fs::writeFileAtomic(CACHE_PATH, out.data(), u32(out.size()));
}

bool GameIndex::loadCache() {
    std::vector<u8> buf;
    if (!fs::readFile(CACHE_PATH, buf, fs::DEFAULT_MAX_FILE)) return false;
    const u8* p = buf.data();
    const u8* end = p + buf.size();
    u32 magic, ver, count;
    if (!get32(p, end, magic) || magic != CACHE_MAGIC) return false;
    if (!get32(p, end, ver) || ver != CACHE_VERSION) return false;
    if (!get32(p, end, count) || count > 10000) return false;

    std::vector<GameEntry> all;
    all.reserve(count);
    for (u32 i = 0; i < count; i++) {
        GameEntry g;
        u32 sys;
        if (!get32(p, end, sys) || sys >= u32(SYSTEM_COUNT)) return false;
        g.system = System(sys);
        if (!get32(p, end, g.pathHash) || !get32(p, end, g.crc32) ||
            !get32(p, end, g.size) || !get32(p, end, g.mtime) ||
            !getStr(p, end, g.name) || !getStr(p, end, g.path) ||
            !getStr(p, end, g.zipEntry) || !getStr(p, end, g.artPath))
            return false;
        all.push_back(std::move(g));
    }
    replaceAll(std::move(all));
    RS_LOGI("index: cache loaded, %d games", totalCount());
    return true;
}

}  // namespace rs::db
