# RetroShell PSP — Architecture

## The bi-layer runtime

RetroShell treats the PSP's RAM as belonging to the *emulator*, borrowed by
the *frontend* between games. Two layers, strictly separated:

```
Layer 1 — Frontend Runtime          Layer 2 — Core Runtime
──────────────────────────          ──────────────────────
UI / scenes / themes                one emulator core at a time
library database / box art          (PRX module or static lib)
config / save manager               talks ONLY to RSHostAPI
launch orchestration                knows nothing about the UI
        │                                   ▲
        └────────── Core API (C ABI) ───────┘
             src/core_api/rs_core_api.h
             src/core_api/rs_host_api.h
```

### Launch protocol

```
browse → pick game
  App::evictForCore()        drop box art, theme assets, non-boot VRAM
  mem::shutdown()            release the arena (PRX mode only)
  CoreManager::loadCore()    sceKernelLoadModule + module_start handshake
  mem::init()                re-reserve the arena for the core
  core.initialize(host)      cores may allocate from the arena here
  ROM → arena                (ZIP entries are extracted straight in)
  load persistent SRAM + RTC → run
… play …
  atomically save persistent SRAM + RTC → unloadROM
  mem::shutdown → unloadCore → mem::init
  App::restoreAfterCore()    reload theme assets
  HomeScene restored from FrontendSnapshot (layer/system/Continue cursor);
  per-system game selection comes back from library.json `lastSelected`
```

The user perceives one seamless application; in reality the frontend
rebuilds itself around every game.

### Memory map (PSP-1000)

- ~24MB user partition
- 4MB fixed newlib heap (`PSP_HEAP_SIZE_KB(4096)`) — small allocations,
  decode scratch
- **arena** (`src/runtime/arena.cpp`): everything else (~17.5MB), reserved
  at boot via `sceKernelAllocPartitionMemory`, bump-allocated with stack
  markers. ROM images and all core memory come from here.
- 2MB VRAM: two 16-bit framebuffers (no depth buffer), then textures.
  A boot mark separates resident assets (fonts, primitive masks) from
  evictable ones (theme background, box art).

On 64MB models the same code simply sees a ~50MB arena.

### PRX loading without export tables

User-mode export lookup on the PSP is awkward, so cores publish their API
through the module-start argument instead: the frontend passes the address
of a pointer slot, `module_start` writes the core's `RSCoreAPI*` into it.
See `CoreManager::loadCore` and `cores/dummy/dummy_core.c`.

## Rendering

`src/platform/psp/gu_renderer.*` is the only file touching sceGu. 480×272,
GU_PSM_5650 double buffer, vblank-synced, GU_TRANSFORM_2D vertices.
Text and UI chrome are T8 textures with a shared alpha-ramp CLUT, tinted
by vertex color; rounded rectangles / circles are anti-aliased masks baked
at boot and 9-sliced. Fonts are pre-baked `.rsf` atlases (see
`tools/assetgen.c` for the format); the Astra shell uses Geist Pixel baked with
a fixed advance (`bake_font_ex`) at 10/12/14/16/19 px for its monospaced
firmware look, `Font::drawBold` double-strikes titles, and `ui::prim` supplies
the flat shapes (outline rect, chevrons, dot field) the layouts are built from.

Emulator frames are dynamic (unswizzled) textures updated per frame and
drawn scaled: fit / stretch / 1:1, linear or nearest, per game.

## Audio

One sceAudio channel at 44100 Hz stereo, fed by a dedicated thread from a
lock-free ring. Cores declare their native rate; a linear resampler runs
on the push path. `src/platform/psp/audio_out.*` is the single audio sink
for cores and (eventually) UI sounds.

## Library

- Scanner (worker thread) recursively walks the single `ms0:/ROMS/` tree on
  first boot, when the cache is empty, or when the user selects **Rescan
  library**. Folder names are ignored and each game is assigned to a system
  by its extension; ZIPs are identified from the central directory only
  (CRC32 comes free, nothing is decompressed at scan time).
- `GameIndex` keeps per-system sorted lists and a binary cache
  (`RETROSHELL/cache/index.bin`) so later boots show the library instantly
  without repeating a full Memory Stick scan.
- Games are keyed by the FNV-1a hash of their path (`pathHash`) for
  favorites/recents/per-game config. Box art is indexed beside the ROM using
  the same base filename, preventing coverless games from causing repeated
  Memory Stick probes while scrolling. Metadata remains keyed by filename
  under `RETROSHELL/metadata/<System>/` and is not read during normal grid
  navigation.

## Home shell

`HomeScene` is one scene with four spatial layers driven by the pure
`nav::HomeNav` model (`scenes/home_nav.h`, unit-tested):

```
        Continue Playing   recent games across systems     (above)
              ^ Up
   Systems  <  >  rail, L/R change system                  (home)
              v Down / X
        Library            vertical list, L/R switch system in place
              v X on a game / Options > Game Details
        Game Detail        art, metadata, Play / Favorite / ...
```

Horizontal always changes system, vertical moves within or between layers,
X selects, O goes back, Square favorites, Triangle opens Options (Systems:
Settings) and Start returns Home. X on a Library game opens its Game Detail
rather than launching. The rail lists only systems that have games. The last game
highlighted in each system is remembered per system and persisted in
`library.json` (`lastSelected`, memory-only while browsing and flushed when
Home is left). Layer changes are vertical shifts and system changes
horizontal slides driven by `ui::Smooth`.

Anything that touches the Memory Stick (core resolution, artwork decode,
optional metadata JSON) runs once after the highlight settles
(`hydrateSelection`), never per frame. Missing artwork is replaced by the
designed placeholder (`drawPlaceholder`): dot field, faded system icon and
abbreviation.

## Themes

Two flat built-in themes: warm ivory (light) and navy-charcoal (dark), one
accent family (cobalt by default; Settings offers eight). Tokens: `bg*`,
`text*`, `accent`, `selectBg/selectText` (selection bar), `divider`,
`railOutline`, `dim` (popup backdrop), `fallbackDot`.

`theme.json` overrides any subset of the palette (colors as `#RRGGBB` or
`#RRGGBBAA`), can supply a 480×272 background image, and can enable the
legacy wave animation (off by default; the built-ins are flat). Unset values
inherit from the built-in Light or Dark palette (`"dark": true|false`).
Built-ins need no files. Palette changes crossfade live.

## Save data

```
RETROSHELL/saves/<System>/<pathHash>/sram.bin      battery save
RETROSHELL/saves/<System>/<pathHash>/state<N>.rst  states (header +
                                                   RGB565 thumbnail +
                                                   core payload)
RETROSHELL/saves/<System>/<pathHash>/states/<core>/ native-adapter states
```

SRAM is flushed on exit and every 10s while dirty (autosave setting).
State files record the core name/version and refuse cross-core loads. Native
adapters use the same per-game directory, but keep their raw states beneath a
core-named folder because emulator state formats are not interchangeable.
Battery saves and native states created before this layout remain readable
from their legacy emulator directories and are written to the canonical
location after the next save.
