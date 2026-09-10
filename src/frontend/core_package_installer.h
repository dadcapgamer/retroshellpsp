/** Startup installer for unopened `.rscore.zip` files copied into
 * ms0:/RETROSHELL or ms0:/RETROSHELL/cores.
 */
#pragma once

namespace rs::corepkg {

struct InstallReport {
    int installed = 0;
    int failed = 0;
};

InstallReport installDroppedPackages();

}  // namespace rs::corepkg
