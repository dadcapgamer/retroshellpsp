# rs_add_core(<name> SOURCES <src>... [LIBS <lib>...])
#
# Builds an emulator core against the RetroSuite Core API, either as a
# loadable PRX module (default) or as a static library registered with the
# frontend's StaticCoreLoader when -DRS_STATIC_CORES=ON.
#
# Fleshed out in Phase 3 alongside the dummy core; the interface is fixed
# here so core directories can be written against it.

function(rs_add_core name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

  if(RS_STATIC_CORES)
    add_library(rs_core_${name} STATIC ${ARG_SOURCES})
  else()
    add_executable(rs_core_${name} ${ARG_SOURCES})
    # PRX modules are relocatable; the pspdev toolchain file provides the
    # link flags when BUILD_PRX-style output is requested per target.
    set_target_properties(rs_core_${name} PROPERTIES OUTPUT_NAME ${name})
  endif()

  target_include_directories(rs_core_${name} PRIVATE
    ${CMAKE_SOURCE_DIR}/src)          # for core_api headers only
  if(ARG_LIBS)
    target_link_libraries(rs_core_${name} PRIVATE ${ARG_LIBS})
  endif()
endfunction()
