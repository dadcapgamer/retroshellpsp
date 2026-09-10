# mGBA is the compatibility-oriented GB/GBC/GBA alternate. Keep the upstream
# libretro build deliberately minimal: no desktop frontends, debugger,
# archive libraries, scripting, or desktop frontends in this PSP package.
set(BINARY_NAME rs_mgba_upstream)
set(BUILD_LIBRETRO ON CACHE BOOL "" FORCE)
set(LIBRETRO_STATIC ON CACHE BOOL "" FORCE)
set(SKIP_LIBRARY ON CACHE BOOL "" FORCE)
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(M_CORE_GB ON CACHE BOOL "" FORCE)
set(DISABLE_DEPS ON CACHE BOOL "" FORCE)
set(ENABLE_DEBUGGERS OFF CACHE BOOL "" FORCE)
set(ENABLE_GDB_STUB OFF CACHE BOOL "" FORCE)
set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP OFF CACHE BOOL "" FORCE)
set(USE_LZMA OFF CACHE BOOL "" FORCE)
set(USE_PNG OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
set(BUILD_QT OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SUITE OFF CACHE BOOL "" FORCE)
set(BUILD_HEADLESS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)

add_subdirectory(upstream EXCLUDE_FROM_ALL)

rs_add_core(mgba
  SOURCES
    ${CMAKE_SOURCE_DIR}/cores/shared/rs_libretro_shim.cpp
    ${CMAKE_SOURCE_DIR}/cores/shared/psp_posix_stubs.c
  LIBS
    rs_mgba_upstream_libretro
    m
)

target_include_directories(rs_core_mgba PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/src/platform/libretro
)
target_compile_definitions(rs_core_mgba PRIVATE
  RS_CORE_NAME="mgba"
  RS_CORE_SYSTEMS="gb|gbc|gba"
  RS_CORE_EXTENSIONS="gb|gbc|gba|bin"
  RS_CORE_VERSION="0.11-compat"
  RS_PSP_NATIVE_RGB565=1
  RS_MGBA_PSP_DEFAULTS=1
)
target_compile_options(rs_core_mgba PRIVATE
  -O3 -G0 -march=allegrex -ffast-math
)
