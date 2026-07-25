# PicoDrive (Genesis/MegaDrive + Master System + Game Gear + 32X + CD) via
# the libretro shim.
#
# PicoDrive's build is too intricate to restate in CMake (arch-specific DRC
# backends, submodules, conditional sources), so upstream/ vendors a
# prebuilt static lib built with its own Makefile for the psp1 target:
#
#   git clone --recurse-submodules https://github.com/libretro/picodrive
#   cd picodrive && make -f Makefile.libretro platform=psp1
#   cp picodrive_libretro_psp1.a  <here>/upstream/picodrive_libretro.a
#
# That build selects the C 68K core (FAME), CZ80, and the MIPS SH2 dynarec —
# no ARM assembly. GPLv2 (see upstream/COPYING).

set(PD ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(LRC ${PD}/lrc)

# The static lib was built with STATIC_LINKING=1, which leaves the
# libretro-common file/stream/PNG helpers for the frontend to provide.
# Compile them here (built without CHD, so no LZMA/LTO complications) and
# link them alongside the core lib.
rs_add_core(picodrive
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${LRC}/streams/file_stream.c
    ${LRC}/streams/file_stream_transforms.c
    ${LRC}/streams/trans_stream.c
    ${LRC}/streams/trans_stream_zlib.c
    ${LRC}/streams/trans_stream_pipe.c
    ${LRC}/vfs/vfs_implementation.c
    ${LRC}/file/file_path.c
    ${LRC}/file/file_path_io.c
    ${LRC}/string/stdstring.c
    ${LRC}/compat/compat_strcasestr.c
    ${LRC}/compat/compat_strl.c
    ${LRC}/compat/fopen_utf8.c
    ${LRC}/compat/compat_posix_string.c
    ${LRC}/encodings/encoding_utf.c
    ${LRC}/time/rtime.c
    ${LRC}/formats/png/rpng.c
    ../shared/psp_posix_stubs.c
  LIBS ${PD}/picodrive_libretro.a)

target_include_directories(rs_core_picodrive PRIVATE ${PD}/lrc/include)

target_compile_definitions(rs_core_picodrive PRIVATE
  RS_CORE_NAME="picodrive"
  RS_CORE_SYSTEMS="md|sms|gg"
  __LIBRETRO__)
