# FCEUmm (NES) via the libretro shim.
#
# upstream/ is a pruned copy of https://github.com/libretro/libretro-fceumm
# (GPLv2 — see upstream/COPYING). Source list mirrors Makefile.common's PSP
# target; the HD-pack and image-format decoders (png/dds/webp/…) are desktop
# features and are left out.

set(F  ${CMAKE_CURRENT_SOURCE_DIR}/upstream/src)
set(LR ${F}/drivers/libretro)
set(LC ${LR}/libretro-common)

file(GLOB FCEU_BOARDS ${F}/boards/*.c)
file(GLOB FCEU_INPUT  ${F}/input/*.c)

rs_add_core(fceumm
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${FCEU_BOARDS}
    ${FCEU_INPUT}
    ${F}/cart.c ${F}/cheat.c ${F}/crc32.c ${F}/fceu-endian.c
    ${F}/fceu-memory.c ${F}/fceu.c ${F}/fds.c ${F}/fds_apu.c ${F}/file.c
    ${F}/filter.c ${F}/general.c ${F}/input.c ${F}/md5.c ${F}/nsf.c
    ${F}/palette.c ${F}/ppu.c ${F}/sound.c ${F}/state.c ${F}/video.c
    ${F}/vsuni.c ${F}/ines.c ${F}/unif.c ${F}/x6502.c
    ${LR}/libretro.c ${LR}/libretro_dipswitch.c
    ${LC}/streams/memory_stream.c
    ${LC}/streams/file_stream.c
    ${LC}/streams/file_stream_transforms.c
    ${LC}/vfs/vfs_implementation.c
    ${LC}/file/file_path.c
    ${LC}/file/file_path_io.c
    ${LC}/encodings/encoding_utf.c
    ${LC}/compat/compat_strl.c
    ${LC}/compat/compat_snprintf.c
    ${LC}/compat/fopen_utf8.c
    ${LC}/time/rtime.c
    ${LC}/string/stdstring.c
    ../shared/psp_posix_stubs.c)

target_include_directories(rs_core_fceumm PRIVATE
  ${LR} ${LC}/include ${F} ${F}/input ${F}/boards)

# NOTE: do NOT define PSP here. FCEUmm's PSP path renders straight to the GU
# (it assumes RetroArch's PSP video driver), which fights our renderer. The
# generic path routes frames through video_cb, which is what the shim wants.
target_compile_definitions(rs_core_fceumm PRIVATE
  RS_CORE_NAME="fceumm"
  RS_CORE_SYSTEMS="nes"
  __LIBRETRO__
  FRONTEND_SUPPORTS_RGB565
  FCEU_VERSION_NUMERIC=9813
  PATH_MAX=1024
  INLINE=inline
  HAVE_ASPRINTF)

target_compile_options(rs_core_fceumm PRIVATE
  -O2                       # NES is light; -O2 keeps the interpreter I-cache-friendly
  # The pspdev toolchain force-defines PSP; cancel it so FCEUmm's RetroArch
  # direct-GU render path stays disabled and frames go through video_cb.
  -UPSP -U__PSP__ -U_PSP
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable)
