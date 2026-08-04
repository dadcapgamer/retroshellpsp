#include "frontend/database/rom_scanner.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/threading.h"
#include "runtime/log.h"
#include "runtime/bounds.h"

#include "miniz.h"

#include <cctype>
#include <cstring>
#include <unordered_map>

namespace rs::db {

namespace {
constexpr u32 MAX_ZIP_FILE_BYTES = 256u * 1024u * 1024u;
constexpr u32 MAX_ROM_BYTES = 16u * 1024u * 1024u;
constexpr mz_uint MAX_ZIP_ENTRIES = 4096;
constexpr mz_uint64 MAX_COMPRESSION_RATIO = 200;
constexpr int MAX_SCAN_DEPTH = 8;
constexpr int MAX_SCANNED_FILES = 100000;
constexpr size_t MAX_ROM_PATH = 511;

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

std::string foldedStem(const char* fileName) {
    std::string stem = displayName(fileName);
    for (char& c : stem)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return stem;
}

const SystemInfo* systemForExtension(const char* ext) {
    for (int si = 0; si < SYSTEM_COUNT; si++) {
        const SystemInfo& info = systemInfo(System(si));
        if (extMatches(info.extensions, ext)) return &info;
    }
    return nullptr;
}

/* Reads the ZIP central directory and assigns the archive to the system of
 * its first supported ROM. Cheap: only directory records are touched. */
bool peekZip(const char* zipPath, GameEntry& g) {
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
        const SystemInfo* info = systemForExtension(ext);
        if (!info) continue;
        g.system   = info->id;
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
    /* FAT is case-insensitive on hardware, while host-backed PPSSPP paths
     * can be case-sensitive. Accept either spelling without scanning twice. */
    std::vector<fs::DirEntry> rootEntries;
    const char* root = fs::ROM_ROOT;
    if (!fs::listDir(root, rootEntries)) {
        root = "ms0:/roms";
        if (!fs::listDir(root, rootEntries)) {
            RS_LOGI("scanner: ROM root not found (%s)", fs::ROM_ROOT);
            return;
        }
    }
    std::vector<fs::DirEntry>().swap(rootEntries);
    scanDirectory(root, 0);
    RS_LOGI("scanner: found %d games (%d files inspected)",
            int(m_results.size()), int(m_progress));
}

void RomScanner::scanDirectory(const std::string& directory, int depth) {
    if (m_stopRequested.load() || depth > MAX_SCAN_DEPTH ||
        m_progress.load() >= MAX_SCANNED_FILES)
        return;

    std::vector<fs::DirEntry> entries;
    if (!fs::listDir(directory.c_str(), entries)) return;
    std::vector<std::string> subdirectories;
    std::unordered_map<std::string, std::string> siblingArt;

    /* Resolve covers once while the directory is already in memory. This
     * turns selection of a coverless game into zero Memory Stick accesses,
     * instead of probing six extensions for every highlighted ROM. */
    for (const auto& e : entries) {
        if (e.isDir) continue;
        char ext[16];
        extOf(e.name.c_str(), ext);
        if (std::strcmp(ext, "png") == 0 || std::strcmp(ext, "jpg") == 0 ||
            std::strcmp(ext, "jpeg") == 0) {
            const std::string path = directory + "/" + e.name;
            if (path.size() <= MAX_ROM_PATH)
                siblingArt.emplace(foldedStem(e.name.c_str()), path);
        }
    }

    for (const auto& e : entries) {
        if (m_stopRequested.load() ||
            m_progress.load() >= MAX_SCANNED_FILES)
            return;
        if (e.name.empty() || e.name == "." || e.name == ".." ||
            e.name.rfind("._", 0) == 0)
            continue;

        const std::string path = directory + "/" + e.name;
        if (path.size() > MAX_ROM_PATH) {
            RS_LOGW("scanner: skipping overlong path under %s",
                    directory.c_str());
            continue;
        }
        if (e.isDir) {
            subdirectories.push_back(path);
            continue;
        }

        m_progress.fetch_add(1);
        char ext[16];
        extOf(e.name.c_str(), ext);

        GameEntry g;
        g.name  = displayName(e.name.c_str());
        g.path  = path;
        g.mtime = e.mtime;
        if (const auto art = siblingArt.find(foldedStem(e.name.c_str()));
            art != siblingArt.end())
            g.artPath = art->second;

        if (std::strcmp(ext, "zip") == 0) {
            if (!peekZip(g.path.c_str(), g)) continue;
        } else {
            const SystemInfo* info = systemForExtension(ext);
            if (!info) continue;
            g.system = info->id;
            g.size = e.size;
        }
        g.pathHash = fnv1a(g.path.c_str());
        m_results.push_back(std::move(g));
    }

    /* Do not retain a potentially large directory listing while descending;
     * the scanner shares the frontend's deliberately small system heap. */
    std::vector<fs::DirEntry>().swap(entries);
    for (const auto& subdirectory : subdirectories) {
        scanDirectory(subdirectory, depth + 1);
        if (m_stopRequested.load() ||
            m_progress.load() >= MAX_SCANNED_FILES)
            return;
    }
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
