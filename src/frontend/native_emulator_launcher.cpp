#include "frontend/native_emulator_launcher.h"

#include "frontend/native_adapter_protocol.h"
#include "frontend/core_package_policy.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include <pspiofilemgr.h>
#include <psploadexec.h>
#include <psploadexec_kernel.h>
#include <systemctrl.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rs::nativeemu {

namespace {
/* Where RetroShell's own EBOOT lives, so an adapter can chain-load back to
 * it. docs/INSTALL.md documents the first; the second covers the common
 * habit of renaming the folder in upper case. */
const char* const LAUNCHER_PATHS[] = {
    "ms0:/PSP/GAME/RetroShell/EBOOT.PBP",
    "ms0:/PSP/GAME/RETROSHELL/EBOOT.PBP",
    /* ARK categories keep applications one level below GAME. */
    "ms0:/PSP/GAME/CAT_Emulators/RetroShell/EBOOT.PBP",
    "ms0:/PSP/GAME/CAT_Emulators/RETROSHELL/EBOOT.PBP",
};
constexpr const char* SESSION_DIRECTORY = "ms0:/RETROSHELL/session";
}  // namespace

namespace {
/* The last "result: " line of an adapter's session log, if it wrote one. */
bool adapterResult(const char* adapter, char* out, size_t size) {
    char path[96];
    std::snprintf(path, sizeof path, "%s/%s.log", SESSION_DIRECTORY, adapter);
    std::vector<u8> bytes;
    if (!fs::readFile(path, bytes, 4096)) return false;
    const std::string text(bytes.begin(), bytes.end());
    const size_t at = text.rfind("result: ");
    if (at == std::string::npos) return false;
    const size_t start = at + 8;
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::snprintf(out, size, "%.*s", int(end - start), text.c_str() + start);
    return out[0] != 0;
}
}  // namespace

bool consumeReturnReceipt(char* notice, size_t size) {
    std::vector<u8> bytes;
    if (!fs::readFile(protocol::SESSION_PATH.data(), bytes, 1024)) return false;
    char adapter[49]{};
    char state[17]{};
    bool early = false;
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()),
                                bytes.size());
    if (protocol::parseReceipt(text, adapter, sizeof adapter,
                               state, sizeof state)) {
        RS_LOGI("native: adapter '%s' returned with state '%s'", adapter,
                state);
        if (std::strcmp(state, "returned") != 0) {
            RS_LOGW("native: previous adapter session ended unexpectedly");
            char reason[96];
            if (adapterResult(adapter, reason, sizeof reason)) {
                RS_LOGW("native: %s reported: %s", adapter, reason);
                std::snprintf(notice, size, "%s: %s", adapter, reason);
            } else {
                std::snprintf(notice, size, "%s closed unexpectedly", adapter);
            }
            early = true;
        }
    } else {
        RS_LOGW("native: discarded malformed adapter session receipt");
    }
    fs::removeFile(protocol::SESSION_PATH.data());
    return early;
}

