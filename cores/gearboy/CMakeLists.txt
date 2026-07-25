# Gearboy (Game Boy / Game Boy Color) via the libretro shim.
#
# A lighter, faster GB/GBC core than Gambatte — the intended pick for
# demanding action games that push Gambatte past the PSP's frame budget.
# Selectable per game through the core picker.
#
# upstream/ is a pruned copy of https://github.com/drhelius/Gearboy
# (GPLv3 — see upstream/LICENSE). Source list mirrors the libretro
# Makefile.common; PSP enables Gearboy's own PERFORMANCE fast paths.

set(GB  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/src)
set(LR  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/libretro)
set(MZ  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/deps/miniz)

set(GEARBOY_SOURCES
  ../shared/rs_libretro_shim.cpp
  ${LR}/libretro.cpp
  ${GB}/GearboyCore.cpp
  ${GB}/Memory.cpp
  ${GB}/Processor.cpp
  ${GB}/Video.cpp
  ${GB}/Audio.cpp
  ${GB}/Input.cpp
  ${GB}/Cartridge.cpp
  ${GB}/MemoryRule.cpp
  ${GB}/CommonMemoryRule.cpp
  ${GB}/IORegistersMemoryRule.cpp
  ${GB}/RomOnlyMemoryRule.cpp
  ${GB}/MBC1MemoryRule.cpp
  ${GB}/MBC2MemoryRule.cpp
  ${GB}/MBC3MemoryRule.cpp
  ${GB}/MBC5MemoryRule.cpp
  ${GB}/MultiMBC1MemoryRule.cpp
  ${GB}/HuC1MemoryRule.cpp
  ${GB}/HuC3MemoryRule.cpp
  ${GB}/MMM01MemoryRule.cpp
  ${GB}/CameraMemoryRule.cpp
  ${GB}/MBC7MemoryRule.cpp
  ${GB}/TAMA5MemoryRule.cpp
  ${GB}/WisdomTreeMemoryRule.cpp
  ${GB}/M161MemoryRule.cpp
  ${GB}/SachenMMC1MemoryRule.cpp
  ${GB}/SachenMMC2MemoryRule.cpp
  ${GB}/FlashcartMemoryRule.cpp
  ${GB}/opcodes.cpp
  ${GB}/opcodes_cb.cpp
  ${GB}/VgmRecorder.cpp
  ${GB}/TraceLogger.cpp
  ${GB}/SGB.cpp
  ${GB}/audio/Blip_Buffer.cpp
  ${GB}/audio/Effects_Buffer.cpp
  ${GB}/audio/Gb_Apu.cpp
  ${GB}/audio/Gb_Apu_State.cpp
  ${GB}/audio/Gb_Oscs.cpp
  ${GB}/audio/Multi_Buffer.cpp)

# Gearboy bundles miniz for loading ROMs from .zip in memory. RetroSuite
# extracts ZIPs itself and hands the core raw ROM data, so this path never
# runs — but the symbols must resolve. In a PRX build the core is a
# standalone module, so it links its own miniz; in a static build the
# frontend's external/miniz already provides those symbols, so adding
# Gearboy's would double-define them.
if(NOT RS_STATIC_CORES)
  list(APPEND GEARBOY_SOURCES ${MZ}/miniz.c)
endif()

rs_add_core(gearboy SOURCES ${GEARBOY_SOURCES})

target_include_directories(rs_core_gearboy PRIVATE ${GB} ${LR} ${MZ})

target_compile_definitions(rs_core_gearboy PRIVATE
  RS_CORE_NAME="gearboy"
  RS_CORE_SYSTEMS="gb|gbc"
  EMULATOR_BUILD="3.8"
  __LIBRETRO__
  PSP                          # enables Gearboy's PERFORMANCE fast paths
  GEARBOY_DISABLE_DISASSEMBLER # drops a 32MB per-ROM-byte disassembler map
  MINIZ_NO_TIME)

# -O3 on the hot path; quiet vendored-only warnings so they don't bury the
# shim's. Upstream (GPLv3) is not ours to fix.
target_compile_options(rs_core_gearboy PRIVATE
  -O3
  -Wno-sign-compare -Wno-unused-variable
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>)
