# Snes9x 2010 compatibility core. This is kept as a narrowly justified
# alternate for SA-1 titles that the faster Snes9x 2005 core cannot render.
# The PSP build removes desktop-only HD/NTSC presentation features, caps
# the internal ROM buffer at 4 MiB, and streams the ROM into that buffer
# through the host VFS so the PSP-1000 never holds two complete copies.
set(SX2010 ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(SX2010_BUILD ${CMAKE_CURRENT_BINARY_DIR}/upstream-build)
set(SX2010_ARCHIVE ${SX2010_BUILD}/snes9x2010_libretro_psp1.a)

file(GLOB_RECURSE SX2010_UPSTREAM_FILES CONFIGURE_DEPENDS
     ${SX2010}/*.c ${SX2010}/*.h
     ${SX2010}/Makefile.libretro ${SX2010}/build/Makefile.common)
find_program(RS_MAKE_EXECUTABLE make REQUIRED)

add_custom_command(
  OUTPUT ${SX2010_ARCHIVE}
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${SX2010_BUILD}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SX2010} ${SX2010_BUILD}
  COMMAND ${CMAKE_COMMAND} -E chdir ${SX2010_BUILD}
          ${RS_MAKE_EXECUTABLE} -f Makefile.libretro
          platform=psp1 GIT_VERSION=da06427 -j2
  DEPENDS ${SX2010_UPSTREAM_FILES}
  COMMENT "Building pinned Snes9x 2010 da064275 for PSP-1000"
  VERBATIM)
add_custom_target(rs_snes9x2010_upstream DEPENDS ${SX2010_ARCHIVE})

rs_add_core(snes9x2010
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${SX2010}/libretro/libretro-common/compat/compat_posix_string.c
    ${SX2010}/libretro/libretro-common/compat/compat_strcasestr.c
    ${SX2010}/libretro/libretro-common/compat/compat_strl.c
    ${SX2010}/libretro/libretro-common/compat/fopen_utf8.c
    ${SX2010}/libretro/libretro-common/encodings/encoding_utf.c
    ${SX2010}/libretro/libretro-common/file/file_path.c
    ${SX2010}/libretro/libretro-common/file/file_path_io.c
    ${SX2010}/libretro/libretro-common/memmap/memalign.c
    ${SX2010}/libretro/libretro-common/streams/file_stream.c
    ${SX2010}/libretro/libretro-common/streams/file_stream_transforms.c
    ${SX2010}/libretro/libretro-common/string/stdstring.c
    ${SX2010}/libretro/libretro-common/time/rtime.c
    ${SX2010}/libretro/libretro-common/vfs/vfs_implementation.c
  LIBS ${SX2010_ARCHIVE})

add_dependencies(rs_core_snes9x2010 rs_snes9x2010_upstream)

target_include_directories(rs_core_snes9x2010 PRIVATE
  ${SX2010}/libretro
  ${SX2010}/libretro/libretro-common/include)

target_compile_definitions(rs_core_snes9x2010 PRIVATE
  RS_CORE_NAME="snes9x2010"
  RS_CORE_SYSTEMS="snes")
