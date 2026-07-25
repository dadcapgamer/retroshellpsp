# TGB Dual (Game Boy / Game Boy Color) via the libretro shim.
#
# A lighter, speed-oriented GB/GBC core — a third accuracy/speed option
# alongside Gambatte and Gearboy, selectable per game.
#
# upstream/ is a pruned copy of https://github.com/libretro/tgbdual-libretro
# (GPLv2 — see upstream/README). Source list mirrors its Makefile.common.

set(TD ${CMAKE_CURRENT_SOURCE_DIR}/upstream)

rs_add_core(tgbdual
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${TD}/gb_core/apu.cpp
    ${TD}/gb_core/cheat.cpp
    ${TD}/gb_core/cpu.cpp
    ${TD}/gb_core/gb.cpp
    ${TD}/gb_core/lcd.cpp
    ${TD}/gb_core/mbc.cpp
    ${TD}/gb_core/rom.cpp
    ${TD}/libretro/dmy_renderer.cpp
    ${TD}/libretro/libretro.cpp)

target_include_directories(rs_core_tgbdual PRIVATE
  ${TD} ${TD}/gb_core ${TD}/libretro)

target_compile_definitions(rs_core_tgbdual PRIVATE
  RS_CORE_NAME="tgbdual"
  RS_CORE_SYSTEMS="gb|gbc"
  GIT_VERSION=""
  FRONTEND_SUPPORTS_RGB565
  __LIBRETRO__
  PSP)

target_compile_options(rs_core_tgbdual PRIVATE
  -O3
  -Wno-sign-compare -Wno-unused-variable
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>)
