set(RS_BUILD_STAMP_HEADER ${CMAKE_CURRENT_BINARY_DIR}/rs_build_stamp.h)
add_custom_target(rs_build_stamp ALL
  COMMAND ${CMAKE_COMMAND} -DOUT_FILE=${RS_BUILD_STAMP_HEADER}
          -P ${CMAKE_SOURCE_DIR}/cmake/rs_build_stamp.cmake
  BYPRODUCTS ${RS_BUILD_STAMP_HEADER}
  COMMENT "Stamping build time")

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
  frontend/core_package_installer.cpp
  frontend/core_manager.cpp
  frontend/core_registry.cpp
  frontend/native_emulator_launcher.cpp
  runtime/jsonfile.cpp
  frontend/game_session.cpp
  frontend/autopilot.cpp
  frontend/text/font.cpp
  frontend/ui/prim.cpp
  frontend/ui/text_layout.cpp
  frontend/ui/state_panel.cpp
  frontend/ui/chrome.cpp
  frontend/ui/icons.cpp
  frontend/themes/palette.cpp
  frontend/themes/theme.cpp
  frontend/database/game_index.cpp
  frontend/database/rom_scanner.cpp
  frontend/database/library.cpp
  frontend/database/metadata.cpp
  frontend/database/thumb_cache.cpp
  frontend/scenes/boot_scene.cpp
  frontend/scenes/home_scene.cpp
  frontend/scenes/settings_scene.cpp)

# Cold, non-per-frame UI and setup code is built for size: every byte of the
# module comes out of the PSP-1000's core arena, and the startup arena has a
# 17,000 KB release gate. The renderer, fonts, the home scene and the
# emulation loop stay at -O2.
set_source_files_properties(
  frontend/scenes/settings_scene.cpp
  frontend/scenes/boot_scene.cpp
  frontend/ui/state_panel.cpp
  frontend/core_package_installer.cpp
  frontend/native_emulator_launcher.cpp
  PROPERTIES COMPILE_OPTIONS "-Os")

target_include_directories(retrosuite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# First-party code is held to strict diagnostics. Vendored emulator sources
# keep their narrowly scoped compatibility suppressions in each core target.
target_compile_options(retrosuite PRIVATE
  -Wextra
  -Werror=return-type
  -Werror=format
  -Werror=implicit-fallthrough)

add_dependencies(retrosuite rs_build_stamp)
target_include_directories(retrosuite PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

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
  # Custom firmware's kernel-side loader, reachable from a user module.
  # sceKernelLoadExec alone is privileged and fails with ILLEGAL_PERM_CALL,
  # so native emulator adapters cannot launch without this.
  pspsystemctrl_user
  m)

rs_embed_assets(retrosuite
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_display.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_title.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_body_strong.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_body.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_small.rsf
  ${CMAKE_SOURCE_DIR}/assets/fonts/font_tiny.rsf
  ${CMAKE_SOURCE_DIR}/assets/SPLASH.PNG
  ${CMAKE_SOURCE_DIR}/assets/branding/retroshell-logo-light-2x.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-color-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-color-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-color-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-advance-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-advance-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-boy-advance-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/nes-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/nes-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/nes-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/snes-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/snes-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/snes-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/megadrive-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/megadrive-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/megadrive-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/master-system-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/master-system-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/master-system-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-gear-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-gear-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/game-gear-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/pc-engine-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/pc-engine-48.png
  ${CMAKE_SOURCE_DIR}/assets/icons/consoles/pc-engine-64.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-cross-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-circle-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-triangle-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-square-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-start-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-left-shoulder-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-right-shoulder-24.png
  ${CMAKE_SOURCE_DIR}/assets/icons/controls/control-select-24.png)

create_pbp_file(
  TARGET retrosuite
  TITLE "RetroShell"
  ICON_PATH ${CMAKE_SOURCE_DIR}/assets/ICON0.PNG
  BACKGROUND_PATH ${CMAKE_SOURCE_DIR}/assets/PIC1.PNG
  VERSION 01.00)
