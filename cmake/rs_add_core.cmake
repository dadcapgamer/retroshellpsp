# rs_add_core(<name> SOURCES <src>... [LIBS <lib>...])
#
# Builds an emulator core against the RetroSuite Core API in one of two
# delivery modes:
#
#   default            — relocatable PRX module (build/cores/<name>.prx,
#                        installed to ms0:/RETROSUITE/cores/). Only one
#                        core occupies RAM at a time.
#   -DRS_STATIC_CORES  — static library linked into the EBOOT and listed
#                        in the generated rs_static_cores.inc registry.
#
# In static mode the core's rs_get_core_api symbol is renamed per-core so
# several cores can coexist in one binary; RS_STATIC_BUILD=1 lets core
# sources drop their PRX module boilerplate.
#
# PRX link recipe mirrors $PSPSDK/lib/build_prx.mak: link with -Wl,-q and
# linkfile.prx, no start files, then psp-fixup-imports + psp-prxgen.

execute_process(COMMAND psp-config --pspsdk-path
                OUTPUT_VARIABLE RS_PSPSDK_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

foreach(crt crti crtbegin crtend crtn)
  string(TOUPPER ${crt} crt_upper)
  execute_process(COMMAND psp-gcc -print-file-name=${crt}.o
                  OUTPUT_VARIABLE RS_${crt_upper}_O
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endforeach()

function(rs_add_core name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  set(target rs_core_${name})

  if(RS_STATIC_CORES)
    add_library(${target} STATIC ${ARG_SOURCES})
    target_compile_definitions(${target} PRIVATE
      RS_STATIC_BUILD=1
      rs_get_core_api=rs_core_entry_${name})
    set_property(GLOBAL APPEND PROPERTY RS_STATIC_CORE_NAMES ${name})
    target_link_libraries(retrosuite PRIVATE ${target})
  else()
    set(exports_c ${CMAKE_CURRENT_BINARY_DIR}/${name}_exports.c)
    add_custom_command(
      OUTPUT ${exports_c}
      COMMAND sh -c "psp-build-exports -b '${CMAKE_CURRENT_SOURCE_DIR}/exports.exp' > '${exports_c}'"
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports.exp
      COMMENT "Generating exports for core ${name}"
      VERBATIM)

    add_executable(${target} ${ARG_SOURCES} ${exports_c})
    set_target_properties(${target} PROPERTIES
      OUTPUT_NAME ${name}
      SUFFIX .elf)
    # PSP_MODULE_INFO stringifies its first argument, so this must be an
    # unquoted token, not a string.
    target_compile_definitions(${target} PRIVATE
      RS_CORE_MODULE_ID=rs_core_${name})
    # -nostartfiles drops crt0 (a plugin must not own module_start or spawn
    # a main thread), but C++ cores still need the ctor/dtor machinery from
    # the crt objects: crti+crtbegin ahead of our objects, crtend+crtn
    # after (CMake puts link *options* before objects and link *libraries*
    # after, which yields exactly that order). The shim then runs
    # constructors by calling _init(). crtbegin also provides __dso_handle.
    target_link_options(${target} PRIVATE
      -Wl,-q
      -T${RS_PSPSDK_PATH}/lib/linkfile.prx
      -nostartfiles
      -Wl,-zmax-page-size=128
      ${RS_CRTI_O} ${RS_CRTBEGIN_O})
    target_link_libraries(${target} PRIVATE ${RS_CRTEND_O} ${RS_CRTN_O})

    add_custom_command(
      TARGET ${target} POST_BUILD
      # fixup-imports fails on modules with zero sce imports (no .lib.stub
      # section) — that's fine for self-contained cores like dummy.
      COMMAND sh -c "psp-fixup-imports '$<TARGET_FILE:${target}>' || true"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/cores
      COMMAND psp-prxgen $<TARGET_FILE:${target}>
              ${CMAKE_BINARY_DIR}/cores/${name}.prx
      COMMENT "Packaging core ${name}.prx"
      VERBATIM)

    # The manifest ships beside the .prx so CoreRegistry can list the core
    # without loading it (see src/frontend/core_registry.h).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json)
      add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
                ${CMAKE_BINARY_DIR}/cores/${name}.json
        VERBATIM)
    else()
      message(WARNING
        "core '${name}' has no manifest.json — it will not be discovered "
        "in PRX builds")
    endif()
  endif()

  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)

  # Emulator cores rely on wrapping signed arithmetic and type-punning that
  # GCC's default strict-overflow / strict-aliasing optimizations miscompile
  # at -O2/-O3 — the classic "core runs but renders garbage/nothing" bug.
  # Every upstream libretro Makefile builds with these; apply them to all
  # cores so no future core has to remember.
  target_compile_options(${target} PRIVATE
    -fno-strict-overflow -fno-strict-aliasing)

  if(ARG_LIBS)
    target_link_libraries(${target} PRIVATE ${ARG_LIBS})
  endif()
endfunction()

# Called once from cores/CMakeLists.txt after all rs_add_core() calls:
# writes the static-core registry consumed by core_manager.cpp.
function(rs_finalize_static_cores)
  if(NOT RS_STATIC_CORES)
    return()
  endif()
  get_property(names GLOBAL PROPERTY RS_STATIC_CORE_NAMES)
  set(decls "")
  set(rows "")
  set(SC "\;")   # literal semicolon for the generated C source
  foreach(n IN LISTS names)
    string(APPEND decls
           "extern \"C\" const RSCoreAPI* rs_core_entry_${n}(void)${SC}\n")
    string(APPEND rows "    {\"${n}\", &rs_core_entry_${n}},\n")
  endforeach()
  set(content "/* Auto-generated static core registry — do not edit. */
${decls}
struct RSStaticCoreEntry {
    const char* name${SC}
    const RSCoreAPI* (*getApi)(void)${SC}
}${SC}

static const RSStaticCoreEntry RS_STATIC_CORE_TABLE[] = {
${rows}}${SC}
")
  string(REPLACE "\;" ";" content "${content}")
  file(WRITE ${CMAKE_BINARY_DIR}/generated/rs_static_cores.inc "${content}")
  target_include_directories(retrosuite PRIVATE ${CMAKE_BINARY_DIR}/generated)
endfunction()
