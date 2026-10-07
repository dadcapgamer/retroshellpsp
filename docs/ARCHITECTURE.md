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
by vertex color; circles are anti-aliased masks baked at boot, while shell
surfaces are integer-aligned rects with stepped (pixel-rounded) corners so
edges stay crisp on the LCD. Fonts are pre-baked `.rsf` atlases (see
`tools/assetgen.c` for the format), baked with FreeType hinting by
`tools/fontbake.c` so stems land on whole pixels. Console icons are exact integer
(alpha-weighted box) downscales of the 192 px masters (`tools/crisp_icons.py`), drawn 1:1 with
nearest sampling.

### Typography

One family, IBM Plex Mono, matching the concept board:

| Weight | Sizes | Used for |
|---|---|---|
| SemiBold | 18, 15, 13 | system name and Detail title (18), wordmark and pane titles (15), focused rows and card titles (13) |
| Regular | 13, 11, 10 | rows, actions and settings (13), metadata, status and counts (11), chips, labels and legend (10) |

Uppercase labels (wordmark, system name, pane titles) are drawn tracked via
`Font::draw`'s `tracking` argument rather than baked as separate atlases.

`Font::centerY` centres a label on its cap height (measured from 'H' at
load), which is how every row, chip and button places text on whole pixels.
Every atlas is embedded in the EBOOT and therefore comes out of the core
arena (startup gate: 17,000 KB), so a size exists only when a role needs it.

### Shared chrome (`ui/chrome.*`)

One grid — 16px margins, header 0–27, content 36–238, footer hairline at
246 — and one focus language in the accent colour: `focusFill` (rows, menus,
settings), `focusFrame` (cards, the selected system) and `focusUnderline`.
`App::drawTopBar` (3×3 dot mark, wordmark, clock, battery, scan status) and
`App::drawHintBar` (glyph+label groups in equal columns) are the only header
and footer. `ui/icons` holds the 11×11 1-bit action and settings icons.

### Library thumbnails (`database/thumb_cache.*`)

24×24 RGB565 thumbnails, one cache file per system under
`RETROSHELL/cache/thumbs/`, built on a background worker from each cover (or
the newest screenshot) after a scan and loaded by the worker when the Library
switches system. The main thread only searches the resident set and uploads
a 1 KB texture per newly visible row, so scrolling never touches the Memory
Stick; rows without a thumbnail show the 24 px console glyph. The worker is
stopped with the scanner before a core launches. Panels, chips, menu
rows, the RetroShell mark and the artwork well/fallback live in the same
module, so a screen composes them rather than drawing its own.

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
              v Triangle on a game (X on a game plays it)
        Game Detail        art, metadata, Play / Favorite / ...
```

Horizontal always changes system, vertical moves within or between layers,
X plays (Library, Continue) or enters (Systems), O goes back, Square
favorites, Triangle opens Options (Systems: Settings) and Start returns Home.
Triangle on a game opens its Game Detail, which holds every action for it
(Play, Save States, Favorite, Emulator, Delete Save Data, Remove from
Continue) and a line of facts under the title. Continue Playing shows exactly three
cards at a time. The rail lists only systems that have games. The last game
highlighted in each system is remembered per system and persisted in
`library.json` (`lastSelected`, memory-only while browsing and flushed when
Home is left). Layer changes are vertical shifts and system changes
horizontal slides driven by `ui::Smooth`.

Anything that touches the Memory Stick (core resolution, artwork decode,
optional metadata JSON) runs once after the highlight settles
(`hydrateSelection`), never per frame. Artwork is contain-fit into a well
(`ui::artWell`); missing artwork is replaced by the branded fallback
(`ui::artFallback`): dot pattern, RetroShell mark and wordmark, system icon
and abbreviation — never an empty frame or a broken-image glyph.

## Themes

Four flat built-in themes with the same roles and contrast steps: deep navy
(Dark, the default), neutral charcoal (Graphite), warm paper (Light) and cool
daylight (Mist). One accent, which means focus and
nothing else (blue by default; Settings offers five). Tokens: `bg`, `surface`,
`surface2`, `line`, `textPrimary/Secondary/Muted/Disabled`, `accent`,
`onAccent`, `focusEdge` (derived from the accent), `danger`, `scrim`, `dim`,
`pattern`. Older theme.json keys (`bgTop`, `menuBg`, `selectBg`, `divider`,
`textDim`, `fallbackDot`, ...) still load, mapped onto the role that replaced
them.

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
