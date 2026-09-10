#include "src/frontend/core_package_policy.h"

#include <cassert>

int main() {
    using namespace rs::corepkg;
    assert(safeCoreName("gambatte"));
    assert(safeCoreName("snes9x2005-plus"));
    assert(!safeCoreName("../core"));
    assert(!safeCoreName("core/name"));
    assert(disqualifiedCoreName("snes9x2010"));
    assert(disqualifiedCoreName("mgba"));
    assert(disqualifiedCoreName("tempgba"));
    assert(!disqualifiedCoreName("gambatte"));
    assert(installableCoreName("gambatte"));
    assert(installableCoreName("froggba"));
    assert(!installableCoreName("tempgba"));
    assert(!installableCoreName("dummy"));
    assert(!installableCoreName("gearboy"));

    assert(hasPackageSuffix("gambatte-1.0.rscore.zip"));
    assert(hasPackageSuffix("GAMBATTE.RSCORE.ZIP"));
    assert(!hasPackageSuffix("release.zip"));
    assert(!hasPackageSuffix(".rscore.zip"));

    assert(safeArchivePath("RETROSHELL/cores/gambatte.prx"));
    assert(!safeArchivePath("../RETROSHELL/cores/evil.prx"));
    assert(!safeArchivePath("RETROSHELL/../evil.prx"));
    assert(!safeArchivePath("RETROSHELL\\cores\\evil.prx"));
    assert(!safeArchivePath("ms0:/RETROSHELL/cores/evil.prx"));

    assert(directCoreManifestPath("RETROSHELL/cores/gambatte.json"));
    assert(!directCoreManifestPath("RETROSHELL/cores/nested/core.json"));
    assert(!directCoreManifestPath("RETROSHELL/cores/../core.json"));

    assert(safeNativeExecutable("froggba",
                                "emulators/froggba/EBOOT.PBP"));
    assert(!safeNativeExecutable("froggba",
                                 "emulators/other/EBOOT.PBP"));
    assert(!safeNativeExecutable("froggba", "../EBOOT.PBP"));
    assert(nativePayloadPath("RETROSHELL/emulators/froggba/EBOOT.PBP",
                             "froggba"));
    assert(nativePayloadPath("RETROSHELL/emulators/froggba/ku_bridge.prx",
                             "froggba"));
    assert(!nativePayloadPath("RETROSHELL/emulators/other/EBOOT.PBP",
                              "froggba"));
    assert(safeSystemFile("system/gba_bios.bin"));
    assert(!safeSystemFile("system/nested/gba_bios.bin"));
    assert(safeNativeSupportFile("froggba",
        "emulators/froggba/gba_bios.bin"));
    assert(!safeNativeSupportFile("froggba",
        "emulators/other/gba_bios.bin"));
    return 0;
}
