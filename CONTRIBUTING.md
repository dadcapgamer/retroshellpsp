# Contributing to RetroShell

Thank you for helping RetroShell work better on real PSP hardware.

The fastest place to contribute is an emulator core. You do not need to
understand the frontend to optimize an existing core or propose a new one.
Start with [Core contributor workflow](docs/CORE_CONTRIBUTING.md), then check
your setup:

```sh
python3 tools/core_lab.py doctor
```

Core Lab can build, audit, package, smoke-test, and report on one core at a
time. Run `python3 tools/core_lab.py --help` for the command list.

For other changes, build RetroShell with `./build.sh` and run the complete
host suite with `./build.sh test` before opening a pull request.

## Content and safety rules

- Do not commit or upload commercial ROMs, BIOS files, encryption keys, or
  proprietary SDK files.
- Emulator source keeps its upstream license. Include the exact upstream
  repository, full commit, retained license, local changes, and build flags.
- A core package contains native PSP code. Passing automated checks does not
  sandbox it or make an unknown package trustworthy.
- PPSSPP checks correctness. Performance and PSP-1000 safety claims require
  testing on real hardware.
- New integrations begin as testing cores and must not claim PSP-1000 safety
  before hardware qualification.

Use the repository's core optimization or new-core issue form before a large
port so maintainers and hardware testers can coordinate with you.
