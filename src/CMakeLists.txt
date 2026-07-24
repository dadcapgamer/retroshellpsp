add_executable(retrosuite
  main.cpp
  platform/psp/gu_renderer.cpp
  platform/psp/vram.cpp
  platform/psp/input_pad.cpp
  platform/psp/power.cpp
  platform/psp/fs_psp.cpp
  platform/psp/threading.cpp
  runtime/log.cpp
  runtime/arena.cpp
  runtime/config.cpp
  frontend/app.cpp
  frontend/autopilot.cpp
  frontend/text/font.cpp
  frontend/ui/prim.cpp
  frontend/themes/palette.cpp
  frontend/themes/theme.cpp
  frontend/database/game_index.cpp
  frontend/database/rom_scanner.cpp
  frontend/database/library.cpp
  frontend/database/metadata.cpp
  frontend/scenes/boot_scene.cpp
  frontend/scenes/home_scene.cpp
  frontend/scenes/settings_scene.cpp)

target_include_directories(retrosuite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(retrosuite PRIVATE
  rs_external
  pspgu
  pspge
  pspdisplay
  pspctrl
  psppower
  psprtc
  pspdebug
  m)

rs_embed_assets(retrosuite
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_title.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_large.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_body.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_small.rsf)

create_pbp_file(
  TARGET retrosuite
  TITLE "RetroSuite"
  ICON_PATH ${CMAKE_SOURCE_DIR}/assets/ICON0.PNG
  BACKGROUND_PATH ${CMAKE_SOURCE_DIR}/assets/PIC1.PNG
  VERSION 00.10)
