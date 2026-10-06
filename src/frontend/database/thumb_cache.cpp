#include "frontend/database/thumb_cache.h"

#include "frontend/database/systems.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/threading.h"
#include "runtime/log.h"

#include "stb_image.h"

#include <pspgu.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace rs::db {

namespace {
constexpr u32 MAGIC = 0x48545352u;     /* "RSTH" */
constexpr u32 VERSION = 1;
constexpr u32 MAX_SOURCE_FILE = 2u * 1024u * 1024u;
constexpr int MAX_SOURCE_DIM = 2048;
/* Transparent pixels in a cover composite over the art well colour. */
constexpr int WELL_R = 0x12, WELL_G = 0x2B, WELL_B = 0x40;

void filePath(int system, char* out, size_t n) {
    std::snprintf(out, n, "%s/cache/thumbs/%s.bin", fs::ROOT,
                  systemInfo(System(system)).dirName);
}

/* Cover-crops the image to a square and box-filters it to SIZE x SIZE
 * RGB565 (R in the low bits, as the GE reads GU_PSM_5650). */
bool makeThumb(const std::string& path, u16* out) {
    std::vector<u8> file;
    if (!fs::readFile(path.c_str(), file, MAX_SOURCE_FILE)) return false;
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(file.data(), int(file.size()), &w, &h,
                                        &comp, 4);
    std::vector<u8>().swap(file);
    if (!px || w <= 0 || h <= 0 || w > MAX_SOURCE_DIM || h > MAX_SOURCE_DIM) {
        if (px) stbi_image_free(px);
        return false;
    }
    constexpr int S = ThumbCache::SIZE;
    const int side = w < h ? w : h;
    const int x0 = (w - side) / 2, y0 = (h - side) / 2;
    for (int oy = 0; oy < S; oy++) {
        const int sy0 = y0 + oy * side / S;
        int sy1 = y0 + (oy + 1) * side / S;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (int ox = 0; ox < S; ox++) {
            const int sx0 = x0 + ox * side / S;
            int sx1 = x0 + (ox + 1) * side / S;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            u32 r = 0, g = 0, b = 0, n = 0;
            for (int y = sy0; y < sy1; y++)
                for (int x = sx0; x < sx1; x++) {
                    const stbi_uc* p = px + (size_t(y) * size_t(w) + size_t(x)) * 4;
                    const u32 a = p[3];
                    r += (p[0] * a + WELL_R * (255u - a)) / 255u;
                    g += (p[1] * a + WELL_G * (255u - a)) / 255u;
                    b += (p[2] * a + WELL_B * (255u - a)) / 255u;
                    n++;
                }
            r /= n; g /= n; b /= n;
            out[oy * S + ox] = u16(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));
        }
    }
    stbi_image_free(px);
    return true;
}

/* Reads one system's file; false (and empty) when absent or malformed. */
template <typename EntryT>
bool readFileEntries(int system, std::vector<EntryT>& out) {
    out.clear();
    char path[128];
    filePath(system, path, sizeof path);
    std::vector<u8> data;
    if (!fs::exists(path) ||
        !fs::readFile(path, data, 4u * 1024u * 1024u) || data.size() < 12)
        return false;
    u32 magic, version, count;
    std::memcpy(&magic, data.data(), 4);
    std::memcpy(&version, data.data() + 4, 4);
    std::memcpy(&count, data.data() + 8, 4);
    if (magic != MAGIC || version != VERSION ||
        count > u32(ThumbCache::MAX_PER_SYSTEM) ||
        data.size() < 12 + size_t(count) * sizeof(EntryT))
        return false;
    out.resize(count);
    std::memcpy(out.data(), data.data() + 12, size_t(count) * sizeof(EntryT));
    return true;
}
}  // namespace

/* ---------------------------------------------------------------------- */
/* Main thread                                                             */
/* ---------------------------------------------------------------------- */

