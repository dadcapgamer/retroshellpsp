# QuickNES (NES) via the shared libretro shim.
# Pinned upstream source is GPLv2; see upstream/LICENSE.

set(QN ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
set(QN_LR ${QN}/libretro)
set(QN_COMMON ${QN_LR}/libretro-common)

file(GLOB QUICKNES_ENGINE ${QN}/nes_emu/*.cpp)

rs_add_core(quicknes
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${QUICKNES_ENGINE}
    ${QN_LR}/libretro.cpp)

target_include_directories(rs_core_quicknes PRIVATE
  ${QN} ${QN}/nes_emu ${QN_LR} ${QN_COMMON}/include)

target_compile_definitions(rs_core_quicknes PRIVATE
  RS_CORE_NAME="quicknes"
  RS_CORE_SYSTEMS="nes"
  __LIBRETRO__
  NO_UNALIGNED_ACCESS
  HAVE_NO_LANGEXTRA)

target_compile_options(rs_core_quicknes PRIVATE
  -O3 -ffast-math
  # QuickNES's normal PSP target writes directly into VRAM. RetroSuite owns
  # the GU, so select the portable RGB565 callback path instead.
  -UPSP -U__PSP__ -U_PSP
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter)
