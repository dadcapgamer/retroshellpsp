# FrogGBA RetroShell adapter

This retained engine is the RetroShell/libretro adaptation of FrogGBA,
upstream commit `9d75721fe8e3df23db4148f49e92df2bafe6dd1a`.

FrogGBA descends from TempGBA and gpSP Kai. RetroShell keeps the libretro
adapter needed for a loadable PRX, while applying FrogGBA's PSP-oriented
cache, compiler, audio-ring, and rendering choices. Direct ownership of the
PSP display, GU, audio device, file browser, and suspend callbacks is omitted:
those services remain owned by RetroShell so the module can unload safely.

The original lineage README, notice, and GPL-2.0 license are retained beside
this file. RetroShell-specific changes are recorded in the repository's
`core-provenance.lock.json`.