void ThumbCache::rebuild(const GameIndex& index, bool force) {
    /* Sources are grouped by system, each run opened by a zero-hash marker
     * that carries the system id; the worker writes one file per run. */
    Job job;
    job.kind = Job::Build;
    job.force = force;
    job.system = -1;
    for (int s = 0; s < SYSTEM_COUNT; s++) {
        const auto& games = index.games(System(s));
        Source marker;
        marker.path = std::to_string(s);
        job.sources.push_back(std::move(marker));
        for (const GameEntry& g : games) {
            Source src;
            src.hash = g.pathHash;
            src.path = g.artPath;       /* empty: try a screenshot */
            job.sources.push_back(std::move(src));
        }
    }
    /* A newer build replaces any queued one. */
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [](const Job& j) { return j.kind == Job::Build; }),
                  m_queue.end());
    m_queue.push_back(std::move(job));
    startNext();
}

void ThumbCache::requestSystem(int system) {
    m_wantSystem = system;
    if (m_residentSystem == system && m_residentGeneration == m_generation)
        return;
    if (m_threadId >= 0 && m_job.kind == Job::Load && m_job.system == system)
        return;
    for (const Job& j : m_queue)
        if (j.kind == Job::Load && j.system == system) return;
    /* Only the latest wanted system matters. */
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [](const Job& j) { return j.kind == Job::Load; }),
                  m_queue.end());
    Job job;
    job.kind = Job::Load;
    job.system = system;
    m_queue.push_back(std::move(job));
    startNext();
}

void ThumbCache::update() {
    if (m_threadId >= 0 && m_done.load()) {
        thread::join(m_threadId);
        m_threadId = -1;
        m_done.store(false);
        if (m_job.kind == Job::Load) {
            dropResident();
            m_resident = std::move(m_loaded);
            m_loaded.clear();
            m_residentSystem = m_job.system;
            m_residentGeneration = m_generation;
        } else {
            m_generation++;
            /* Pick up what the build just wrote for the visible system. */
            if (m_wantSystem >= 0) requestSystem(m_wantSystem);
        }
        m_job = Job{};
    }
    startNext();
}

void ThumbCache::startNext() {
    if (m_threadId >= 0 || m_queue.empty()) return;
    m_job = std::move(m_queue.front());
    m_queue.erase(m_queue.begin());
    m_stop.store(false);
    m_done.store(false);
    m_threadId = thread::spawn("rs_thumbs", &ThumbCache::threadMain, this, 64);
    if (m_threadId < 0) {
        RS_LOGW("thumbs: worker spawn failed");
        m_job = Job{};
        m_queue.clear();
    }
}

void ThumbCache::suspend() {
    m_stop.store(true);
    if (m_threadId >= 0) {
        thread::join(m_threadId);
        m_threadId = -1;
    }
    m_done.store(false);
    m_stop.store(false);
    m_queue.clear();
    m_loaded.clear();
    m_job = Job{};
    dropResident();
    for (Slot& s : m_ring) {
        gfx::Renderer::freeTexture(s.tex);
        s = Slot{};
    }
}

void ThumbCache::dropResident() {
    std::vector<Entry>().swap(m_resident);
    m_residentSystem = -1;
    for (Slot& s : m_ring) s.hash = 0;
}

const gfx::Texture* ThumbCache::get(u32 pathHash) {
    if (m_resident.empty() || pathHash == 0) return nullptr;
    m_clock++;
    for (Slot& s : m_ring)
        if (s.hash == pathHash && s.tex.valid()) {
            s.lastUse = m_clock;
            return &s.tex;
        }
    const auto it = std::lower_bound(
        m_resident.begin(), m_resident.end(), pathHash,
        [](const Entry& e, u32 h) { return e.hash < h; });
    if (it == m_resident.end() || it->hash != pathHash) return nullptr;

    /* Least recently used slot; at most a few uploads per frame of 1 KB. */
    Slot* victim = &m_ring[0];
    for (Slot& s : m_ring)
        if (s.lastUse < victim->lastUse) victim = &s;
    if (!victim->tex.valid() &&
        !gfx::Renderer::createTexture(victim->tex, SIZE, SIZE, GU_PSM_5650,
                                      it->px, /*dynamic=*/true))
        return nullptr;
    gfx::Renderer::updateTexture(victim->tex, it->px, SIZE * 2);
    victim->hash = pathHash;
    victim->lastUse = m_clock;
    return &victim->tex;
}

