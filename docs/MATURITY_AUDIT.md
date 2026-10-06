# Maturity audit

Status of RetroShell against `RetroShell_Mature_Frontend_Foundations.md`.
Verified = PPSSPP screenshots (dark and light) and host tests; real-PSP
testing is still pending.

| Section | Status |
|---|---|
| 1 Interaction model | Done. Deviation: in a Library X opens Details (Play is its first row), by request. Select opens the View menu. |
| 2 Systems / 3 Continue / 4 Library | Done. Per-system selection, filter and sort remembered. |
| 5 Resilience | Duplicates, regions, revisions, hacks, translations, homebrew and discs disambiguated; tested with 2,000+ games. |
| 6 Artwork fallback | Artwork, then screenshot, then system placeholder, then text-only. |
| 7 Metadata cleanup | `database/title_clean.h`; unknown fields omitted. |
| 8 Persistence | Last system/layer/Continue persist across restarts; play time added. |
| 9 Launch | Per-system default and per-game override. BIOS preflight only for FrogGBA and native adapters. |
| 10 Detail | Play time, last played, version, file name. Cheats / Manual rows still say "not available" (no data exists). |
| 11 PSP hardware | Battery/clock, low-battery state. Sleep/wake unchanged. |
| 12 Performance | Cached index, hashed play history; 2,034-game library navigates normally in PPSSPP. Hardware timing unmeasured. |
| 13 Errors | See `launch_notice.h`; shared `ui/state_panel`. |
| 14 First run | `SetupScene`. |
| 15 Search/filters | View menu; letter search instead of a keyboard. |
| 16-20 Visuals, legend, motion | Astra baseline; legend now includes Select/Back in Library. |
| 21 Primitives | State panel and text layout shared. System rail, game list and menus remain inline in `HomeScene`. |
