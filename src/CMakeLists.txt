add_executable(retrosuite
  main.cpp
  platform/psp/gu_renderer.cpp
  platform/psp/vram.cpp
  platform/psp/input_pad.cpp
  platform/psp/power.cpp
  platform/psp/fs_psp.cpp
  platform/psp/threading.cpp
  platform/psp/audio_out.cpp
  runtime/log.cpp
  runtime/arena.cpp
  runtime/config.cpp
  runtime/host_services.cpp
  runtime/save_manager.cpp
  frontend/app.cpp
  frontend/core_manager.cpp
  frontend/core_registry.cpp
  runtime/jsonfile.cpp
  frontend/game_session.cpp
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

if(RS_INCLUDE_TEST_CORES)
  # Candidate visibility is a frontend policy. Do not leak this definition
  # into every PRX or identical core sources produce different artifacts in
  # stable and candidate bundles.
  target_compile_definitions(retrosuite PRIVATE RS_INCLUDE_TEST_CORES=1)
endif()

# First-party code is held to strict diagnostics. Vendored emulator sources
# keep their narrowly scoped compatibility suppressions in each core target.
target_compile_options(retrosuite PRIVATE
  -Wextra
  -Werror=return-type
  -Werror=format
  -Werror=implicit-fallthrough)

target_link_libraries(retrosuite PRIVATE
  rs_external
  pspgu
  pspge
  pspdisplay
  pspctrl
  psppower
  psprtc
  pspaudio
  pspdebug
  m)

rs_embed_assets(retrosuite
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_title.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_large.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_body.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_small.rsf
  ${CMAKE_SOURCE_DIR}/assets/SPLASH.PNG
  ${CMAKE_SOURCE_DIR}/assets/branding/retroshell-logo-light-2x.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-color.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-advance.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/nes.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/snes.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/megadrive.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/master-system.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-gear.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/pc-engine.png)

create_pbp_file(
  TARGET retrosuite
  TITLE "RetroShell"
  ICON_PATH ${CMAKE_SOURCE_DIR}/assets/ICON0.PNG
  BACKGROUND_PATH ${CMAKE_SOURCE_DIR}/assets/PIC1.PNG
  VERSION 01.00)
