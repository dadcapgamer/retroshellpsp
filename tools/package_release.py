#!/usr/bin/env python3
"""Create a byte-for-byte reproducible PSP release archive."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EPOCH = (1980, 1, 1, 0, 0, 0)


def add_file(archive: zipfile.ZipFile, source: Path, destination: str) -> None:
    add_bytes(archive, source.read_bytes(), destination)


def add_bytes(archive: zipfile.ZipFile, data: bytes, destination: str) -> None:
    info = zipfile.ZipInfo(destination, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data, compresslevel=9)


def package_core(build: Path, output_dir: Path, core: dict) -> Path:
    name = core["name"]
    manifest_path = ROOT / f"cores/{name}/manifest.json"
    manifest = json.loads(manifest_path.read_text())
    output = output_dir / f"{name}-{manifest['version']}.rscore.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    provenance = {
        "formatVersion": 1,
        "target": "mipsel-sony-psp",
        "core": core,
    }
    readme = (
        f"RetroSuite experimental core: {name}\n\n"
        "Extract this archive at the root of the PSP Memory Stick.\n"
        "Experimental/testOnly cores require a RetroSuite candidate EBOOT.\n"
        "Native PRX files execute with the application's permissions; only "
        "install packages from a source you trust.\n"
    ).encode()
    with zipfile.ZipFile(output, "w") as archive:
        add_file(archive, build / f"cores/{name}.prx",
                 f"RETROSUITE/cores/{name}.prx")
        add_file(archive, manifest_path, f"RETROSUITE/cores/{name}.json")
        add_file(archive, ROOT / core["licenseFile"],
                 f"RETROSUITE/licenses/{name}-{Path(core['licenseFile']).name}")
        add_bytes(archive,
                  json.dumps(provenance, indent=2, sort_keys=True).encode() + b"\n",
                  f"RETROSUITE/core-packages/{name}/provenance.json")
        add_bytes(archive, readme,
                  f"RETROSUITE/core-packages/{name}/README.txt")
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--include-candidates", action="store_true")
    parser.add_argument("--core-packages", action="store_true",
                        help="also create individual drag-and-drop .rscore.zip packages")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    lock = json.loads((ROOT / "core-provenance.lock.json").read_text())
    output = args.output
    if output is None:
        filename = ("RetroSuite-PSP-Candidates.zip"
                    if args.include_candidates else "RetroSuite-PSP.zip")
        output = ROOT / "dist" / filename
    output.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output, "w") as archive:
        add_file(archive, build / "src/EBOOT.PBP", "PSP/GAME/RetroSuite/EBOOT.PBP")
        selected = [
            core for core in lock["cores"]
            if core.get("delivery") == "production" or
            (args.include_candidates and core.get("delivery") == "candidate")
        ]
        for core in sorted(selected, key=lambda value: value["name"]):
            name = core["name"]
            add_file(archive, build / f"cores/{name}.prx", f"RETROSUITE/cores/{name}.prx")
            add_file(archive, ROOT / f"cores/{name}/manifest.json",
                     f"RETROSUITE/cores/{name}.json")
            add_file(archive, ROOT / core["licenseFile"],
                     f"RETROSUITE/licenses/{name}-{Path(core['licenseFile']).name}")
        add_file(archive, ROOT / "core-provenance.lock.json",
                 "RETROSUITE/core-provenance.lock.json")
        add_file(archive, ROOT / "THIRD_PARTY_NOTICES.md",
                 "RETROSUITE/THIRD_PARTY_NOTICES.md")
    print(output)

    if args.core_packages:
        candidates = [
            core for core in lock["cores"]
            if core.get("delivery") == "candidate"
        ]
        for core in sorted(candidates, key=lambda value: value["name"]):
            print(package_core(build, ROOT / "dist/cores", core))


if __name__ == "__main__":
    main()
