cmake_minimum_required(VERSION 3.16)
project(RetroShell VERSION 1.0.0 LANGUAGES C CXX ASM)

# RetroShell PSP — all-in-one retro emulator frontend for Sony PSP.
# Build with the pspdev toolchain wrapper:
#   psp-cmake -S . -B build && cmake --build build
# Output: build/src/EBOOT.PBP + build/cores/*.prx

if(NOT PSP)
  message(FATAL_ERROR
    "This project targets the Sony PSP. Configure with psp-cmake:\n"
    "  psp-cmake -S . -B build && cmake --build build")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RS_DEBUG_OVERLAY "Compile in the FPS / frame-time debug overlay (toggled with SELECT)" ON)
option(RS_AUTOPILOT "Scripted self-test: inject input and dump screenshots (debug only)" OFF)
set(RS_AUTOPILOT_SYSTEM_INDEX "2" CACHE STRING
    "System index for the scripted self-test (0=GB, 1=GBC, 2=GBA, 3=NES, 4=SNES, 5=MD, 6=SMS, 7=GG, 8=PCE)")
option(RS_AUTOPILOT_TOUR "With RS_AUTOPILOT: walk every Home layer with seeded recents instead of launching a game (UI review)" OFF)
set(RS_AUTOPILOT_SETUP "0" CACHE STRING
    "With RS_AUTOPILOT + RS_AUTOPILOT_TOUR: capture first-run setup instead (1 = with games, 2 = empty library)")
option(RS_SHIM_DIAG "Log one-shot libretro video diagnostics (debug only)" OFF)
option(RS_FAILURE_INJECTION "Enable launch-stage failures selected by per-game injectFailure option" OFF)
option(RS_INCLUDE_TEST_CORES "Build/package test-only cores such as dummy" OFF)

if(DEFINED RS_STATIC_CORES AND RS_STATIC_CORES)
  message(FATAL_ERROR
    "RS_STATIC_CORES is disabled: libretro cores export colliding retro_* "
    "symbols and cannot safely coexist. Production uses loadable PRX cores.")
endif()

# -G0 keeps all data out of the gp-relative small-data section, which is
# required for code that is shared between the EBOOT and relocatable PRX cores.
add_compile_options(-O2 -G0 -Wall)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
                    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

if(RS_DEBUG_OVERLAY)
  add_compile_definitions(RS_DEBUG_OVERLAY=1)
endif()
if(RS_AUTOPILOT)
  if(NOT RS_AUTOPILOT_SYSTEM_INDEX MATCHES "^[0-8]$")
    message(FATAL_ERROR "RS_AUTOPILOT_SYSTEM_INDEX must be an integer from 0 through 8")
  endif()
  add_compile_definitions(RS_AUTOPILOT=1)
  if(RS_AUTOPILOT_TOUR)
    add_compile_definitions(RS_AUTOPILOT_TOUR=1)
    if(NOT RS_AUTOPILOT_SETUP STREQUAL "0")
      add_compile_definitions(RS_AUTOPILOT_SETUP=${RS_AUTOPILOT_SETUP})
    endif()
  endif()
  add_compile_definitions(
    RS_AUTOPILOT_SYSTEM_INDEX=${RS_AUTOPILOT_SYSTEM_INDEX})
endif()
if(RS_SHIM_DIAG)
  add_compile_definitions(RS_SHIM_DIAG=1)
endif()
if(RS_FAILURE_INJECTION)
  add_compile_definitions(RS_FAILURE_INJECTION=1)
endif()

# Stamp the release version into the binary so a running PSP can be matched
# to the build that produced it. Installing a mismatched EBOOT and core set is
# an easy mistake to make by hand and produced a confusing API-version error.
file(STRINGS ${CMAKE_SOURCE_DIR}/RELEASE_VERSION RS_RELEASE_VERSION LIMIT_COUNT 1)
add_compile_definitions(RS_RELEASE_VERSION="${RS_RELEASE_VERSION}")
# RS_BUILD_STAMP comes from a header regenerated on every build; see
# cmake/rs_build_stamp.cmake for why it cannot be computed here.

include(cmake/rs_assets.cmake)
include(cmake/rs_add_core.cmake)

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(cores)
