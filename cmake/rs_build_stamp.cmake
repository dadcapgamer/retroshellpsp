# Writes a build-stamp header, regenerated on every build.
#
# The stamp exists so a console can be matched to the artifact running on it.
# That only works if it is accurate: computing it with string(TIMESTAMP) at
# CMake *configure* time meant an incremental rebuild kept the previous value,
# so two different EBOOTs could claim the same stamp and a PSP could report a
# time from long before the binary it was running. A stale stamp is worse than
# none, because it invites testing the wrong build with confidence.
#
# Run via `cmake -P`, so this script sees only what is passed with -D.
string(TIMESTAMP RS_BUILD_STAMP "%Y-%m-%d %H:%M" UTC)
set(CONTENT "/* Generated every build by cmake/rs_build_stamp.cmake. */\n")
string(APPEND CONTENT "#pragma once\n")
string(APPEND CONTENT "#define RS_BUILD_STAMP \"${RS_BUILD_STAMP}\"\n")

# Only rewrite when the value changed, so an unchanged stamp within the same
# minute does not force a needless recompile of everything that includes it.
set(EXISTING "")
if(EXISTS "${OUT_FILE}")
  file(READ "${OUT_FILE}" EXISTING)
endif()
if(NOT EXISTING STREQUAL CONTENT)
  file(WRITE "${OUT_FILE}" "${CONTENT}")
endif()
