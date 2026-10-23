## What changed

Describe the core, problem, and focused change.

## Provenance and licensing

- Upstream repository:
- Full upstream commit:
- License and retained license path:
- Local patches/build flags:

## Validation

- [ ] `python3 tools/core_lab.py audit CORE`
- [ ] Core builds with `./build.sh core CORE`
- [ ] `.rscore.zip` created with `./build.sh core-package CORE`
- [ ] PPSSPP model 0 correctness report attached
- [ ] PPSSPP model 1 correctness report attached when memory-sensitive
- [ ] Real PSP log/report attached, or hardware testing is explicitly requested
- [ ] Save state, SRAM, relaunch, video, and audio behavior checked

## Before/after evidence

Include comparable workload, PSP model, speed, p95 frame cost, arena use,
audio drops/underruns, and any compatibility change.

## Safety and content

- [ ] No ROMs, BIOS files, keys, firmware, or proprietary SDK material added
- [ ] New core remains `testOnly: true` and `psp1000Safe: false` until qualified
- [ ] Known regressions and untested workloads are listed below

## Known limitations

List missing hardware coverage, compatibility gaps, or follow-up work.