/* ---------------------------------------------------------------------- */
/* Worker                                                                  */
/* ---------------------------------------------------------------------- */

int ThumbCache::threadMain(void* self) {
    auto* t = static_cast<ThumbCache*>(self);
    t->runJob();
    t->m_done.store(true);
    return 0;
}

void ThumbCache::runJob() {
    if (m_job.kind == Job::Load) {
        readFileEntries(m_job.system, m_loaded);
        return;
    }

    char dir[96];
    std::snprintf(dir, sizeof dir, "%s/cache/thumbs", fs::ROOT);
    fs::mkdirs(dir);

    /* The newest screenshot per game stands in for a missing cover. */
    std::unordered_map<u32, std::pair<unsigned, std::string>> shots;
    {
        char shotDir[64];
        std::snprintf(shotDir, sizeof shotDir, "%s/screenshots", fs::ROOT);
        std::vector<fs::DirEntry> entries;
        if (fs::listDir(shotDir, entries))
            for (const auto& e : entries) {
                unsigned hash = 0, serial = 0;
                char tail[8] = {};
                if (e.isDir ||
                    std::sscanf(e.name.c_str(), "%8x_%u.%7s", &hash, &serial,
                                tail) != 3 ||
                    std::strcmp(tail, "png") != 0)
                    continue;
                auto& slot = shots[hash];
                if (slot.second.empty() || serial >= slot.first)
                    slot = {serial, std::string(shotDir) + "/" + e.name};
            }
    }

    int built = 0;
    size_t i = 0;
    const auto& src = m_job.sources;
    while (i < src.size() && !m_stop.load()) {
        /* Marker: start of a system's run. */
        const int system = std::atoi(src[i].path.c_str());
        i++;
        std::vector<Entry> existing;
        if (!m_job.force) readFileEntries(system, existing);
        std::vector<Entry> out;
        bool changed = m_job.force;
        for (; i < src.size() && src[i].hash != 0; i++) {
            if (m_stop.load()) return;
            if (int(out.size()) >= MAX_PER_SYSTEM) continue;
            const u32 hash = src[i].hash;
            const auto have = std::lower_bound(
                existing.begin(), existing.end(), hash,
                [](const Entry& e, u32 h) { return e.hash < h; });
            if (have != existing.end() && have->hash == hash) {
                out.push_back(*have);
                continue;
            }
            std::string path = src[i].path;
            if (path.empty()) {
                const auto shot = shots.find(hash);
                if (shot == shots.end()) continue;
                path = shot->second.second;
            }
            Entry e;
            e.hash = hash;
            if (!makeThumb(path, e.px)) continue;
            out.push_back(e);
            changed = true;
            built++;
            thread::sleepMs(1);     /* stay polite to the stick and the UI */
        }
        if (!changed && out.size() == existing.size()) continue;
        std::sort(out.begin(), out.end(),
                  [](const Entry& a, const Entry& b) { return a.hash < b.hash; });
        char path[128];
        filePath(system, path, sizeof path);
        if (out.empty()) {
            if (fs::exists(path)) fs::removeFile(path);
            continue;
        }
        std::vector<u8> blob(12 + out.size() * sizeof(Entry));
        const u32 header[3] = {MAGIC, VERSION, u32(out.size())};
        std::memcpy(blob.data(), header, 12);
        std::memcpy(blob.data() + 12, out.data(), out.size() * sizeof(Entry));
        if (!fs::writeFileAtomic(path, blob.data(), u32(blob.size())))
            RS_LOGW("thumbs: could not write %s", path);
    }
    if (built) RS_LOGI("thumbs: built %d", built);
}

}  // namespace rs::db
