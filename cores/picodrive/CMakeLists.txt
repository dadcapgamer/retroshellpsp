# PicoDrive (Genesis/MegaDrive + Master System + Game Gear + 32X + CD) via
# the libretro shim.
#
# PicoDrive is built from the pinned, pruned upstream source in this tree.
# Its Makefile selects FAME, CZ80, and the MIPS SH2 dynarec for psp1.
# LOW_MEMORY reduces caches for the PSP-1000; libchdr is disabled because
# RetroSuite does not advertise Sega CD support. GPLv2 (upstream/COPYING).

set(PD ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(LRC ${PD}/platform/libretro/libretro-common)
set(PD_BUILD ${CMAKE_CURRENT_BINARY_DIR}/upstream-build)
set(PD_ARCHIVE ${PD_BUILD}/picodrive_libretro_psp1.a)

file(GLOB_RECURSE PD_UPSTREAM_FILES CONFIGURE_DEPENDS
     ${PD}/*.c ${PD}/*.h ${PD}/*.S ${PD}/*.s
     ${PD}/Makefile ${PD}/Makefile.libretro)
find_program(RS_MAKE_EXECUTABLE make REQUIRED)

add_custom_command(
  OUTPUT ${PD_ARCHIVE}
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${PD_BUILD}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PD} ${PD_BUILD}
  COMMAND ${CMAKE_COMMAND} -E chdir ${PD_BUILD}
          ${RS_MAKE_EXECUTABLE} -f Makefile.libretro
          platform=psp1 LOW_MEMORY=1 use_libchdr=0 PLATFORM_TREMOR=1
          GIT_REVISION=-78a662e -j2
  DEPENDS ${PD_UPSTREAM_FILES}
  COMMENT "Building pinned PicoDrive 78a662e for PSP-1000"
  VERBATIM)
add_custom_target(rs_picodrive_upstream DEPENDS ${PD_ARCHIVE})

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
  LIBS ${PD_ARCHIVE})

add_dependencies(rs_core_picodrive rs_picodrive_upstream)

target_include_directories(rs_core_picodrive PRIVATE
  ${LRC}/include
  ${LRC})

target_compile_definitions(rs_core_picodrive PRIVATE
  RS_CORE_NAME="picodrive"
  RS_CORE_SYSTEMS="md|sms|gg"
  RS_PSP_NATIVE_RGB565
  RS_PICODRIVE_PSP_DEFAULTS
  __LIBRETRO__)
