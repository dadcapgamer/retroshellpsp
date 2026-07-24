# Emulator cores. Each subdirectory produces one core via rs_add_core().
# The dummy core validates the Core API and the PRX/static loading
# pipeline; real emulator cores land in Phases 4+ (see docs/ROADMAP notes
# in README.md).

add_subdirectory(dummy)

rs_finalize_static_cores()
