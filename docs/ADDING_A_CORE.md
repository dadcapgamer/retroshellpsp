# Adding an emulator core to RetroSuite

A core is one directory under `cores/` with four things:

```
cores/mycore/
├── CMakeLists.txt      rs_add_core(mycore SOURCES …)
├── exports.exp         PRX export table (copy dummy's verbatim)
├── manifest.json       { "name", "version", "systems", "priority", "testOnly" }
└── …sources…           the emulator + a thin adapter
```

The frontend discovers cores at boot by reading the manifests next to the
`.prx` files — **you do not edit any frontend source to add a core.** Drop
the directory in, register it in `cores/CMakeLists.txt`, build, and it
appears. When several installed cores claim the same system, the highest
priority core is the deterministic default and Square opens the per-game
core picker.

Two ways in, easiest first:

- **A libretro core** (gambatte, gpSP, PicoDrive, SMS Plus GX, FCEUmm,
  Snes9x 2005 — most PSP-viable emulators): reuse the shared shim, write
  no adapter. See "Porting a libretro core" below and copy
  `cores/gambatte/` — it is ~90 lines of CMake and a manifest, no C++.
- **Everything else**: implement `RSCoreAPI` directly. Study `cores/dummy/`
  — a complete, working core in ~250 lines of C.

## The manifest

```json
{
  "name": "mycore",
  "version": "1.2.0",
  "systems": "gb|gbc",
  "priority": 100,
  "testOnly": true
}
```

`name` must match the module (`mycore.prx`) and the `RSCoreAPI::name`
field. `systems` is a pipe-separated list of the stable core-ids from
`db::SystemInfo::coreId` in `src/frontend/database/systems.h`
(`gb gbc gba nes snes md sms gg pce`). Higher `priority` wins, with core
name as the stable tie-breaker. New cores stay `testOnly` until they pass
the PSP-1000 release gate. `rs_add_core` copies the manifest next to the
built `.prx`.

## The contract

Implement every function of `RSCoreAPI` (`src/core_api/rs_core_api.h`) and
expose the table:

```c
const RSCoreAPI* rs_get_core_api(void);
```

Rules that keep cores portable and the PSP-1000 alive:

- **No sce\* calls, no display, no audio hardware.** Everything goes
  through the `RSHostAPI` table you receive in `init` (logging, memory,
  audio push, file access, options).
- **All large allocations via `host->mem_alloc`.** The frontend guarantees
  ~17MB on a PSP-1000 (more on later models) and reclaims everything
  wholesale on unload — never free-on-exit yourself.
- `run_frame` advances exactly one video frame and pushes that frame's
  audio through `host->audio_push` (declare your rate once with
  `audio_set_rate`).
- `get_frame` returns a borrowed RGB565 (preferred) frame descriptor with
  a byte pitch; the frontend uploads and scales it.
- Save states must be self-contained; SRAM is exposed as a live pointer
  plus a `sram_dirty` poll so the frontend can autosave.
- The boundary is C. Internals can be C++ (the PRX links libstdc++
  statically), but keep exceptions/RTTI off.

## PRX module boilerplate

```c
#include <pspkernel.h>
PSP_MODULE_INFO("rs_core_mycore", 0, 1, 0);

int module_start(SceSize args, void* argp) {
    if (args >= sizeof(void*) && argp) {
        const RSCoreAPI** slot = *(const RSCoreAPI***)argp;
        if (slot) *slot = rs_get_core_api();
    }
    return 0;
}
int module_stop(SceSize args, void* argp) { return 0; }
```

Production is PRX-only. Do not add a static-core fallback: every libretro
core exports the same global `retro_*` symbol set.

## Wiring it up

1. `cores/CMakeLists.txt`: `add_subdirectory(mycore)`.
2. Build. PRX mode drops `build/cores/mycore.prx` and `mycore.json` —
   install both to `ms0:/RETROSUITE/cores/`.
3. Add the exact upstream commit, retained-source hash, license, patches,
   flags, and expected PRX hash to `core-provenance.lock.json`.

That's the whole wiring. There is no frontend source to touch — the
registry (`src/frontend/core_registry.cpp`) reads your manifest, and the
launch UI resolves cores per game: it remembers which core a game was last
run with (save states are core-specific). Highlight a game and press Square
to open the picker when a system has two or more installed cores.

## Community packages

`./build.sh candidates` creates an experimental all-cores bundle plus one
`dist/cores/<name>-<version>.rscore.zip` file per candidate. An `.rscore.zip`
is a deterministic, drag-and-drop archive containing:

- the PRX and manifest under `RETROSUITE/cores/`;
- the upstream license;
- pinned provenance and artifact-hash metadata;
- an installation and native-code trust warning.

Extract the archive at the root of the Memory Stick. A `testOnly` package
is intentionally invisible to a normal production EBOOT, so testers must
install the candidate EBOOT from `RetroSuite-PSP-Candidates.zip` first.
Core packages are native PSP executables, not sandboxed themes or data:
community distribution should publish source, the exact commit and patches,
and a reproducible artifact hash. RetroSuite does not treat an unsigned
third-party package as trusted.

## Porting a libretro core

Prefer mature engines over rewrites. A libretro core needs no hand-written
adapter — `cores/shared/rs_libretro_shim.cpp` already translates the whole
libretro API onto `RSCoreAPI` (`retro_run` → `run_frame`, `retro_serialize`
→ `state_save`, the video/audio/input callbacks, and SRAM). Copy
`cores/gambatte/` and change:

- the upstream source list and include dirs in `CMakeLists.txt` (mirror the
  emulator's own `Makefile.common`);
- `RS_CORE_NAME` / `RS_CORE_SYSTEMS` compile definitions;
- the pixel-format define the core wants (`VIDEO_RGB565`, etc.);
- `manifest.json`.

The shim also supplies the C/C++ runtime a bare PRX lacks (an arena-backed
`malloc`, static-constructor init, single-threaded `__cxa_guard_*`), so C++
cores link cleanly with `-nostartfiles`. Upstream keeps its own license —
vendor it under `cores/<name>/upstream/` with its `COPYING` intact
(gambatte is GPLv2; RetroSuite itself is MIT).

Budget checklist for the PSP-1000:

- text+data of your PRX + working RAM + ROM must fit ~17MB
- large GBA ROMs need streaming via `host->file_read` (gpSP already
  knows how)
- 333MHz MIPS: measure with the FPS overlay before optimizing blindly
