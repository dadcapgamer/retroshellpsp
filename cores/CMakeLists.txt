# Emulator cores. Each subdirectory produces one core via rs_add_core().
# The dummy core validates the Core API and the PRX/static loading
# pipeline; real emulator cores land in Phases 4+ (see docs/ROADMAP notes
# in README.md).

add_subdirectory(dummy)
add_subdirectory(gambatte)
add_subdirectory(gearboy)
add_subdirectory(tgbdual)
add_subdirectory(fceumm)
add_subdirectory(snes9x2005)
add_subdirectory(picodrive)

rs_finalize_static_cores()
