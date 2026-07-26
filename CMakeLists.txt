cmake_minimum_required(VERSION 3.16)
project(RetroSuite VERSION 0.1.0 LANGUAGES C CXX)

# RetroSuite PSP — all-in-one retro emulator frontend for Sony PSP.
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
option(RS_AUTOPILOT_SNES "Make the scripted self-test target the SNES core" OFF)
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
if(RS_INCLUDE_TEST_CORES)
  add_compile_definitions(RS_INCLUDE_TEST_CORES=1)
endif()
if(RS_AUTOPILOT)
  add_compile_definitions(RS_AUTOPILOT=1)
endif()
if(RS_AUTOPILOT_SNES)
  if(NOT RS_AUTOPILOT)
    message(FATAL_ERROR "RS_AUTOPILOT_SNES requires RS_AUTOPILOT=ON")
  endif()
  add_compile_definitions(RS_AUTOPILOT_SNES=1)
endif()
if(RS_SHIM_DIAG)
  add_compile_definitions(RS_SHIM_DIAG=1)
endif()
if(RS_FAILURE_INJECTION)
  add_compile_definitions(RS_FAILURE_INJECTION=1)
endif()

include(cmake/rs_assets.cmake)
include(cmake/rs_add_core.cmake)

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(cores)
