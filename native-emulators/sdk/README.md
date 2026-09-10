# RetroShell native adapter SDK

Protocol v1 lets a standalone PSP emulator participate in RetroShell without
running the frontend beside the emulator.

- `argv[1]`: ROM path
- `argv[2]`: RetroShell EBOOT path
- `argv[3]`: bounded session receipt path
- pause chord: L+R+Select
- return: custom-firmware `loadexec` back to `argv[2]`

Copy `retroshell_adapter.c`, `retroshell_adapter.h`, and the canonical
`src/core_api/rs_pause_menu.h` to the emulator source tree. Add
`retroshell_adapter.c` to the emulator build, initialize one static
`RSAdapterContext`, and check `rs_adapter_pause_requested()` before the
emulator's other shortcut combinations. The first adapter menu must use the
shared `RSPauseMenuItem` order. Only `Emulator Settings` should enter the
emulator's original settings UI. On pause, record `PAUSED`, invoke the shared
menu, wait for the chord to be released, then record `RESUMED`. Its exit action
should call `rs_adapter_return(..., RETURNED)` after saves and audio/video
teardown.

The SDK does not emulate, draw, allocate during gameplay, or keep RetroShell
resident. An adapter renders the shared menu with its own already-loaded PSP
drawing code so the emulator retains all available gameplay memory.
