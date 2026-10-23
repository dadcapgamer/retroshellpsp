# Core contributor workflow

This guide is for people improving emulator compatibility, speed, memory use,
audio, or battery-save behavior. Core work is intentionally separate from the
RetroShell interface: most contributors only touch one directory under
`cores/` and use Core Lab to run the surrounding machinery.

## Choose a path

### Optimize an existing core

Choose this path for compiler tuning, a newer upstream revision, cache or
memory changes, hot-loop optimization, compatibility fixes, or improvements
to an existing libretro integration.

1. Create a branch and record an unmodified baseline.
2. Make one measurable change under `cores/<name>/` or, when necessary, the
   shared libretro shim.
3. Build and smoke-test only the affected core.
4. Compare logs on the same workload and PSP model.
5. Submit the exact source, provenance, and evidence.

Shared-shim changes affect every libretro core and require the all-core
regression matrix before release, even when the original goal concerns one
core.

### Port a new core

Prefer an established libretro core. RetroShell's shared shim already bridges
video, audio, input, save states, SRAM, RTC, core options, and read-only VFS
access. Use `cores/gambatte/` as the integration example and
`docs/ADDING_A_CORE.md` as the API reference.

A new core begins with:

- `testOnly: true`;
- `psp1000Safe: false`;
- an upstream repository and full commit ID;
- a retained upstream license;
- a reproducible retained-source tree;
- explicit compiler flags and local-change descriptions.

Do not make a new core the default based only on a benchmark. Compatibility,
save integrity, stability, and hardware results matter too.

## Set up once

Install PSPDEV and set `PSPDEV` if it is not at `~/pspdev`. Core Lab reports
missing required tools and treats PPSSPP as optional until the smoke-test step:

```sh
python3 tools/core_lab.py doctor
```

Every mutating Core Lab command supports a preview by placing `--dry-run`
before the command:

```sh
python3 tools/core_lab.py --dry-run build gambatte
```

## Build one core

```sh
./build.sh core gambatte
```

This configures `build-core-lab/` when needed and builds only the selected PRX
target. The equivalent long form is:

```sh
python3 tools/core_lab.py build gambatte
```

Use a separate build directory when comparing configurations:

```sh
python3 tools/core_lab.py build gambatte --build-dir build-gambatte-baseline
```

## Audit policy and provenance

```sh
python3 tools/core_lab.py audit gambatte
```

The audit delegates to RetroShell's authoritative repository checker. It
validates all manifests, delivery classifications, default priorities,
retained-source hashes, full upstream commits, licenses, core API/package
version synchronization, and expected artifacts when a release build is
provided.

An optimization normally changes the retained-source or PRX hash. Update only
the selected entry in `core-provenance.lock.json`, including:

- the exact upstream commit;
- the retained source path and its tree hash;
- every local patch or integration change;
- effective compiler flags;
- the PRX SHA-256 produced by the project toolchain;
- the upstream license file.

Keep `core-catalog.json` user-facing: status and PSP model claims must match
the evidence. Never promote a core by changing labels alone.

## Create one testing package

```sh
./build.sh core-package gambatte
```

Core Lab builds the target and calls the same deterministic packager used for
official `.rscore.zip` files. Output goes to `dist/core-directory/` by default.
Archived cores are refused. Maintainers investigating a historical regression
may explicitly use `--allow-archived`; such packages remain local testing
artifacts and are not release candidates.

The package contains native PSP code. Back up saves and install testing
packages only on a device you control.

## Run the PPSSPP correctness gate

Use a ROM you own or a redistributable homebrew/test ROM:

```sh
./build.sh core-test gambatte --rom /path/to/test.gbc
```

For a multi-system core, select the intended system when necessary:

```sh
python3 tools/core_lab.py smoke picodrive \
  --system sms --rom /path/to/test.sms --psp-model 0
```

Run both memory profiles for memory-sensitive work:

```sh
python3 tools/core_lab.py smoke gpsp --rom /path/to/test.gba --psp-model 0
python3 tools/core_lab.py smoke gpsp --rom /path/to/test.gba --psp-model 1
```

Model 0 represents a 32 MB PSP-1000; model 1 represents later 64 MB models.
The automation checks package/core loading, relaunch, video, audio, save/load,
return to the frontend, allocation failures, and clean exit.

PPSSPP is a correctness gate. It is not acceptable evidence for real-time
performance, audio stability, suspend behavior, or PSP-1000 safety.

## Generate a pull-request report

After a smoke test:

```sh
python3 tools/core_lab.py report gambatte
```

The latest matching result becomes `dist/core-lab/gambatte-report.md`. To use
a specific run:

```sh
python3 tools/core_lab.py report gambatte --result /path/to/result.json
```

Attach the report to the pull request. It includes the ROM hash, never the ROM
itself.

## Qualify on real PSP hardware

PPSSPP success permits review and broader testing; it does not qualify a core
for release. On a real PSP, use the same build and workload, enable 333 MHz
and the FPS overlay, exercise save/load and SRAM, exit cleanly, then grade the
log:

```sh
./build.sh qualify /path/to/retroshell.log
```

The current hardware gate checks median emulation speed, p95 frame cost,
arena high-water use, allocation failures, sustained audio starvation,
dropped audio frames, and video health. A PSP-1000 pass is required before
setting `psp1000Safe: true`. See `docs/REAL_PSP_ALL_CORE_TEST.md` when a shared
change requires the complete matrix.

You may submit a promising optimization without owning every PSP model. State
which hardware evidence is missing so a maintainer or community tester can
complete it. Missing hardware evidence blocks promotion, not collaboration.

## Licensing and content policy

- Do not commit or upload ROMs, BIOS files, keys, firmware, or proprietary SDK
  material.
- Test commands accept local paths specifically so protected content remains
  outside the repository.
- Retain upstream copyright and license files. RetroShell's MIT license does
  not replace a core's license.
- Identify the exact upstream commit and describe every local change.
- If redistribution is restricted, discuss it before adding vendored source or
  release artifacts.
- Copyleft cores must ship the corresponding license and have reviewable source
  available for the distributed binary.

## Native-code boundary

PRX cores are not sandboxed. The intended contract is deliberately narrow:

- use `RSHostAPI` for memory, audio, input, files, logging, and options;
- do not call PSP display/audio hardware directly;
- do not allocate large buffers outside the host arena;
- treat host file access as read-only;
- return borrowed frames in a supported format;
- leave save persistence to RetroShell.

Archive validation prevents malformed layouts and mismatched descriptors, but
it cannot make untrusted executable code safe. Only reviewed source built
through the project workflow should enter the official core directory.

## Pull-request checklist

Include:

- core and supported system;
- problem and expected improvement;
- upstream repository and full commit;
- license and retained license path;
- source/build flag changes;
- Core Lab report for each tested PSP memory model;
- before/after real-hardware evidence when performance is claimed;
- known regressions or untested workloads;
- confirmation that no ROM, BIOS, keys, or proprietary SDK content is present.
