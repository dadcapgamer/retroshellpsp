# Beetle PCE Fast (PC Engine / TurboGrafx-16) via the shared libretro shim.
#
# Build the pinned upstream PSP archive without CHD. Tremor remains because
# upstream's shared CD image reader references its callbacks even for HuCards;
# the heavy CHD/LZMA/Zstd dependency tree is excluded from this candidate.

set(PF ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(LRC ${PF}/libretro-common)
set(PF_BUILD ${CMAKE_CURRENT_BINARY_DIR}/upstream-build)
set(PF_ARCHIVE ${PF_BUILD}/mednafen_pce_fast_libretro_psp1.a)

file(GLOB_RECURSE PF_UPSTREAM_FILES CONFIGURE_DEPENDS
     ${PF}/*.c ${PF}/*.h ${PF}/Makefile ${PF}/Makefile.common)
find_program(RS_MAKE_EXECUTABLE make REQUIRED)

add_custom_command(
  OUTPUT ${PF_ARCHIVE}
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${PF_BUILD}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PF} ${PF_BUILD}
  COMMAND ${CMAKE_COMMAND} -E chdir ${PF_BUILD}
          ${RS_MAKE_EXECUTABLE} platform=psp1 NEED_TREMOR=1 HAVE_CHD=0 -j2
  DEPENDS ${PF_UPSTREAM_FILES}
  COMMENT "Building pinned Beetle PCE Fast 9e65100 for PSP HuCards"
  VERBATIM)
add_custom_target(rs_pcefast_upstream DEPENDS ${PF_ARCHIVE})

# STATIC_LINKING leaves these libretro-common services for the frontend
# module to provide.
rs_add_core(pcefast
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${LRC}/streams/file_stream.c
    ${LRC}/streams/file_stream_transforms.c
    ${LRC}/file/file_path.c
    ${LRC}/file/retro_dirent.c
    ${LRC}/lists/string_list.c
    ${LRC}/lists/dir_list.c
    ${LRC}/compat/compat_strl.c
    ${LRC}/compat/compat_snprintf.c
    ${LRC}/compat/compat_posix_string.c
    ${LRC}/compat/compat_strcasestr.c
    ${LRC}/compat/fopen_utf8.c
    ${LRC}/encodings/encoding_utf.c
    ${LRC}/encodings/encoding_crc32.c
    ${LRC}/memmap/memalign.c
    ${LRC}/string/stdstring.c
    ${LRC}/time/rtime.c
    ${LRC}/vfs/vfs_implementation.c
    ../shared/psp_posix_stubs.c
  LIBS ${PF_ARCHIVE})

add_dependencies(rs_core_pcefast rs_pcefast_upstream)

target_include_directories(rs_core_pcefast PRIVATE
  ${PF}
  ${PF}/mednafen
  ${LRC}/include
  ${LRC})

target_compile_definitions(rs_core_pcefast PRIVATE
  RS_CORE_NAME="pcefast"
  RS_CORE_SYSTEMS="pce"
  RS_PSP_NATIVE_RGB565
  __LIBRETRO__)

target_compile_options(rs_core_pcefast PRIVATE
  -O2 -G0 -fomit-frame-pointer
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-function
  -Wno-missing-field-initializers)
