# rs_embed_assets(<target> <asset-file>...)
#
# Embeds binary files into a target as C arrays. For each asset `foo.bar` a
# symbol `rs_asset_foo_bar` (+ `_len`) is generated, declared in the header
# "rs_asset_foo_bar.h" which is added to the target's include path.

function(rs_embed_assets target)
  set(gen_dir "${CMAKE_BINARY_DIR}/embedded_assets")
  file(MAKE_DIRECTORY "${gen_dir}")

  foreach(asset IN LISTS ARGN)
    get_filename_component(abs "${asset}" ABSOLUTE)
    get_filename_component(name "${asset}" NAME)
    string(TOLOWER "${name}" sym)
    string(REGEX REPLACE "[^a-z0-9]" "_" sym "${sym}")
    set(sym "rs_asset_${sym}")
    set(out_c "${gen_dir}/${sym}.c")
    set(out_h "${gen_dir}/${sym}.h")

    add_custom_command(
      OUTPUT "${out_c}" "${out_h}"
      COMMAND ${CMAKE_COMMAND}
              -DIN=${abs} -DOUT_C=${out_c} -DOUT_H=${out_h} -DSYM=${sym}
              -P ${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake
      DEPENDS "${abs}" "${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake"
      COMMENT "Embedding asset ${name}"
      VERBATIM)

    target_sources(${target} PRIVATE "${out_c}")
  endforeach()

  target_include_directories(${target} PRIVATE "${gen_dir}")
endfunction()
