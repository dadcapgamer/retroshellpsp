# Adding an emulator core to RetroSuite

A core is one directory under `cores/` with three things:

```
cores/mycore/
├── CMakeLists.txt      rs_add_core(mycore SOURCES …)
├── exports.exp         PRX export table (copy dummy's verbatim)
└── …sources…           the emulator + a thin adapter
```

Study `cores/dummy/` first — it is a complete, working core in ~250 lines.

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
#ifndef RS_STATIC_BUILD
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
#endif
```

Guard it with `RS_STATIC_BUILD` — in static mode the build renames your
`rs_get_core_api` (via a compile definition) and registers it
automatically; module boilerplate must disappear.

## Wiring it up

1. `cores/CMakeLists.txt`: `add_subdirectory(mycore)` (before
   `rs_finalize_static_cores()`).
2. Map systems to your core in `CoreManager::coreForSystem`
   (`src/frontend/core_manager.cpp`).
3. Build. PRX mode drops `build/cores/mycore.prx` — install it to
   `ms0:/RETROSUITE/cores/`. Verify the static build too:
   `./build.sh static`.

## Porting an existing emulator

Prefer mature engines over rewrites. Most target emulators (gambatte,
gpSP, PicoDrive, SMS Plus GX, FCEUmm, Snes9x 2005) expose the libretro
API and already build on PSP — vendor the upstream source, build its
files in your `rs_add_core` call, and write a small adapter translating
`RSCoreAPI` calls onto the engine (or onto its `libretro.h`, which maps
almost 1:1: `retro_run` ↔ `run_frame`, `retro_serialize` ↔ `state_save`,
video/audio callbacks ↔ `get_frame` / `audio_push`).

Budget checklist for the PSP-1000:

- text+data of your PRX + working RAM + ROM must fit ~17MB
- large GBA ROMs need streaming via `host->file_read` (gpSP already
  knows how)
- 333MHz MIPS: measure with the FPS overlay before optimizing blindly
