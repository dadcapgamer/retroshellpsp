# Changelog

## Unreleased — Systems, themes, save states, sharpness

- **Emulator Settings** in the pause menu: per-game options for every
  built-in emulator. Two RetroShell options apply to every core and take
  effect at once: **Frame skip** (Auto, or draw one frame in 2, 3 or 4) and
  **Audio buffer** (Normal, Large, Max — more buffering, fewer crackles).
  Each core adds its own speed and sound options: PicoDrive renderer, sound
  quality, sprite limit, audio and FM filters; gpSP sound, colour correction
  and dynarec; Snes9x filter, flicker and overclock; QuickNES palette, sprite
  limit and sound quality; Gambatte resampler, frame blending and colour;
  PCE Fast and SMS Plus options. The list scrolls when a core has more.
- **Settings stick, per emulator**: aspect ratio, filter and Emulator
  Settings are saved the moment you change them (quitting through HOME or
  turning the PSP off no longer loses them) and apply to every game on
  that emulator. Emulator Settings' new "Save for" row switches a game to
  "This game only", keeping its own copy of everything.
- **Per-game emulator choice is back**: Game Details > Emulator sets the
  emulator for that game only (or "System default"), without launching.
  Every other game follows the system default from Settings. Previously
  each launch silently pinned the emulator it used, so changing the
  default in Settings didn't reach games already played, and the Emulator
  row showed the system default rather than the core the game really used.
  Earlier per-game picks are not carried over; set them again once.
- Save states made by a different emulator now say so instead of being
  tried: the pause menu marks the slot "Other core" and points to Game
  Details, and a state from the retired in-process FrogGBA build is refused
  with a message rather than launching the native FrogGBA without it.
- **FrogGBA and Snes9xTYL ship in the release**, no separate install.
  Snes9xTYL now has the full RetroShell adapter, like FrogGBA: L+R+Select (or
  HOME) opens RetroShell's pause menu, Emulator Settings opens Snes9xTYL's
  own settings, saves live in RetroShell's save library, and Exit returns to
  RetroShell instead of the XMB. Both stay labelled Testing, and gpSP and
  Snes9x 2005 remain the defaults; pick them per game with the core picker.
  FrogGBA still needs your own GBA BIOS in `RETROSHELL/system/gba_bios.bin`.
- **Apply & Restart asks first**: save a state to a slot you pick and
  restart from it, restart from power-on without saving, or cancel. A save
  that fails never restarts the game. (The first version's hidden carry-over
  slot is gone; any leftover file is removed on launch.)
