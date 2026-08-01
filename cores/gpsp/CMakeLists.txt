# gpSP (Game Boy Advance) via the shared libretro shim.
#
# This is the upstream PSP/MIPS dynarec build with a deliberately bounded
# 8 MiB ROM page cache. Larger uncompressed ROMs are paged through the
# frontend-owned VFS instead of being duplicated in the PSP-1000 arena.
# Candidate-only until it passes the real PSP-1000 memory/performance gate.

set(GP ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(LC ${GP}/libretro/libretro-common)

rs_add_core(gpsp
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${GP}/main.c
    ${GP}/gba_memory.c
    ${GP}/savestate.c
    ${GP}/input.c
    ${GP}/sound.c
    ${GP}/cheats.c
    ${GP}/memmap.c
    ${GP}/serial.c
    ${GP}/gbp.c
    ${GP}/rfu.c
    ${GP}/serial_proto.c
    ${GP}/libretro/libretro.c
    ${GP}/gba_cc_lut.c
    ${GP}/cpu_threaded.c
    ${GP}/video.cc
    ${GP}/cpu.cc
    ${GP}/bios_data.S
    ${GP}/mips/mips_stub.S
    ${LC}/compat/compat_posix_string.c
    ${LC}/compat/compat_strl.c
    ${LC}/compat/fopen_utf8.c
    ${LC}/encodings/encoding_utf.c
    ${LC}/file/file_path.c
    ${LC}/file/file_path_io.c
    ${LC}/streams/file_stream.c
    ${LC}/string/stdstring.c
    ${LC}/time/rtime.c
    ${LC}/vfs/vfs_implementation.c)

target_include_directories(rs_core_gpsp PRIVATE
  ${GP} ${GP}/libretro ${LC}/include)

target_compile_definitions(rs_core_gpsp PRIVATE
  RS_CORE_NAME="gpsp"
  RS_CORE_SYSTEMS="gba"
  RS_GPSP_PSP_DEFAULTS
  GIT_VERSION="5b6e751"
  PSP
  MIPS_HAS_R2_INSTS
  SMALL_TRANSLATION_CACHE
  ROM_BUFFER_SIZE=8
  HAVE_DYNAREC
  MIPS_ARCH
  HAVE_STRINGS_H
  HAVE_STDINT_H
  HAVE_INTTYPES_H
  __LIBRETRO__
  INLINE=inline
  FRONTEND_SUPPORTS_RGB565)

target_compile_options(rs_core_gpsp PRIVATE
  -O3
  -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi
  -fomit-frame-pointer -ffast-math
  -falign-functions=32 -falign-loops -falign-labels -falign-jumps
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable
  -Wno-unused-function -Wno-missing-field-initializers)

# bios_data.S uses an upstream-relative .incbin path.
set_source_files_properties(${GP}/bios_data.S PROPERTIES
  COMPILE_OPTIONS "-Wa,-I${GP}")
