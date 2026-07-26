# RetroSuite PSP

A modern, all-in-one retro emulator frontend for the Sony PSP. One EBOOT,
one polished XMB-inspired interface, many emulator cores behind a plugin
architecture — inspired by Emu4Vita++, built native for the PSP.

- **Targets:** PSP-1000 (32MB) through PSP Go, and PPSSPP
- **Stack:** pspsdk · CMake · C++17 · PSP GU rendering · no SDL, no Qt,
  no RetroArch
- **Design:** bi-layer runtime — the frontend tears itself down to hand
  nearly all RAM to the running emulator core, then rebuilds seamlessly

## Features (Phases 1–4, current)

- 60 fps animated UI: XMB-style category bar, game lists, light/dark themes
  with live crossfade, animated wave background, battery + clock
- Background ROM scanner for `ms0:/ROMS/<System>/` with ZIP support and a
  binary index cache (instant boot)
- Favorites, recently played, play counts
- Box art + metadata (drop-in JSON/PNG files, no tooling required)
- Theme engine: user themes on the Memory Stick can override any color,
  the background image, and the wave animation
- Core API: emulator cores are loadable PRX plugins, with exactly one core
  in RAM at a time. Static multi-core builds are deliberately blocked
  because libretro's global `retro_*` symbols cannot safely coexist.
- Production defaults: Gambatte (GB/GBC), FCEUmm (NES), Snes9x 2005
  (SNES), and PicoDrive (Genesis/Master System/Game Gear).
- Gearboy, TGB Dual, QuickNES, SMS Plus GX, Snes9x 2005 Plus, and a
  PSP-pruned Snes9x 2010 SA-1 compatibility core are buildable test
  candidates, but are omitted from normal releases until PSP-1000
  measurements prove a compatibility win.
- **Manifest-driven core registry**: cores are discovered at boot from
  `.json` sidecars, so adding one never touches frontend source. When a
  system has more than one installed core the launch UI offers a picker,
  and each game remembers the core it was last played with
- **libretro shim** (`cores/shared/`): one reusable bridge so future
  libretro cores integrate with almost no per-core code
- Save states with thumbnails, battery-save autosave, screenshots,
  in-game menu (L + R + START)
- GBA and PC Engine remain blocked until streaming, memory, and hardware
  safety gates are complete.

See [docs/ADDING_A_CORE.md](docs/ADDING_A_CORE.md) to add your own.

## Building

Install the [pspdev toolchain](https://github.com/pspdev/pspdev/releases)
(prebuilt archives exist for macOS/Linux) and make sure `psp-cmake` is on
your PATH:

```sh
export PSPDEV="$HOME/pspdev"
export PATH="$PSPDEV/bin:$PATH"
./build.sh            # → build/src/EBOOT.PBP + build/cores/*.prx
./build.sh test       # manifests, provenance, source hashes, PRX policy
./build.sh release    # audited deterministic dist/RetroSuite-PSP.zip
./build.sh candidates # experimental EBOOT, all candidates, .rscore.zip files
```

If the prebuilt toolchain fails to start on macOS, install its library
dependencies: `brew install gmp mpfr libmpc isl zstd`.

## Installing

```
ms0:/PSP/GAME/RetroSuite/EBOOT.PBP      ← build/src/EBOOT.PBP
ms0:/RETROSUITE/cores/*.prx + *.json    ← production pairs from release ZIP
ms0:/ROMS/GameBoy/…                     ← your ROMs (see layout below)
```

Each core needs both its `.prx` and its `.json` manifest in the cores
folder — the frontend lists a core from the manifest and won't show one
whose module is missing.

ROM directories: `GameBoy`, `GameBoyColor`, `GBA`, `NES`, `SNES`,
`Genesis`, `MasterSystem`, `GameGear`, `PCEngine`. ZIPs are fine.

Optional, per game (`<name>` = ROM file name without extension):

```
ms0:/RETROSUITE/boxart/<System>/<name>.png
ms0:/RETROSUITE/metadata/<System>/<name>.json
    { "year": 1989, "genre": "Puzzle", "developer": "…",
      "publisher": "…", "description": "…" }
ms0:/RETROSUITE/themes/<yourtheme>/theme.json     (see docs/ARCHITECTURE.md)
```

## Testing in PPSSPP

Point PPSSPP at `build/src/EBOOT.PBP`. The virtual Memory Stick lives in
PPSSPP's config directory (`~/.config/ppsspp/` on macOS/Linux) — put
`ROMS/` and `RETROSUITE/` there. Keep the PSP model at PSP-1000 to test
32MB memory behavior honestly.

Developer goodies:

- Run `./build.sh candidates` to create
  `dist/RetroSuite-PSP-Candidates.zip` and individual drag-and-drop
  candidate packages under `dist/cores/`. Install the candidate EBOOT
  before testing a `testOnly` core. Candidate PRXs are native executable
  code, so only install packages from a source you trust.
- In the game list, press Square to choose an installed core for that game.
  Snes9x 2005 remains the deterministic production default; a successful
  launch remembers the selected alternate.
- `-DRS_AUTOPILOT=ON` builds a scripted self-test that drives the UI,
  launches a game, exercises save states, and dumps framebuffer PNGs to
  `ms0:/RETROSUITE/shots/` — enable PPSSPP's software renderer for exact
  framebuffer readbacks.
- `L-trigger + TRIANGLE` toggles the FPS/frame-time overlay.
- Logs: `ms0:/RETROSUITE/retrosuite.log`.
- Runtime logs include p95 core frame time, arena high-water/allocation
  failures, and audio underrun/drop counters for hardware qualification.

The acceptance matrix and unresolved hardware gates are tracked in
[docs/CORE_AUDIT.md](docs/CORE_AUDIT.md). PPSSPP is useful for correctness,
but only real PSP hardware can approve performance.

## Project layout

```
src/core_api/     the C contract between frontend and cores (start here)
src/platform/psp/ every sce* call: GU renderer, audio, input, fs, power
src/runtime/      arena allocator, config, save manager, host services
src/frontend/     app shell, scenes, UI kit, themes, library database
cores/            emulator cores (each: sources + exports.exp + CMake)
tools/            host-side asset baker (fonts → .rsf, PBP art)
assets/           baked fonts and artwork (committed; no tooling needed)
docs/             architecture and core-porting guides
```

Want to add a core or fork the UI? Read `docs/ARCHITECTURE.md` and
`docs/ADDING_A_CORE.md`.

## License

MIT for RetroSuite itself (see `LICENSE`). Vendored libraries and future
emulator cores keep their upstream licenses.
