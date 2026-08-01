/** rs_libretro_shim — RSCoreAPI on top of a statically linked libretro core.
 *
 * This is the one-time bridge that makes libretro-based emulators cheap to
 * integrate: a core directory vendors its upstream source, compiles this
 * shim alongside it, and gets the whole RetroShell feature set (save
 * states, SRAM autosave, in-game menu, resampled audio) without writing
 * any RSCoreAPI code itself.
 *
 * The shim links against exactly one libretro core (the retro_* symbols),
 * so it is compiled once per core module, never shared as a binary.
 *
 * A core's CMakeLists provides identity via compile definitions:
 *   RS_CORE_NAME     — module/manifest name, e.g. "gambatte"
 *   RS_CORE_SYSTEMS  — pipe-separated SystemInfo::coreId list, e.g. "gb|gbc"
 * Version and ROM extensions are taken from retro_get_system_info at init.
 *
 * Pixel formats: libretro cores emit frames with red in the high bits
 * (RGB565 / 0RGB1555 / XRGB8888); the PSP GE wants red in the low bits.
 * The shim converts every frame into a PSP-native buffer — small cores
 * like Game Boy pay ~23k pixels per frame for it, which is negligible.
 * Cores with PSP-native output paths can skip conversion later via a
 * dedicated flag once one exists.
 */
#pragma once

/* Everything is internal; the module only exports rs_get_core_api (plus
 * module_start/module_stop in PRX builds). This header exists for
 * documentation and to keep core CMakeLists honest about the contract. */
