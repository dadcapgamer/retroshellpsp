# Vendored third-party code (single-file libraries).
#   stb_image / stb_image_write — PNG/JPG decode, screenshot encode (MIT / public domain)
#   miniz                       — ZIP archive support (MIT)
#   cJSON                       — config / metadata / themes (MIT)

add_library(rs_external STATIC
  stb_impl.c
  miniz.c
  cJSON.c)

target_include_directories(rs_external PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The PSP has no mmap and we never touch archive timestamps.
target_compile_definitions(rs_external PUBLIC
  MINIZ_NO_TIME
  MINIZ_NO_MMAP
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES)  # keeps `crc32` etc. out of our namespace
