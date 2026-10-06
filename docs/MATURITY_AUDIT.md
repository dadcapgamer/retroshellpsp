# Maturity audit

Status of RetroShell against `RetroShell_Mature_Frontend_Foundations.md`.
Verified = PPSSPP screenshots (dark and light) and host tests; real-PSP
testing is still pending.

| Section | Status |
|---|---|
| 1 Interaction model | Done. X plays from the Library and Continue; Game Details is the second row of Options. Select opens the View menu. |
| 2 Systems / 3 Continue / 4 Library | Done. Per-system selection, filter and sort remembered. |
| 5 Resilience | Duplicates, regions, revisions, hacks, translations, homebrew and discs disambiguated; tested with 2,000+ games. |
| 6 Artwork fallback | Artwork, then screenshot, then system placeholder, then text-only. |
| 7 Metadata cleanup | `database/title_clean.h`; unknown fields omitted. |
| 8 Persistence | Last system/layer/Continue persist across restarts; play time added. |
| 9 Launch | Per-system default and per-game override. BIOS preflight only for FrogGBA and native adapters. |
| 10 Detail | Play time, last played, version, file name. Cheats / Manual are not offered until a data source exists. |
| 11 PSP hardware | Battery/clock, low-battery state. Sleep/wake unchanged. |
| 12 Performance | Cached index, hashed play history; 2,034-game library navigates normally in PPSSPP. Hardware timing unmeasured. |
| 13 Errors | See `launch_notice.h`; shared `ui/state_panel`. |
| 14 First run | `SetupScene`. |
| 15 Search/filters | View menu; letter search instead of a keyboard. |
| 16-20 Visuals, legend, motion | Visual polish pass (`RetroShell_Modern_PSP_Visual_Polish_Guide.md`): navy default, two type roles, one header/footer, one focus language, branded fallback, tabbed Settings. |
| 21 Primitives | `ui/chrome` (header/footer grid, focus, panels, menu rows, chips, art well and fallback), state panel and text layout shared by every scene. |
