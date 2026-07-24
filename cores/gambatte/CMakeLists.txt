# Gambatte (Game Boy / Game Boy Color) via the libretro shim.
#
# upstream/ is a pruned copy of https://github.com/libretro/gambatte-libretro
# (commit 9b3b5e3, GPLv2 — see upstream/COPYING). Source lists and defines
# mirror upstream's Makefile.common / Makefile.libretro psp1 target.

set(GB ${CMAKE_CURRENT_SOURCE_DIR}/upstream/libgambatte)
set(LRC ${GB}/libretro-common)

rs_add_core(gambatte
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${GB}/libretro/libretro.cpp
    ${GB}/libretro/gambatte_log.c
    ${GB}/libretro/blipper.c
    ${GB}/libretro/cc_resampler.c
    ${GB}/src/bootloader.cpp
    ${GB}/src/cpu.cpp
    ${GB}/src/gambatte.cpp
    ${GB}/src/initstate.cpp
    ${GB}/src/interrupter.cpp
    ${GB}/src/interruptrequester.cpp
    ${GB}/src/gambatte-memory.cpp
    ${GB}/src/sound.cpp
    ${GB}/src/statesaver.cpp
    ${GB}/src/tima.cpp
    ${GB}/src/video.cpp
    ${GB}/src/video_libretro.cpp
    ${GB}/src/mem/cartridge.cpp
    ${GB}/src/mem/cartridge_libretro.cpp
    ${GB}/src/mem/huc3.cpp
    ${GB}/src/mem/memptrs.cpp
    ${GB}/src/mem/rtc.cpp
    ${GB}/src/sound/channel1.cpp
    ${GB}/src/sound/channel2.cpp
    ${GB}/src/sound/channel3.cpp
    ${GB}/src/sound/channel4.cpp
    ${GB}/src/sound/duty_unit.cpp
    ${GB}/src/sound/envelope_unit.cpp
    ${GB}/src/sound/length_counter.cpp
    ${GB}/src/video/ly_counter.cpp
    ${GB}/src/video/lyc_irq.cpp
    ${GB}/src/video/next_m0_time.cpp
    ${GB}/src/video/ppu.cpp
    ${GB}/src/video/sprite_mapper.cpp
    ${LRC}/compat/compat_posix_string.c
    ${LRC}/compat/compat_snprintf.c
    ${LRC}/compat/compat_strcasestr.c
    ${LRC}/compat/compat_strl.c
    ${LRC}/compat/fopen_utf8.c
    ${LRC}/encodings/encoding_utf.c
    ${LRC}/file/file_path.c
    ${LRC}/file/file_path_io.c
    ${LRC}/streams/file_stream.c
    ${LRC}/streams/file_stream_transforms.c
    ${LRC}/string/stdstring.c
    ${LRC}/time/rtime.c
    ${LRC}/vfs/vfs_implementation.c)

target_include_directories(rs_core_gambatte PRIVATE
  ${GB}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/common
  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/common/resample
  ${GB}/include
  ${GB}/libretro
  ${LRC}/include)

target_compile_definitions(rs_core_gambatte PRIVATE
  RS_CORE_NAME="gambatte"
  RS_CORE_SYSTEMS="gb|gbc"
  __LIBRETRO__
  PSP
  HAVE_STDINT_H
  HAVE_INTTYPES_H
  CC_RESAMPLER_NO_HIGHPASS
  VIDEO_RGB565)

# Gambatte's CPU/PPU inner loops are the frame-time hot path; -O3 (over the
# project-wide -O2) buys tighter inlining there. Wins are measured via the
# in-game "emu us/frame" perf log, not assumed.
#
# The -Wno-* flags silence warnings from the vendored upstream sources only
# (member-init order, signedness, unused locals) so they don't bury real
# warnings from the shim. Upstream is not ours to fix.
target_compile_options(rs_core_gambatte PRIVATE
  -O3
  -Wno-sign-compare -Wno-unused-variable
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>)
