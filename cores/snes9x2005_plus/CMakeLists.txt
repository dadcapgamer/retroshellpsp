# Snes9x 2005 Plus uses the same pinned Snes9x 2005 source tree with the
# Blargg APU enabled. RetroArch's PSP buildbot publishes this as a distinct
# core, so RetroSuite does the same for an equal on-hardware comparison.

set(SX ${CMAKE_CURRENT_SOURCE_DIR}/../snes9x2005/upstream)
set(LC ${SX}/libretro-common)

file(GLOB SNES_PLUS_SRC ${SX}/source/*.c)
# dsp*emu.c is included by dsp*.c. The legacy APU is replaced by
# apu_blargg.c when USE_BLARGG_APU is enabled.
list(FILTER SNES_PLUS_SRC EXCLUDE REGEX
  "(apu|soundux|spc700|dsp[1234]emu)\\.c$")

rs_add_core(snes9x2005_plus
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${SNES_PLUS_SRC}
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

target_include_directories(rs_core_snes9x2005_plus PRIVATE
  ${SX}/source ${SX} ${LC}/include)

target_compile_definitions(rs_core_snes9x2005_plus PRIVATE
  RS_CORE_NAME="snes9x2005_plus"
  RS_CORE_SYSTEMS="snes"
  RS_PSP_NATIVE_PIXELS
  RS_PSP_NATIVE_STORAGE_HEIGHT=512
  __LIBRETRO__
  FRONTEND_SUPPORTS_RGB565
  USE_BLARGG_APU
  USE_OLD_COLOUR_OPS
  RIGHTSHIFT_IS_SEDATE
  INLINE=inline)

target_compile_options(rs_core_snes9x2005_plus PRIVATE
  -O3
  # The upstream PSP path writes directly to RetroArch's GU driver. Keep
  # RetroSuite's PRX isolated and route frames through the libretro callback.
  -UPSP -U__PSP__ -U_PSP
  "SHELL:-include stdint.h"
  "SHELL:-include stdbool.h"
  -Wno-error=incompatible-pointer-types
  -Wno-error=implicit-function-declaration
  -Wno-error=int-conversion
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable)
