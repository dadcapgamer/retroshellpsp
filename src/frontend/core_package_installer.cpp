#include "frontend/core_package_installer.h"

#include "core_api/rs_core_api.h"
#include "frontend/core_package_policy.h"
#include "frontend/database/systems.h"
#include "platform/psp/fs_psp.h"
#include "runtime/bounds.h"
#include "runtime/log.h"

#include "cJSON.h"
#include "miniz.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace rs::corepkg {
namespace {

constexpr u32 MAX_PACKAGE_BYTES = 24u * 1024u * 1024u;
constexpr u64 MAX_EXPANDED_BYTES = 24u * 1024u * 1024u;
constexpr u64 MAX_PRX_BYTES = 6u * 1024u * 1024u;
constexpr u64 MAX_NATIVE_FILE_BYTES = 8u * 1024u * 1024u;
constexpr u64 MAX_MANIFEST_BYTES = 8u * 1024u;
constexpr u64 MAX_METADATA_BYTES = 256u * 1024u;
constexpr mz_uint MAX_ENTRIES = 40;
constexpr mz_uint INVALID_INDEX = mz_uint(-1);
constexpr u64 MAX_COMPRESSION_RATIO = 200;

bool knownSystems(const char* systems) {
    if (!systems || !*systems || std::strlen(systems) > 64) return false;
    const char* p = systems;
    while (*p) {
        const char* end = std::strchr(p, '|');
        const size_t len = end ? size_t(end - p) : std::strlen(p);
        bool known = false;
        for (int i = 0; i < db::SYSTEM_COUNT; ++i) {
            const char* id = db::systemInfo(db::System(i)).coreId;
            if (std::strlen(id) == len && std::strncmp(id, p, len) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
        if (!end) break;
        p = end + 1;
    }
    return true;
}

bool readEntry(mz_zip_archive& zip, mz_uint index, u64 maxBytes,
               std::vector<u8>& out) {
    mz_zip_archive_file_stat st{};
    if (!mz_zip_reader_file_stat(&zip, index, &st) || st.m_is_directory ||
        !st.m_is_supported || st.m_is_encrypted || !st.m_uncomp_size ||
        st.m_uncomp_size > maxBytes || st.m_uncomp_size > SIZE_MAX ||
        !bounds::decompressionRatio(st.m_comp_size, st.m_uncomp_size,
                                    MAX_COMPRESSION_RATIO))
        return false;
    out.resize(size_t(st.m_uncomp_size));
    return mz_zip_reader_extract_to_mem(&zip, index, out.data(), out.size(), 0);
}

bool metadataPath(std::string_view path, std::string_view name) {
    constexpr std::string_view licensePrefix = "RETROSHELL/licenses/";
    if (path.substr(0, licensePrefix.size()) == licensePrefix)
        return path.find('/', licensePrefix.size()) == std::string_view::npos;
    const std::string prefix =
        std::string("RETROSHELL/core-packages/") + std::string(name) + "/";
    if (path.substr(0, prefix.size()) != prefix) return false;
    const auto leaf = path.substr(prefix.size());
    return leaf == "package.json" || leaf == "provenance.json" ||
           leaf == "README.txt";
}

bool installOne(const char* packagePath) {
    const s32 archiveBytes = fs::fileSize(packagePath);
    if (archiveBytes <= 0 || u32(archiveBytes) > MAX_PACKAGE_BYTES) {
        RS_LOGW("core package: rejected oversized archive %s", packagePath);
        return false;
    }

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, packagePath, 0)) {
        RS_LOGW("core package: invalid ZIP %s", packagePath);
        return false;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    mz_uint manifestIndex = INVALID_INDEX;
    u64 expandedBytes = 0;
    bool archiveValid = count > 0 && count <= MAX_ENTRIES;
    for (mz_uint i = 0; archiveValid && i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
            continue;
        const std::string_view path(st.m_filename);
        if (!safeArchivePath(path) || !st.m_is_supported || st.m_is_encrypted ||
            !st.m_uncomp_size ||
            !bounds::decompressionRatio(st.m_comp_size, st.m_uncomp_size,
                                        MAX_COMPRESSION_RATIO) ||
            expandedBytes > MAX_EXPANDED_BYTES - st.m_uncomp_size) {
            archiveValid = false;
            break;
        }
        expandedBytes += st.m_uncomp_size;
        if (directCoreManifestPath(path)) {
            if (manifestIndex != INVALID_INDEX) archiveValid = false;
            manifestIndex = i;
        }
    }
    if (!archiveValid || manifestIndex == INVALID_INDEX) {
        RS_LOGW("core package: unsafe layout in %s", packagePath);
        mz_zip_reader_end(&zip);
        return false;
    }

    std::vector<u8> manifestBytes;
    if (!readEntry(zip, manifestIndex, MAX_MANIFEST_BYTES, manifestBytes)) {
        RS_LOGW("core package: manifest extraction failed in %s", packagePath);
        mz_zip_reader_end(&zip);
        return false;
    }
    cJSON* manifest = cJSON_ParseWithLength(
        reinterpret_cast<const char*>(manifestBytes.data()),
        manifestBytes.size());
    const cJSON* nameNode = manifest ? cJSON_GetObjectItem(manifest, "name") : nullptr;
    const cJSON* versionNode = manifest ? cJSON_GetObjectItem(manifest, "version") : nullptr;
    const cJSON* systemsNode = manifest ? cJSON_GetObjectItem(manifest, "systems") : nullptr;
    const cJSON* priorityNode = manifest ? cJSON_GetObjectItem(manifest, "priority") : nullptr;
    const cJSON* testNode = manifest ? cJSON_GetObjectItem(manifest, "testOnly") : nullptr;
    const cJSON* safeNode = manifest ? cJSON_GetObjectItem(manifest, "psp1000Safe") : nullptr;
    const cJSON* backendNode = manifest ? cJSON_GetObjectItem(manifest, "backend") : nullptr;
    const cJSON* executableNode = manifest ? cJSON_GetObjectItem(manifest, "executable") : nullptr;
    const cJSON* adapterProtocolNode = manifest
        ? cJSON_GetObjectItem(manifest, "adapterProtocol") : nullptr;
    const cJSON* pauseModeNode = manifest
        ? cJSON_GetObjectItem(manifest, "pauseMode") : nullptr;
    const cJSON* pauseHotkeyNode = manifest
        ? cJSON_GetObjectItem(manifest, "pauseHotkey") : nullptr;
    const cJSON* returnModeNode = manifest
        ? cJSON_GetObjectItem(manifest, "returnMode") : nullptr;
    const bool nativeBackend = cJSON_IsString(backendNode) &&
        std::strcmp(backendNode->valuestring, "native") == 0;
    const int manifestAdapterProtocol = cJSON_IsNumber(adapterProtocolNode)
        ? adapterProtocolNode->valueint : 0;
    /* Capture contract validity before deleting the parsed manifest below.
     * Never retain cJSON child pointers beyond their owning tree. */
    const bool manifestAdapterContractValid =
        manifestAdapterProtocol == 1 &&
        cJSON_IsString(pauseModeNode) &&
        std::strcmp(pauseModeNode->valuestring, "embedded") == 0 &&
        cJSON_IsString(pauseHotkeyNode) &&
        std::strcmp(pauseHotkeyNode->valuestring, "L+R+SELECT") == 0 &&
        cJSON_IsString(returnModeNode) &&
        std::strcmp(returnModeNode->valuestring, "loadexec") == 0;
    const bool validManifest = cJSON_IsString(nameNode) &&
        cJSON_IsString(versionNode) && std::strlen(versionNode->valuestring) <= 48 &&
        cJSON_IsString(systemsNode) && knownSystems(systemsNode->valuestring) &&
        cJSON_IsNumber(priorityNode) && cJSON_IsBool(testNode) &&
        cJSON_IsBool(safeNode) && installableCoreName(nameNode->valuestring) &&
        (!backendNode ||
         (cJSON_IsString(backendNode) &&
          (std::strcmp(backendNode->valuestring, "prx") == 0 ||
           nativeBackend))) &&
        (!nativeBackend ||
         (cJSON_IsString(executableNode) &&
          safeNativeExecutable(nameNode->valuestring,
                               executableNode->valuestring)));
    if (!validManifest) {
        RS_LOGW("core package: invalid manifest in %s", packagePath);
        if (manifest) cJSON_Delete(manifest);
        mz_zip_reader_end(&zip);
        return false;
    }
    const std::string name(nameNode->valuestring);
    const std::string version(versionNode->valuestring);
    const std::string executable = nativeBackend
        ? executableNode->valuestring : "";
    const std::string manifestEntry = "RETROSHELL/cores/" + name + ".json";
    const std::string prxEntry = "RETROSHELL/cores/" + name + ".prx";
    const std::string descriptorEntry =
        "RETROSHELL/core-packages/" + name + "/package.json";
    cJSON_Delete(manifest);

    mz_zip_archive_file_stat manifestStat{};
    if (!mz_zip_reader_file_stat(&zip, manifestIndex, &manifestStat) ||
        manifestEntry != manifestStat.m_filename) {
        RS_LOGW("core package: manifest filename does not match core name");
        mz_zip_reader_end(&zip);
        return false;
    }

    const int prxIndex = mz_zip_reader_locate_file(
        &zip, prxEntry.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    const int descriptorIndex = mz_zip_reader_locate_file(
        &zip, descriptorEntry.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    mz_zip_archive_file_stat prxStat{};
    const std::string nativeExecutableEntry =
        nativeBackend ? std::string("RETROSHELL/") + executable : "";
    const int nativeExecutableIndex = nativeBackend
        ? mz_zip_reader_locate_file(&zip, nativeExecutableEntry.c_str(),
                                    nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE)
        : -1;
    mz_zip_archive_file_stat nativeExecutableStat{};
    const bool artifactValid = nativeBackend
        ? nativeExecutableIndex >= 0 &&
          mz_zip_reader_file_stat(&zip, mz_uint(nativeExecutableIndex),
                                  &nativeExecutableStat) &&
          !nativeExecutableStat.m_is_directory &&
          nativeExecutableStat.m_uncomp_size >= 32u * 1024u &&
          nativeExecutableStat.m_uncomp_size <= MAX_NATIVE_FILE_BYTES
        : prxIndex >= 0 &&
          mz_zip_reader_file_stat(&zip, mz_uint(prxIndex), &prxStat) &&
          !prxStat.m_is_directory && prxStat.m_is_supported &&
          !prxStat.m_is_encrypted && prxStat.m_uncomp_size >= 32u * 1024u &&
          prxStat.m_uncomp_size <= MAX_PRX_BYTES;
    if (descriptorIndex < 0 || !artifactValid) {
        RS_LOGW("core package: missing or invalid executable/descriptor for %s",
                name.c_str());
        mz_zip_reader_end(&zip);
        return false;
    }

    std::vector<u8> descriptorBytes;
    if (!readEntry(zip, mz_uint(descriptorIndex), MAX_METADATA_BYTES,
                   descriptorBytes)) {
        mz_zip_reader_end(&zip);
        return false;
    }
    cJSON* descriptor = cJSON_ParseWithLength(
        reinterpret_cast<const char*>(descriptorBytes.data()),
        descriptorBytes.size());
    const cJSON* kind = descriptor ? cJSON_GetObjectItem(descriptor, "kind") : nullptr;
    const cJSON* format = descriptor ? cJSON_GetObjectItem(descriptor, "formatVersion") : nullptr;
    const cJSON* coreApi = descriptor ? cJSON_GetObjectItem(descriptor, "coreApiVersion") : nullptr;
    const cJSON* packageName = descriptor ? cJSON_GetObjectItem(descriptor, "name") : nullptr;
    const cJSON* packageVersion = descriptor ? cJSON_GetObjectItem(descriptor, "version") : nullptr;
    const cJSON* packageExecutable = descriptor
        ? cJSON_GetObjectItem(descriptor, "executable") : nullptr;
    const cJSON* packageAdapterProtocol = descriptor
        ? cJSON_GetObjectItem(descriptor, "adapterProtocol") : nullptr;
    const cJSON* packagePauseMode = descriptor
        ? cJSON_GetObjectItem(descriptor, "pauseMode") : nullptr;
    const cJSON* packagePauseHotkey = descriptor
        ? cJSON_GetObjectItem(descriptor, "pauseHotkey") : nullptr;
    const cJSON* packageReturnMode = descriptor
        ? cJSON_GetObjectItem(descriptor, "returnMode") : nullptr;
    const bool legacyNativeDescriptor =
        cJSON_IsNumber(format) && format->valueint == 4 &&
        manifestAdapterProtocol == 0;
    const bool protocolNativeDescriptor =
        cJSON_IsNumber(format) && format->valueint == 5 &&
        manifestAdapterContractValid &&
        cJSON_IsNumber(packageAdapterProtocol) &&
        packageAdapterProtocol->valueint == manifestAdapterProtocol &&
        cJSON_IsString(packagePauseMode) &&
        std::strcmp(packagePauseMode->valuestring, "embedded") == 0 &&
        cJSON_IsString(packagePauseHotkey) &&
        std::strcmp(packagePauseHotkey->valuestring, "L+R+SELECT") == 0 &&
        cJSON_IsString(packageReturnMode) &&
        std::strcmp(packageReturnMode->valuestring, "loadexec") == 0;
    const bool descriptorKindValid = nativeBackend
        ? cJSON_IsString(kind) &&
          std::strcmp(kind->valuestring, "retroshell-emulator") == 0 &&
          (legacyNativeDescriptor || protocolNativeDescriptor) &&
          cJSON_IsString(packageExecutable) &&
          executable == packageExecutable->valuestring
        : cJSON_IsString(kind) &&
          std::strcmp(kind->valuestring, "retroshell-core") == 0 &&
          cJSON_IsNumber(format) && format->valueint == 3 &&
          cJSON_IsNumber(coreApi) &&
          coreApi->valueint == int(RS_CORE_API_VERSION);
    const bool validDescriptor = descriptorKindValid &&
        cJSON_IsString(packageName) && name == packageName->valuestring &&
        cJSON_IsString(packageVersion) && version == packageVersion->valuestring;
    if (descriptor) cJSON_Delete(descriptor);
    if (!validDescriptor) {
        RS_LOGW("core package: incompatible format/API descriptor for %s",
                name.c_str());
        mz_zip_reader_end(&zip);
        return false;
    }

    /* Reject hidden payloads. Only the exact core pair plus bounded license
     * and package metadata are accepted, even though we never extract an
     * unrecognised entry. */
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
            continue;
        const std::string_view path(st.m_filename);
        if (path != manifestEntry &&
            ((!nativeBackend && path == prxEntry) ||
             (nativeBackend && nativePayloadPath(path, name))) == false &&
            !metadataPath(path, name)) {
            RS_LOGW("core package: unexpected entry %s", st.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    char coreDir[96], finalPrx[160], finalJson[160], tempPrx[176], tempJson[176];
    std::snprintf(coreDir, sizeof coreDir, "%s/cores", fs::ROOT);
    std::snprintf(finalPrx, sizeof finalPrx, "%s/%s.prx", coreDir, name.c_str());
    std::snprintf(finalJson, sizeof finalJson, "%s/%s.json", coreDir, name.c_str());
    std::snprintf(tempPrx, sizeof tempPrx, "%s.pkg.tmp", finalPrx);
    std::snprintf(tempJson, sizeof tempJson, "%s.pkg.tmp", finalJson);
    fs::mkdirs(coreDir);
    fs::removeFile(tempPrx);
    fs::removeFile(tempJson);
    bool installedPayload = true;
    if (nativeBackend) {
        struct PreparedFile {
            std::string destination;
            std::string temporary;
            std::string backup;
            bool hadOld = false;
            bool promoted = false;
        };
        std::vector<PreparedFile> prepared;
        prepared.reserve(count + 1);

        /* Prepare every large file before replacing anything. Holding a
         * 5-8 MB executable in std::vector would exceed the 4 MB UI heap. */
        for (mz_uint i = 0; installedPayload && i < count; ++i) {
            mz_zip_archive_file_stat st{};
            if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
                continue;
            const std::string_view entry(st.m_filename);
            if (!nativePayloadPath(entry, name)) continue;
            if (st.m_uncomp_size > MAX_NATIVE_FILE_BYTES) {
                installedPayload = false;
                break;
            }
            const std::string relative(entry.substr(std::strlen("RETROSHELL/")));
            const std::string destination = std::string(fs::ROOT) + "/" + relative;
            const std::string temporary = destination + ".pkg.tmp";
            const size_t slash = destination.rfind('/');
            if (slash == std::string::npos ||
                !fs::mkdirs(destination.substr(0, slash).c_str())) {
                installedPayload = false;
                break;
            }
            fs::removeFile(temporary.c_str());
            installedPayload =
                mz_zip_reader_extract_to_file(&zip, i, temporary.c_str(), 0) &&
                fs::fileSize(temporary.c_str()) == s32(st.m_uncomp_size);
            if (installedPayload)
                prepared.push_back({destination, temporary,
                                    destination + ".pkg.bak"});
            else
                fs::removeFile(temporary.c_str());
        }

        /* The manifest is promoted last, so discovery never sees a native
         * adapter before all of its executable dependencies are ready. */
        fs::removeFile(tempJson);
        installedPayload = installedPayload &&
            mz_zip_reader_extract_to_file(&zip, manifestIndex, tempJson, 0) &&
            fs::fileSize(tempJson) == s32(manifestBytes.size());
        if (installedPayload)
            prepared.push_back({finalJson, tempJson,
                                std::string(finalJson) + ".pkg.bak"});

        for (auto& file : prepared) {
            if (!installedPayload) break;
            fs::removeFile(file.backup.c_str());
            file.hadOld = fs::exists(file.destination.c_str());
            if (file.hadOld &&
                !fs::renameFile(file.destination.c_str(), file.backup.c_str())) {
                installedPayload = false;
                break;
            }
            if (!fs::renameFile(file.temporary.c_str(),
                                file.destination.c_str())) {
                if (file.hadOld)
                    fs::renameFile(file.backup.c_str(), file.destination.c_str());
                installedPayload = false;
                break;
            }
            file.promoted = true;
        }
        if (!installedPayload) {
            for (auto it = prepared.rbegin(); it != prepared.rend(); ++it) {
                if (it->promoted) fs::removeFile(it->destination.c_str());
                if (it->hadOld)
                    fs::renameFile(it->backup.c_str(), it->destination.c_str());
                fs::removeFile(it->temporary.c_str());
            }
        } else {
            for (const auto& file : prepared)
                fs::removeFile(file.backup.c_str());
        }
        if (installedPayload) fs::removeFile(finalPrx); /* obsolete adapter */
    } else {
        const bool extracted = mz_zip_reader_extract_to_file(
            &zip, mz_uint(prxIndex), tempPrx, 0) &&
            mz_zip_reader_extract_to_file(&zip, manifestIndex, tempJson, 0);
        installedPayload = extracted &&
            fs::fileSize(tempPrx) == s32(prxStat.m_uncomp_size) &&
            fs::fileSize(tempJson) == s32(manifestBytes.size()) &&
            fs::replaceFilePairAtomic(finalPrx, tempPrx, finalJson, tempJson);
    }
    if (!installedPayload) {
        RS_LOGW("core package: transactional install failed for %s", name.c_str());
        fs::removeFile(tempPrx);
        fs::removeFile(tempJson);
        mz_zip_reader_end(&zip);
        return false;
    }

    /* Preserve only small provenance/license records. The original archive
     * is consumed after success so it does not double Memory Stick usage. */
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
            continue;
        const std::string_view entry(st.m_filename);
        if (!metadataPath(entry, name)) continue;
        std::vector<u8> bytes;
        if (!readEntry(zip, i, MAX_METADATA_BYTES, bytes)) continue;
        const std::string relative(entry.substr(std::strlen("RETROSHELL/")));
        const std::string destination = std::string(fs::ROOT) + "/" + relative;
        const size_t slash = destination.rfind('/');
        if (slash != std::string::npos)
            fs::mkdirs(destination.substr(0, slash).c_str());
        fs::writeFileAtomic(destination.c_str(), bytes.data(), u32(bytes.size()));
    }
    mz_zip_reader_end(&zip);

    if (!fs::removeFile(packagePath)) {
        char consumed[384];
        std::snprintf(consumed, sizeof consumed, "%s.installed", packagePath);
        fs::removeFile(consumed);
        if (!fs::renameFile(packagePath, consumed))
            RS_LOGW("core package: installed but could not consume %s",
                    packagePath);
    }
    RS_LOGI("core package: installed %s %s", name.c_str(), version.c_str());
    return true;
}

void scanDirectory(const char* directory, InstallReport& report) {
    std::vector<fs::DirEntry> entries;
    if (!fs::listDir(directory, entries)) return;
    for (const auto& entry : entries) {
        if (entry.isDir || entry.name.rfind("._", 0) == 0 ||
            !hasPackageSuffix(entry.name))
            continue;
        char path[384];
        const int n = std::snprintf(path, sizeof path, "%s/%s", directory,
                                    entry.name.c_str());
        if (n <= 0 || n >= int(sizeof path)) {
            ++report.failed;
            continue;
        }
        if (installOne(path)) ++report.installed;
        else ++report.failed;
    }
}

}  // namespace

InstallReport installDroppedPackages() {
    InstallReport report;
    scanDirectory(fs::ROOT, report);
    char coreDir[96];
    std::snprintf(coreDir, sizeof coreDir, "%s/cores", fs::ROOT);
    scanDirectory(coreDir, report);
    return report;
}

}  // namespace rs::corepkg
