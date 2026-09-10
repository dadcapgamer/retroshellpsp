# Changelog

## v1.0.0-beta.3

Beta 3 expands RetroShell from a fixed PRX launcher into a PSP-focused frontend
that can run both in-process cores and adapted standalone PSP emulators.

### Core and adapter platform

- Added `RSCoreAPI` version 4 and stricter validation at every PRX boundary.
- Added a native-emulator backend for emulators that need exclusive ownership
  of PSP memory, display, audio, timing, dynarec, and suspend handling.
- Added the native adapter SDK, bounded launch/session receipt, direct-ROM
  arguments, L+R+Select pause contract, and return-to-RetroShell flow.
- Added a shared per-game save library. Native save states remain isolated by
  emulator because state formats are not interchangeable.
- Added validated drag-and-drop `.rscore.zip` packages with transactional
  install, provenance, license records, size limits, and path traversal checks.
- Added deterministic multi-core selection. Every installed, non-blocklisted
  core for a system is shown; unsafe retired integrations remain unavailable.
- Added a generated emulator directory describing Included, Testing,
  Experimental, model support, and known removals.

### Core and PSP performance work

- Added host VFS and paged-ROM support so large GBA ROMs do not need to coexist
  entirely with core state in the PSP-1000 arena.
- Hardened gpSP memory mapping, ROM paging, dynarec cache handling, save access,
  and PSP-specific recovery behavior for demanding games and ROM hacks.
- Added Gambatte RTC persistence and PSP-native video paths.
- Added PSP-native zero-conversion frame paths where supported by Gambatte,
  QuickNES, Snes9x 2005, PicoDrive, and PCE Fast integrations.
- Tuned bounded audio recovery independently for GBA, SNES, PicoDrive, and
  PC Engine so a transient underrun cannot become an extended low-refresh run.
- Added safe suspend/resume behavior that returns an active game to the paused
  state after the PSP wakes.
- Hardened SRAM, RTC, save-state, screenshot, ZIP, geometry, pitch, rate, and
  allocation handling. Writes use recoverable temporary/backup paths.
- Added arena high-water and allocation-failure diagnostics for hardware logs.

### Library and interface

- Reworked large-library browsing to draw only visible rows and avoid loading
  artwork or metadata while it is off screen.
- Added natural, case-insensitive ordering, stable selection, cached indexes,
  cached beside-ROM artwork paths, and explicit rescanning.
- Fixed texture/CLUT lifetime and GU-state leaks that caused red/green artifacts
  after scrolling through larger libraries or opening the pause menu.
- Added square cover presentation with beside-ROM PNG/JPG discovery and a
  console-specific fallback when artwork is missing.
- Rebuilt the home console rail, collapsed Recently Played drawer, game lists,
  selected-game details, settings rows, action bar, and in-game pause menu.
- Added Geist Pixel interface fonts, redesigned console icons, updated
  RetroShell branding, 12/24-hour time setting, themes, and accent colors.
- Added the orange startup subtitle shimmer as lightweight loading feedback.

### Reliability and distribution

- Added staged lifecycle teardown so failed launches unwind only completed work.
- Added thread shutdown/join handling and safer cross-thread state.
- Added host tests for package policy, adapter protocol, pause-menu contract,
  bounds, rendering windows, ordering, and pixel-copy behavior.
- Expanded repository audits, deterministic packaging, provenance hashes,
  third-party notices, and PSP/PPSSPP qualification tooling.
- Release archives contain no ROMs, BIOS files, keys, logs, or personal data.

### Current compatibility status

- Included defaults: Gambatte (GB/GBC), QuickNES (NES), Snes9x 2005 (SNES),
  PicoDrive (Genesis/Mega Drive, with experimental SMS/Game Gear support).
- Testing packages: gpSP (GBA), Beetle PCE Fast (PC Engine), SMS Plus GX
  (Master System/Game Gear), and the FrogGBA native adapter.
- Snes9xTYL has an adapter specification and direct-launch patch but still
  requires a qualified distributable build before it can be offered.
- mGBA, FCEUmm, Gearboy, Snes9x 2005 Plus, and TGB Dual remain unavailable due
  to real-hardware crash, performance, video, or state-path failures.

Games and BIOS files are not included. Compatibility labels describe tested
integration status, not a guarantee that every game or ROM hack will work.

## v1.0.0-beta.2

- Improved large-library performance and stable natural ordering.
- Cached artwork paths and limited rendering to visible items.
- Fixed PSP menu texture artifacts and capped generated screenshots.

## v1.0.0-beta.1

- First public RetroShell PSP beta.
