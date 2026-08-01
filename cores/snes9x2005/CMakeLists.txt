# Snes9x2005 (SNES) via the libretro shim.
#
# upstream/ is a pruned copy of https://github.com/libretro/snes9x2005
# (non-commercial Snes9x license — see upstream/copyright). Heavy games may
# need frameskip on the PSP; that's expected for SNES on this hardware.

set(SX ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(LC ${SX}/libretro-common)

file(GLOB SNES_SRC ${SX}/source/*.c)
# Exclude files that are #included into another TU (dsp1/2/3/4emu.c → dsp1.c)
# and the unused Blargg APU (USE_BLARGG_APU=0 → apu.c/soundux.c/spc700.c).
list(FILTER SNES_SRC EXCLUDE REGEX "(apu_blargg|dsp[1234]emu)\\.c$")

rs_add_core(snes9x2005
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${SNES_SRC}
    ${SX}/libretro.c
    ${LC}/streams/file_stream.c
    ${LC}/streams/file_stream_transforms.c
    ${LC}/vfs/vfs_implementation.c
    ${LC}/file/file_path.c
    ${LC}/file/file_path_io.c
    ${LC}/encodings/encoding_utf.c
    ${LC}/compat/compat_strl.c
    ${LC}/compat/compat_posix_string.c
    ${LC}/compat/fopen_utf8.c
    ${LC}/time/rtime.c
    ${LC}/string/stdstring.c
    ../shared/psp_posix_stubs.c)

target_include_directories(rs_core_snes9x2005 PRIVATE
  ${SX}/source ${SX} ${LC}/include)

# No PSP (its libretro.c renders straight to the GU); route through video_cb.
target_compile_definitions(rs_core_snes9x2005 PRIVATE
  RS_CORE_NAME="snes9x2005"
  RS_CORE_SYSTEMS="snes"
  # Generate the upstream PSP BGR555 layout, but keep PSP itself undefined:
  # defining PSP would let libretro.c issue GU commands behind the host.
  # The shim borrows this buffer as GU_PSM_5551 without a conversion copy.
  RS_PSP_NATIVE_PIXELS
  RS_PSP_NATIVE_STORAGE_HEIGHT=512
  RS_PSP_BALANCED_AUDIO
  RS_PSP_SECRET_OF_MANA_HIRES_RESOLVE
  __LIBRETRO__
  FRONTEND_SUPPORTS_RGB565
  # Required by upstream's BGR555 path: the newer colour helpers assume
  # red occupies the high bits, while BGR555 intentionally puts it low.
  USE_OLD_COLOUR_OPS
  # Upstream enables this for every release build. It exits the specialised
  # CPU loop as soon as the PPU finishes a frame instead of waiting for a
  # later scan-key event.
  LAGFIX
  NDEBUG
  RIGHTSHIFT_IS_SEDATE
  INLINE=inline)

target_compile_options(rs_core_snes9x2005 PRIVATE
  -O3
  # Match upstream's PSP/Allegrex release target. The generic PSP CMake
  # defaults omit these on PRX targets, leaving avoidable ABI/PIC overhead in
  # the core's instruction-heavy CPU, APU, and tile loops.
  -march=allegrex
  -mno-abicalls
  -fno-pic
  -fomit-frame-pointer
  -ffunction-sections
  -fdata-sections
  # Safe parts of Snes9xTYL's PSP compiler profile. The TYL GU renderer,
  # Media Engine ownership, and whole-program mode are deliberately excluded:
  # RetroSuite owns the GU and must be able to unload this PRX cleanly.
  -fsingle-precision-constant
  -fpredictive-commoning
  -frename-registers
  -fno-gcse
  -finline-functions
  -finline-limit=20000
  -falign-functions=64
  -falign-loops=64
  -fno-align-jumps
  -fno-align-labels
  -UPSP -U__PSP__ -U_PSP
  # The core gated its stdint/stdbool includes behind the platform macros
  # we just cancelled; force them so the fixed-width types resolve. SHELL:
  # keeps CMake from de-duplicating the repeated -include into one arg.
  "SHELL:-include stdint.h"
  "SHELL:-include stdbool.h"
  # GCC 15 promotes these old-C-isms to hard errors; this vendored core
  # predates that, so keep them warnings.
  -Wno-error=incompatible-pointer-types
  -Wno-error=implicit-function-declaration
  -Wno-error=int-conversion
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable)

target_link_options(rs_core_snes9x2005 PRIVATE
  -Wl,--gc-sections)
