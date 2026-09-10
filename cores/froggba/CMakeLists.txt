# FrogGBA is a PSP-native gpSP/TempGBA-lineage compatibility alternate.
# This adapter retains FrogGBA's balanced PSP cache/audio/rendering profile,
# while routing video, audio, input, saves and lifecycle through RetroShell.
set(FG ${CMAKE_CURRENT_SOURCE_DIR}/upstream)

rs_add_core(froggba
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${FG}/libretro.c
    ${FG}/input.c
    ${FG}/main.c
    ${FG}/memory.c
    ${FG}/sound_alt.c
    ${FG}/video.c
    ${FG}/zip.c
    ${FG}/cpu.c
    ${FG}/mips_stub.S
  LIBS z m)

target_include_directories(rs_core_froggba PRIVATE ${FG})
target_compile_definitions(rs_core_froggba PRIVATE
  RS_CORE_NAME="froggba"
  RS_CORE_SYSTEMS="gba"
  RS_CORE_EXTENSIONS="gba|bin|agb|gbz|zip"
  RS_CORE_VERSION="0.4.0-rc.1-9d75721-rs1"
  RS_PSP_NATIVE_BGR555
  RS_PSP_NATIVE_STORAGE_HEIGHT=256
  RS_RETROSHELL
  RS_FROGGBA
  __LIBRETRO__
  PSP
  )

target_compile_options(rs_core_froggba PRIVATE
  -O3 -G0
  -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi
  -mllsc -mbranch-likely -mno-check-zero-division
  -fomit-frame-pointer
  -fno-split-wide-types -fgcse-after-reload
  -fno-unwind-tables -fno-asynchronous-unwind-tables
  -falign-functions=32 -falign-loops -falign-labels -falign-jumps
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-function
  -Wno-missing-field-initializers)
target_compile_options(rs_core_froggba PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-fcommon>)

# The dynarec is sensitive to aggressive optimisation in cpu.c.
set_source_files_properties(${FG}/cpu.c PROPERTIES COMPILE_OPTIONS "-O2")
