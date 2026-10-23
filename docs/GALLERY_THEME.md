# Gallery themes

RetroShell can discover external themes from the Memory Stick. This keeps
large artwork and separately licensed community packages outside the main
application archive and lets new themes be installed without rebuilding
RetroShell.

## Install layout

Each theme lives in a directory whose name exactly matches its manifest ID:

```text
ms0:/RETROSHELL/themes/art-book-next/theme.json
ms0:/RETROSHELL/themes/art-book-next/system-art/art-game-boy.png
ms0:/RETROSHELL/themes/art-book-next/logos/logo-game-boy.png
```

Restart RetroShell after installing a package, then choose it under
**Settings → Theme**. Deleting that one theme directory removes it without
changing RetroShell or the user's library.

## Manifest

The Gallery-specific fields are optional additions to the normal palette
manifest:

```json
{
  "formatVersion": 1,
  "id": "example-gallery",
  "name": "Example Gallery",
  "dark": true,
  "presentation": "gallery",
  "waves": false,
  "systemArt": {
    "gb": "system-art/game-boy.png",
    "gba": "system-art/game-boy-advance.png"
  },
  "systemLogos": {
    "gb": "logos/game-boy.png",
    "gba": "logos/game-boy-advance.png"
  }
}
```

Supported keys are `gb`, `gbc`, `gba`, `nes`, `snes`, `md`, `sms`, `gg`, and
`pce`. Missing artwork or logos fall back to RetroShell's built-in system icon
and title. PNG dimensions may be at most 512×512; transparent PNGs are
supported. Theme IDs may contain letters, numbers, `_`, and `-` only. Asset
paths must be relative and remain inside the theme directory.

The standard `colors`, `background`, and `waves` fields remain available. A
Gallery theme gets a neutral black and charcoal base palette before any color
overrides are applied.

## Build a package

From the source tree, run:

```sh
python3 tools/package_theme.py theme-packs/art-book-next
```

The deterministic ZIP is written under `dist/` and contains the complete
`RETROSHELL/themes/<theme-id>/` install path. The main RetroShell release ZIP
does not include files from `theme-packs/`.

Every add-on must include its own license and attribution. An external theme
is not automatically covered by RetroShell's MIT license.
