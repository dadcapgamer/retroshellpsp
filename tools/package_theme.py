#!/usr/bin/env python3
"""Create a deterministic drag-and-drop RetroShell theme package."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EPOCH = (1980, 1, 1, 0, 0, 0)


def add_file(archive: zipfile.ZipFile, source: Path, destination: str) -> None:
    info = zipfile.ZipInfo(destination, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, source.read_bytes(), compresslevel=9)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("theme_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    theme_dir = args.theme_dir.resolve()
    manifest_path = theme_dir / "theme.json"
    if not manifest_path.is_file():
        parser.error("theme directory must contain theme.json")
    manifest = json.loads(manifest_path.read_text())
    theme_id = manifest.get("id", theme_dir.name)
    if not isinstance(theme_id, str) or not re.fullmatch(
            r"[A-Za-z0-9_-]{1,48}", theme_id):
        parser.error("theme id must contain only letters, numbers, _ or -")
    if theme_id != theme_dir.name:
        parser.error("theme id must match its directory name")

    output = args.output or ROOT / "dist" / f"RetroShell-Theme-{theme_id}.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    files = sorted(path for path in theme_dir.rglob("*") if path.is_file())
    if not files:
        parser.error("theme directory is empty")
    with zipfile.ZipFile(output, "w") as archive:
        for source in files:
            relative = source.relative_to(theme_dir).as_posix()
            add_file(archive, source,
                     f"RETROSHELL/themes/{theme_id}/{relative}")
    print(output)


if __name__ == "__main__":
    main()
