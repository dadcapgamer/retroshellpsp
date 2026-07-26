# SMS Plus GX (Master System / Game Gear) via the libretro shim.
# Pinned upstream source and licenses are under upstream/docs.

set(SP ${CMAKE_CURRENT_SOURCE_DIR}/upstream/source)
set(SP_LR ${SP}/ports/libretro)
set(SP_COMMON ${SP_LR}/libretro-common)

rs_add_core(smsplus
  SOURCES
    ../shared/rs_libretro_shim.cpp
    ${SP}/loadrom.c
    ${SP}/memz80.c
    ${SP}/pio.c
    ${SP}/render.c
    ${SP}/sms.c
    ${SP}/state.c
    ${SP}/system.c
    ${SP}/tms.c
    ${SP}/vdp.c
    ${SP}/cpu_cores/z80/z80.c
    ${SP}/sound/maxim_sn76489/sn76489.c
    ${SP}/sound/fmintf.c
    ${SP}/sound/sound.c
    ${SP}/sound/ym2413.c
    ${SP_COMMON}/streams/memory_stream.c
    ${SP_LR}/smsplus_libretro.c)

target_include_directories(rs_core_smsplus PRIVATE
  ${SP} ${SP_LR} ${SP_COMMON}/include
  ${SP}/cpu_cores/z80
  ${SP}/sound ${SP}/sound/maxim_sn76489 ${SP}/sound_output)

target_compile_definitions(rs_core_smsplus PRIVATE
  RS_CORE_NAME="smsplus"
  RS_CORE_SYSTEMS="sms|gg"
  __LIBRETRO__
  NOZIP_SUPPORT
  MAXIM_PSG
  USE_Z80
  LSB_FIRST
  ALIGN_DWORD
  HAVE_NO_LANGEXTRA)

target_compile_options(rs_core_smsplus PRIVATE
  -O3
  -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter)
