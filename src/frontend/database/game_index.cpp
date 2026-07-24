#include "frontend/database/game_index.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include <algorithm>
#include <cstring>

namespace rs::db {

namespace {
constexpr u32 CACHE_MAGIC   = 0x58495352u;  /* "RSIX" */
constexpr u32 CACHE_VERSION = 1;
const char* CACHE_PATH = "ms0:/RETROSUITE/cache/index.bin";

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

void GameIndex::replaceAll(std::vector<GameEntry> all) {
    for (auto& v : m_bySystem) v.clear();
    for (auto& g : all) m_bySystem[u8(g.system)].push_back(std::move(g));
    for (auto& v : m_bySystem)
        std::sort(v.begin(), v.end(),
                  [](const GameEntry& a, const GameEntry& b) {
                      return a.name < b.name;
                  });
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
        }
    }
    fs::mkdirs("ms0:/RETROSUITE/cache");
    return fs::writeFile(CACHE_PATH, out.data(), u32(out.size()));
}

bool GameIndex::loadCache() {
    std::vector<u8> buf;
    if (!fs::readFile(CACHE_PATH, buf)) return false;
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
            !getStr(p, end, g.zipEntry))
            return false;
        all.push_back(std::move(g));
    }
    replaceAll(std::move(all));
    RS_LOGI("index: cache loaded, %d games", totalCount());
    return true;
}

}  // namespace rs::db
