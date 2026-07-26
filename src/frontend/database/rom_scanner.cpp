#include "frontend/database/rom_scanner.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/threading.h"
#include "runtime/log.h"
#include "runtime/bounds.h"

#include "miniz.h"

#include <cctype>
#include <cstring>

namespace rs::db {

namespace {
constexpr u32 MAX_ZIP_FILE_BYTES = 256u * 1024u * 1024u;
constexpr u32 MAX_ROM_BYTES = 16u * 1024u * 1024u;
constexpr mz_uint MAX_ZIP_ENTRIES = 4096;
constexpr mz_uint64 MAX_COMPRESSION_RATIO = 200;

/* Lower-cased extension of `name` (no dot), or empty. */
void extOf(const char* name, char out[16]) {
    out[0] = 0;
    const char* dot = std::strrchr(name, '.');
    if (!dot || !dot[1]) return;
    size_t i = 0;
    for (dot++; *dot && i < 15; dot++, i++)
        out[i] = char(std::tolower(static_cast<unsigned char>(*dot)));
    out[i] = 0;
}

std::string displayName(const char* fileName) {
    std::string n = fileName;
    const size_t dot = n.rfind('.');
    if (dot != std::string::npos && dot > 0) n.resize(dot);
    return n;
}

/* Reads the zip central directory and returns the first entry matching the
 * system's extensions. Cheap: only directory records are touched. */
bool peekZip(const char* zipPath, const SystemInfo& info, GameEntry& g) {
    const s32 archiveSize = fs::fileSize(zipPath);
    if (archiveSize <= 0 || u32(archiveSize) > MAX_ZIP_FILE_BYTES) return false;
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zipPath, 0)) return false;

    bool found = false;
    const mz_uint n = mz_zip_reader_get_num_files(&zip);
    if (n > MAX_ZIP_ENTRIES) {
        mz_zip_reader_end(&zip);
        return false;
    }
    for (mz_uint i = 0; i < n && !found; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        if (st.m_is_directory) continue;
        if (!st.m_is_supported || st.m_is_encrypted ||
            !st.m_uncomp_size || st.m_uncomp_size > MAX_ROM_BYTES ||
            std::strlen(st.m_filename) > 255 ||
            !bounds::decompressionRatio(st.m_comp_size, st.m_uncomp_size,
                                        MAX_COMPRESSION_RATIO))
            continue;
        char ext[16];
        extOf(st.m_filename, ext);
        if (!extMatches(info.extensions, ext)) continue;
        g.zipEntry = st.m_filename;
        g.crc32    = u32(st.m_crc32);
        g.size     = u32(st.m_uncomp_size);
        found = true;
    }
    mz_zip_reader_end(&zip);
    return found;
}

}  // namespace

void RomScanner::start() {
    if (m_running.load()) return;
    if (m_threadId >= 0) {
        thread::join(m_threadId);
        m_threadId = -1;
    }
    m_stopRequested.store(false);
    m_running.store(true);
    m_done.store(false);
    m_progress.store(0);
    m_results.clear();
    m_threadId = thread::spawn("rs_scanner", &RomScanner::threadMain, this, 96);
    if (m_threadId < 0) {
        RS_LOGE("scanner: thread spawn failed, scanning inline");
        scan();
        m_done.store(true);
        m_running.store(false);
    }
}

void RomScanner::stop() {
    m_stopRequested.store(true);
    if (m_threadId >= 0) {
        thread::join(m_threadId);
        m_threadId = -1;
    }
    m_running.store(false);
    if (m_stopRequested.load()) {
        m_done.store(false);
        m_results.clear();
    }
}

int RomScanner::threadMain(void* self) {
    auto* s = static_cast<RomScanner*>(self);
    s->scan();
    s->m_done.store(!s->m_stopRequested.load());
    s->m_running.store(false);
    return 0;
}

void RomScanner::scan() {
    for (int si = 0; si < SYSTEM_COUNT; si++) {
        if (m_stopRequested.load()) return;
        const SystemInfo& info = systemInfo(System(si));
        char dir[128];
        std::snprintf(dir, sizeof dir, "%s/%s", fs::ROM_ROOT, info.dirName);

        std::vector<fs::DirEntry> entries;
        if (!fs::listDir(dir, entries)) continue;

        for (const auto& e : entries) {
            if (m_stopRequested.load()) return;
            if (e.isDir) continue;
            m_progress.fetch_add(1);

            char ext[16];
            extOf(e.name.c_str(), ext);

            GameEntry g;
            g.system = info.id;
            g.name   = displayName(e.name.c_str());
            g.path   = std::string(dir) + "/" + e.name;
            g.mtime  = e.mtime;

            if (std::strcmp(ext, "zip") == 0) {
                if (!peekZip(g.path.c_str(), info, g)) continue;
            } else if (extMatches(info.extensions, ext)) {
                g.size = e.size;
            } else {
                continue;
            }
            g.pathHash = fnv1a(g.path.c_str());
            m_results.push_back(std::move(g));
        }
    }
    RS_LOGI("scanner: found %d games (%d files inspected)",
            int(m_results.size()), int(m_progress));
}

bool RomScanner::takeResults(std::vector<GameEntry>& out) {
    if (!m_done.load()) return false;
    if (m_threadId >= 0) {
        thread::join(m_threadId);
        m_threadId = -1;
    }
    m_done.store(false);
    out = std::move(m_results);
    m_results.clear();
    return true;
}

}  // namespace rs::db
