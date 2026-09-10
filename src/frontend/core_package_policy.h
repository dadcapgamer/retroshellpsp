/** Pure validation helpers for drag-and-drop RetroShell core packages.
 * Kept free of PSP APIs so hostile path cases can be covered by host tests.
 */
#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace rs::corepkg {

inline bool safeCoreName(std::string_view name) {
    if (name.empty() || name.size() > 48) return false;
    for (const unsigned char c : name)
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    return true;
}

inline bool disqualifiedCoreName(std::string_view name) {
    /* Hardware safety tombstones. These names stay blocked even when stale
     * files or a third-party package are copied onto the Memory Stick. */
    return name == "fceumm" || name == "gearboy" ||
           name == "snes9x2005_plus" || name == "snes9x2010" ||
           name == "tgbdual" || name == "mgba" || name == "tempgba";
}

inline bool installableCoreName(std::string_view name) {
    /* The dummy module is a development fixture, never a user-installable
     * emulator. Known unsafe integrations are rejected at package intake. */
    return safeCoreName(name) && name != "dummy" &&
           !disqualifiedCoreName(name);
}

inline bool hasPackageSuffix(std::string_view name) {
    constexpr std::string_view suffix = ".rscore.zip";
    if (name.size() <= suffix.size()) return false;
    const auto tail = name.substr(name.size() - suffix.size());
    for (size_t i = 0; i < suffix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(tail[i])) != suffix[i])
            return false;
    return true;
}

inline bool safeArchivePath(std::string_view path) {
    if (path.empty() || path.size() > 255 || path.front() == '/' ||
        path.back() == '/' || path.find('\\') != std::string_view::npos ||
        path.find(':') != std::string_view::npos)
        return false;
    size_t start = 0;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const auto part = path.substr(
            start, slash == std::string_view::npos ? path.size() - start
                                                   : slash - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (const unsigned char c : part)
            if (c < 0x20 || c == 0x7f) return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}

inline bool directCoreManifestPath(std::string_view path) {
    constexpr std::string_view prefix = "RETROSHELL/cores/";
    constexpr std::string_view suffix = ".json";
    if (path.size() <= prefix.size() + suffix.size() ||
        path.substr(0, prefix.size()) != prefix ||
        path.substr(path.size() - suffix.size()) != suffix)
        return false;
    const auto name = path.substr(
        prefix.size(), path.size() - prefix.size() - suffix.size());
    return name.find('/') == std::string_view::npos && safeCoreName(name);
}

/* A working directory the native emulator expects to exist before launch.
 * Constrained to the adapter's own folder so a manifest can never ask the
 * frontend to create directories elsewhere on the Memory Stick. */
inline bool safeNativeDirectory(std::string_view name,
                                std::string_view relativePath) {
    if (!safeCoreName(name) || !safeArchivePath(relativePath)) return false;
    const std::string prefix =
        std::string("emulators/") + std::string(name) + "/";
    if (relativePath.size() <= prefix.size() ||
        relativePath.substr(0, prefix.size()) != prefix)
        return false;
    const auto leaf = relativePath.substr(prefix.size());
    return !leaf.empty() && leaf.find('/') == std::string_view::npos;
}

inline bool safeNativeExecutable(std::string_view name,
                                 std::string_view relativePath) {
    if (!safeCoreName(name) || !safeArchivePath(relativePath)) return false;
    const std::string prefix =
        std::string("emulators/") + std::string(name) + "/";
    if (relativePath.substr(0, prefix.size()) != prefix) return false;
    const auto leaf = relativePath.substr(prefix.size());
    return !leaf.empty() && leaf.find('/') == std::string_view::npos &&
           (leaf == "EBOOT.PBP" || leaf == "BOOT.PBP");
}

inline bool nativePayloadPath(std::string_view path, std::string_view name) {
    const std::string prefix =
        std::string("RETROSHELL/emulators/") + std::string(name) + "/";
    return path.substr(0, prefix.size()) == prefix &&
           path.size() > prefix.size() && safeArchivePath(path);
}

inline bool safeSystemFile(std::string_view relativePath) {
    constexpr std::string_view prefix = "system/";
    return relativePath.substr(0, prefix.size()) == prefix &&
           relativePath.size() > prefix.size() &&
           relativePath.find('/', prefix.size()) == std::string_view::npos &&
           safeArchivePath(relativePath);
}

inline bool safeNativeSupportFile(std::string_view name,
                                  std::string_view relativePath) {
    const std::string prefix =
        std::string("emulators/") + std::string(name) + "/";
    return safeCoreName(name) && safeArchivePath(relativePath) &&
           relativePath.substr(0, prefix.size()) == prefix &&
           relativePath.size() > prefix.size() &&
           relativePath.find('/', prefix.size()) == std::string_view::npos;
}

}  // namespace rs::corepkg
