/** Scripted self-test (debug builds only, -DRS_AUTOPILOT=ON).
 *
 * Injects synthetic input on a fixed frame schedule and dumps framebuffer
 * PNGs to ms0:/RETROSHELL/shots/, letting the UI be exercised and visually
 * verified without a human at the controls (e.g. driving PPSSPP from CI or
 * an agent). Compiled out of release builds.
 */
#pragma once

namespace rs {
class App;
namespace autopilot {
void tick(App& app);
}
}  // namespace rs
