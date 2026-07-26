# Emulator cores. Each subdirectory produces one core via rs_add_core().
# The dummy core validates the Core API and PRX loading
# pipeline; real emulator cores land in Phases 4+ (see docs/ROADMAP notes
# in README.md).

if(RS_INCLUDE_TEST_CORES)
  add_subdirectory(dummy)
endif()
add_subdirectory(gambatte)
add_subdirectory(gearboy)
add_subdirectory(tgbdual)
add_subdirectory(fceumm)
add_subdirectory(quicknes)
add_subdirectory(snes9x2005)
add_subdirectory(snes9x2005_plus)
add_subdirectory(snes9x2010)
add_subdirectory(picodrive)
add_subdirectory(smsplus)