int launch(const CoreInfo& core, const db::GameEntry& game) {
    if (!core.isNative() ||
        !corepkg::safeNativeExecutable(core.name, core.executable))
        return -1;

    char executable[320];
    const int executableChars = std::snprintf(
        executable, sizeof executable, "%s/%s", fs::ROOT,
        core.executable.c_str());
    if (executableChars <= 0 || executableChars >= int(sizeof executable) ||
        !fs::exists(executable))
        return -2;

    /* PSP crt0 turns this NUL-separated block into argc/argv. Keeping the
     * executable as argv[0] is required for emulators that locate assets
     * beside their EBOOT; the selected ROM is argv[1]. */
    alignas(64) char arguments[1024]{};
    if (game.path.size() > core.maxRomPath) return -5;

    /* Native emulators expect their own working directories to exist — a zip
     * cannot carry empty ones, and FrogGBA aborts at startup naming every
     * missing rom/save/state/cheat/snapshot path. Create them before handing
     * the machine over; paths are manifest-declared and constrained to the
     * adapter's folder by corepkg::safeNativeDirectory. */
    for (const std::string& relative : core.requiredDirectories) {
        char directory[320];
        const int chars = std::snprintf(directory, sizeof directory, "%s/%s",
                                        fs::ROOT, relative.c_str());
        if (chars <= 0 || chars >= int(sizeof directory)) return -8;
        if (!fs::mkdirs(directory)) {
            RS_LOGE("native: could not create %s", directory);
            return -8;
        }
    }

    if (core.biosBytes) {
        char source[320], destination[320];
        const int sourceChars = std::snprintf(source, sizeof source, "%s/%s",
                                              fs::ROOT,
                                              core.biosSource.c_str());
        const int destinationChars = std::snprintf(
            destination, sizeof destination, "%s/%s", fs::ROOT,
            core.biosDestination.c_str());
        if (sourceChars <= 0 || sourceChars >= int(sizeof source) ||
            destinationChars <= 0 ||
            destinationChars >= int(sizeof destination))
            return -6;
        if (fs::fileSize(destination) != s32(core.biosBytes)) {
            std::vector<u8> bios;
            if (!fs::readFile(source, bios, core.biosBytes) ||
                bios.size() != core.biosBytes ||
                !fs::writeFileAtomic(destination, bios.data(), core.biosBytes))
                return -7;
            RS_LOGI("native: synchronized %u-byte BIOS into emulator folder",
                    unsigned(core.biosBytes));
        }
    }

    /* argv[2] is RetroShell's own boot path. An adapter patched to honour it
     * chain-loads back here on quit instead of dropping the user at the XMB,
     * which is what makes a process-replacement emulator still feel like part
     * of the shell. Emulators that ignore the extra argument are unaffected.
     * sceKernelInitFileName reports where this process was booted from, so a
     * non-standard install location still returns correctly. */
    /* sceKernelInitFileName would report this exactly, but it lives in
     * libpspkernel, whose kernel-mode stubs collide with newlib in a user
     * module. Probing the documented install location is enough: when it is
     * missing we simply pass no argv[2] and the adapter exits to the XMB as
     * before, which is a graceful degradation rather than a failure. */
    const char* launcher = nullptr;
    for (const char* candidate : LAUNCHER_PATHS) {
        if (fs::exists(candidate)) {
            launcher = candidate;
            break;
        }
    }
    const size_t executableBytes = std::strlen(executable) + 1;
    const size_t romBytes = game.path.size() + 1;
    const size_t launcherBytes =
        (launcher && *launcher) ? std::strlen(launcher) + 1 : 0;
    const size_t sessionBytes =
        (core.adapterProtocol == protocol::VERSION && launcherBytes)
            ? protocol::SESSION_PATH.size() + 1
            : 0;
    if (executableBytes + romBytes + launcherBytes + sessionBytes >
        sizeof arguments)
        return -3;

    if (sessionBytes) {
        char receipt[256];
        if (!fs::mkdirs(SESSION_DIRECTORY) ||
            !protocol::formatReceipt(receipt, sizeof receipt, core.name,
                                     game.pathHash, "launched") ||
            !fs::writeFileAtomic(protocol::SESSION_PATH.data(), receipt,
                                 u32(std::strlen(receipt))))
            return -9;
    }
    std::memcpy(arguments, executable, executableBytes);
    std::memcpy(arguments + executableBytes, game.path.c_str(), romBytes);
    if (launcherBytes)
        std::memcpy(arguments + executableBytes + romBytes, launcher,
                    launcherBytes);
    if (sessionBytes)
        std::memcpy(arguments + executableBytes + romBytes + launcherBytes,
                    protocol::SESSION_PATH.data(), sessionBytes);

    RS_LOGI("native: replacing RetroShell with %s", executable);
    RS_LOGI("native: argv[1]=%s%s", game.path.c_str(),
            game.zipEntry.empty() ? "" : " (archive entry selected by emulator)");
    RS_LOGI("native: argv[2]=%s", launcherBytes ? launcher : "(none)");
    RS_LOGI("native: adapter protocol=%u argv[3]=%s",
            unsigned(core.adapterProtocol),
            sessionBytes ? protocol::SESSION_PATH.data() : "(none)");
    sceIoSync("ms0:", 0);

    /* RetroShell is a user-mode module, and sceKernelLoadExec is privileged:
     * calling it directly returns SCE_KERNEL_ERROR_ILLEGAL_PERM_CALL
     * (0x80020149) on real hardware. Custom firmware exposes the kernel-side
     * loader to user modules through SystemCtrl, which is how PSP homebrew
     * launchers chain-load another EBOOT. Try that first and keep the direct
     * call as a fallback for environments that permit it. */
    SceKernelLoadExecVSHParam vshParams{};
    vshParams.size = sizeof vshParams;
    vshParams.args = SceSize(executableBytes + romBytes + launcherBytes +
                             sessionBytes);
    vshParams.argp = arguments;
    vshParams.key = "game";

    const int cfwResult = sctrlKernelLoadExecVSHMs2(executable, &vshParams);
    RS_LOGW("native: SystemCtrl loader returned %08x; trying direct loadexec",
            unsigned(cfwResult));

    SceKernelLoadExecParam params{};
    params.size = sizeof params;
    params.args = SceSize(executableBytes + romBytes + launcherBytes +
                          sessionBytes);
    params.argp = arguments;
    params.key = nullptr;
    const int directResult = sceKernelLoadExec(executable, &params);
    RS_LOGE("native: direct loadexec returned %08x", unsigned(directResult));
    if (sessionBytes) fs::removeFile(protocol::SESSION_PATH.data());
    /* Report the CFW error when both failed: it is the meaningful one. */
    return cfwResult < 0 ? cfwResult : directResult;
}

}  // namespace rs::nativeemu