- Fix: per-game core options that a core reads during initialisation (such
  as PicoDrive's renderer) were ignored — the active game was set only after
  the core had loaded.
- **Console icons** are now exact integer box downscales of the masters: the
  previous hard-edged reduction broke thin outlines on the device.
- **L1 / R1 hints** where the shoulders work: the ends of the Home rail, the
  Library header (with the neighbouring systems), Continue's position and the
  Settings legend. Start is gone from the Home legend.
- Spacing pass: Home and Continue centred in the content band, legends in at
  least three even columns, a little more air under the Library header and
  the Detail chips.
- **Triangle opens Game Detail** directly; the Options popup is gone and its
  actions (Remove from Continue, Emulator) live on the Detail screen. Delete
  All Save Data is kept out of the way, as the last row of Save States, with
  a two-press confirmation and a plain warning. Game Detail shows no
  placeholder text: without a description there is no description box. The "Game Details" row is replaced by a facts line under
  the title (last played, play time, version, size); Return is O.
- **Settings → Systems**: every system with an emulator installed gets a row;
  Left/Right cycles its emulators and Off. Systems turned off disappear from
  Home and Continue Playing (their games stay indexed).
- **Two more themes**: Graphite (neutral charcoal) and Mist (cool daylight),
  beside Dark and Light.
- **Save States in Game Details**: every slot with when it was saved and by
  which emulator, a large preview of the selected slot (the saved frame at
  native resolution, shown 1:1 when it fits), X to start the game in that
  state with the emulator that wrote it, Triangle twice to delete. States
  saved before this build show their small thumbnail.
- **Sharper text**: atlases are baked with FreeType hinting
  (`tools/fontbake.c`), so stems land on whole pixels.
- **Sharper console icons**: regenerated as hard-edged downscales of the
  192 px masters (`tools/crisp_icons.py`) instead of smoothed exports.
- Save-state load failures now log their reason.

## Unreleased — Visual polish

Brings the whole shell to `RetroShell_Modern_PSP_Visual_Polish_Guide.md` and
its concept board: one design language on every screen, built for 480×272
and the PSP's rendering budget (flat surfaces, no blur, no shadows, no glow).

- **Deep navy is the default theme** (`#081828` field, two raised surfaces);
  Light is retuned to the same roles and contrast steps. Five accents; blue
  marks focus.
- **One typeface, IBM Plex Mono**, in SemiBold and Regular, with tracked
  uppercase for the wordmark, system names and pane titles. Labels centre on
  their cap height, so every row and chip sits on whole pixels.
- **One header and one footer everywhere**: the 3×3 dot mark and RETROSHELL
  wordmark, clock and battery; a legend in equal columns across the width.
  Scan progress lives in the header; toasts are a small pill above the legend.
- **Systems**: the selected system sits in a framed card; the name is set
  large and tracked, and crossfades on a switch.
- **Continue Playing** shows exactly three bordered cards: artwork, system
  chip, title and "Last played".
- **Library**: a 24 px thumbnail (or console glyph) per row, hairlines
  between rows, a chevron on the focused one; a framed preview with chips and
  a size line. X now plays (Game Details is the second row of Options).
- **Thumbnails** are built in the background from covers and screenshots,
  cached per system in `RETROSHELL/cache/thumbs/`, and loaded off the main
  thread, so scrolling never reads the Memory Stick.
- **Branded artwork fallback**: pattern, RetroShell mark, system icon and
  abbreviation at every size — never an empty frame.
- **Game Detail**: framed art and description box; icon action rows with Play
  first, showing the emulator it will use. Cheats and View Manual are not
  offered (no data exists for them).
- **Settings**: a sidebar (Appearance, Performance, Audio, Library, About)
  beside a panel of that category's rows; About carries the build details.
- Setup, empty, loading and error states, the in-game pause menu and every
  popup use the same cards, rows, chips and type; 11×11 pixel icons
  throughout.
- Font atlases cut from 278 KB to 155 KB; the boot scene draws the embedded
  splash instead of a 26 px atlas. Startup arena: 17,099 KB.

## Unreleased — Maturity pass

Closes the gaps found auditing the shell against
`RetroShell_Mature_Frontend_Foundations.md` (see `docs/MATURITY_AUDIT.md`).

- **Clean titles.** File names are cleaned for display
  (`Pokemon - Crystal Version (USA, Europe) (Rev 1)` -> `Pokémon Crystal`);
  dump tags become a variant, shown only to tell duplicates apart and in
  Game Details, which also keeps the real file name. Missing metadata fields
  are omitted instead of printed as "Unknown".
- **View menu (Select, in a Library):** Show All / Favorites / Recently Added,
  sort A-Z / Z-A / Recently Played / Most Played, and D-pad letter search.
  Filter and sort are remembered per system.
- **Resume anywhere.** The rail system, layer and Continue slot persist across
  process restarts (native emulators replace the process). Play time is
  tracked and shown in Game Details.
- **Actionable errors** for missing ROM, emulator, BIOS, storage, long path,
  memory limit and rejected ROM, each with one fix action; plus explicit
  empty, scanning and "ROM folder not found" screens.
- **First-run setup** (scan, confirm systems, confirm emulators, done);
  re-runnable from Settings. Per-system default emulator is now configurable.
- **Artwork fallback** now includes your latest in-game screenshot before the
  system placeholder.
- Low-battery colour and toasts; Select glyph; shared state-panel and
  text-layout primitives.
- Removed the unused 69 KB Inter 19 atlas to pay for the new code; startup
  arena is 17,016 KB (gate: 17,000).

## Unreleased — Astra shell redesign

Home is rebuilt around a spatial, PSP-native model instead of the card
dashboard: **horizontal changes system, vertical moves within a layer, X
selects, O goes back, Square favorites, Triangle opens options, Start goes
Home.**

- **Systems** is a five-slot rail with a boxed selection, system name and game
  count. It lists only systems that have games.
- **Continue Playing** (Up from Systems) shows up to five recent games across
  systems with art, title, system and relative last-played time ("2h ago").
  X resumes, Square favorites, Triangle opens options. A single recent game
  collapses to one compact item; with none, the layer is omitted.
- **Library** is a flat text list with a selection bar and stars. Left/Right
  and L/R switch system without leaving the Library, and each system
  remembers its selected game, also across restarts.
- New **Game Detail** screen (X on a game in the Library, or Options -> Game
  Details; Play is its first action, so launching is X, X) and a new **Options** popup: Play, Add/Remove Favorite, Game
  Details, Cheats, View Manual, Delete Save Data (confirms), Remove from
  Recent, Back. Cheats and manuals show "not available" until data exists.
- Designed artwork fallback (dot field, system icon, abbreviation); no blank
  or broken image slots. Metadata line reads "Publisher · Year".
- Console icons and the face-button / START glyphs now come from the latest
  Figma library ("console icons v2", "system icons").
- Flat ivory/navy themes with a cobalt accent, monospaced pixel typography,
  compact status bar (clock, battery %), and a space-between control legend.
- Settings adopts the same look. The Home layout option is retired.
- Library Memory Stick writes stay off the browsing path: selection memory is
  flushed when leaving Home.
- Tooling: `RS_AUTOPILOT_TOUR` walks every Home layer for visual review;
  new host tests cover relative time and the navigation model.

## v1.0.0-beta.4

Beta 4 completes the native adapter pipeline, adds a home-layout choice, and
fixes a memory regression that could freeze the application mid-game.

### Native emulator adapters

- FrogGBA now launches, creates its own working directories, and returns to
  RetroShell instead of exiting to the XMB. Launch and return are confirmed on
  a real PSP-1000.
- Fixed native launches failing with `0x80020149`
  (`SCE_KERNEL_ERROR_ILLEGAL_PERM_CALL`). RetroShell is a user-mode module and
  `sceKernelLoadExec` is privileged, so launches now go through custom
  firmware's SystemCtrl loader with the direct call kept as a fallback.
- Native manifests can declare `requiredDirectories`. A ZIP cannot carry empty
  folders, so the adapter's `roms`/`save`/`state`/`cfg`/`cheat`/`snapshot`
  directories are created before the emulator starts; FrogGBA previously
  aborted naming each missing path.
- Native launch failures now report the cause — a missing BIOS, an
  uninstalled emulator, an over-long ROM path — instead of a status code.
- Native adapters no longer outrank in-process cores. Both shipped adapters
  were priority 140 against the PRX cores' 100, which would have silently made
  them the default for their systems rather than the alternates the
  documentation describes. A repository audit rule now prevents this.

### Interface

- Added **Settings -> Home layout**, switching between the beta.3 text rail
  (`Modern`) and the pre-beta.3 badge cards (`Classic`). The two differ in
  recents navigation as well as painting, and both are supported.
- Settings now shows the build identity — release version, build timestamp and
  core API version — so a console can be matched to the artifact that produced
  it without reading the log. The timestamp is regenerated on every build; a
  configure-time value went stale across incremental rebuilds, which would let
  a console report a time from long before the binary it was running.

### Fixes

- Fixed the application freezing mid-game on the first in-game screenshot.
  `PSP_HEAP_SIZE_KB` had been reduced on the strength of a core-launch heap
  sample, which never observes a capture — the largest heap burst in the
  application at 1,536 KB claimed. The PSP installs no exception handler, so
  the failure appeared as the application silently stopping.
- Fixed a zero-size SRAM being treated as an error for cartridges without
  battery backup, which failed the release gate on ordinary games.
- QuickNES and Gambatte now emit the PSP GE's native BGR565 ordering from
  their palettes, removing a per-pixel channel swap of 61,440 and 23,040
  pixels per frame respectively at no runtime cost.

### Documentation and tooling

- Added a step-by-step BIOS setup guide covering the required path, exact
  size, and every failure message.
- `tools/run_ppsspp_automation.py` gained `--psp-model` (the 64 MB model had
  never been exercised) and `--home-layout`.
- Corrected a stale hardware matrix that still reported July failures
  superseded by the September run.
- Fixed the vendored mGBA tree adopting this repository's git tag and dirty
  state as its own version. Its `version.cmake` ran `git describe` from inside
  the repository, so `mgba.prx` changed bytes on every commit and on every
  release tag, breaking the provenance lockfile the artifact hash is meant to
  pin. The build now passes `SKIP_GIT` through to the build-time version
  script, making the artifact depend only on the pinned source.

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
