#!/usr/bin/env python3
"""Create a byte-for-byte reproducible PSP release archive."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EPOCH = (1980, 1, 1, 0, 0, 0)
VERSION = (ROOT / "RELEASE_VERSION").read_text().strip()
CATALOG = json.loads((ROOT / "core-catalog.json").read_text())["cores"]
CORE_API_VERSION = 4


def native_adapters() -> dict[str, tuple[dict, Path]]:
    adapters: dict[str, tuple[dict, Path]] = {}
    for manifest_path in sorted((ROOT / "native-emulators").glob("*/manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        name = manifest["name"]
        package = (ROOT / "dist/core-directory" /
                   f"{name}-{manifest['version']}.rscore.zip")
        adapters[name] = (manifest, package)
    return adapters


def native_package_descriptor(package: Path, name: str) -> dict:
    with zipfile.ZipFile(package) as archive:
        descriptor = json.loads(archive.read(
            f"RETROSHELL/core-packages/{name}/package.json"))
    descriptor["file"] = package.name
    descriptor["packageSha256"] = sha256(package)
    descriptor["bytes"] = package.stat().st_size
    return descriptor


def add_file(archive: zipfile.ZipFile, source: Path, destination: str) -> None:
    add_bytes(archive, source.read_bytes(), destination)


def add_bytes(archive: zipfile.ZipFile, data: bytes, destination: str) -> None:
    info = zipfile.ZipInfo(destination, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data, compresslevel=9)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_core(build: Path, output_dir: Path, core: dict) -> tuple[Path, dict]:
    name = core["name"]
    manifest_path = ROOT / f"cores/{name}/manifest.json"
    manifest = json.loads(manifest_path.read_text())
    artifact_path = build / f"cores/{name}.prx"
    output = output_dir / f"{name}-{manifest['version']}.rscore.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    catalog_entry = CATALOG[name]
    status = catalog_entry["status"]
    model_support = catalog_entry["modelSupport"]
    provenance = {
        "formatVersion": 1,
        "target": "mipsel-sony-psp",
        "core": core,
    }
    descriptor = {
        "formatVersion": 3,
        "kind": "retroshell-core",
        "coreApiVersion": CORE_API_VERSION,
        "name": name,
        "displayName": catalog_entry["displayName"],
        "version": manifest["version"],
        "systems": manifest["systems"].split("|"),
        "status": status,
        "notes": catalog_entry["notes"],
        "modelSupport": model_support,
        "psp1000Safe": manifest["psp1000Safe"],
        "testOnly": manifest["testOnly"],
        "artifactSha256": sha256(artifact_path),
    }
    readme = (
        f"RetroShell core: {name} {manifest['version']}\n\n"
        "Copy this unopened .rscore.zip into RETROSHELL/cores/ on the PSP "
        "Memory Stick, then restart RetroShell.\n"
        "Testing cores require a RetroShell beta/testing EBOOT.\n"
        "Native PRX files execute with the application's permissions; only "
        "install packages from a source you trust.\n"
    ).encode()
    if name == "froggba":
        readme += (
            "\nFrogGBA requires a legally obtained GBA BIOS copied to "
            "RETROSHELL/system/gba_bios.bin. The BIOS is not included.\n"
        ).encode()
    with zipfile.ZipFile(output, "w") as archive:
        add_file(archive, artifact_path,
                 f"RETROSHELL/cores/{name}.prx")
        add_file(archive, manifest_path, f"RETROSHELL/cores/{name}.json")
        add_file(archive, ROOT / core["licenseFile"],
                 f"RETROSHELL/licenses/{name}-{Path(core['licenseFile']).name}")
        add_bytes(archive,
                  json.dumps(provenance, indent=2, sort_keys=True).encode() + b"\n",
                  f"RETROSHELL/core-packages/{name}/provenance.json")
        add_bytes(archive, readme,
                  f"RETROSHELL/core-packages/{name}/README.txt")
        add_bytes(archive,
                  json.dumps(descriptor, indent=2, sort_keys=True).encode() + b"\n",
                  f"RETROSHELL/core-packages/{name}/package.json")
    descriptor["file"] = output.name
    descriptor["packageSha256"] = sha256(output)
    descriptor["bytes"] = output.stat().st_size
    return output, descriptor


def write_core_directory(build: Path, cores: list[dict], all_cores: list[dict]) -> list[Path]:
    output_dir = ROOT / "dist/core-directory"
    output_dir.mkdir(parents=True, exist_ok=True)
    adapters = native_adapters()
    available_native = {
        name for name, (_, package) in adapters.items() if package.is_file()
    }
    preserved_native = {package.name for _, package in adapters.values()
                        if package.is_file()}
    # Do not leave obsolete packages behind when a core is removed or its
    # version changes. Native packages created by the adapter pipeline are
    # preserved only when their filename matches the current manifest.
    for stale in output_dir.glob("*.rscore.zip"):
        if stale.name not in preserved_native:
            stale.unlink()
    packages: list[Path] = []
    catalog: list[dict] = []
    for core in sorted(cores, key=lambda value: value["name"]):
        package, descriptor = package_core(build, output_dir, core)
        packages.append(package)
        catalog.append(descriptor)
    for name, (_, package) in adapters.items():
        if package.is_file():
            packages.append(package)
            catalog = [item for item in catalog if item["name"] != name]
            catalog.append(native_package_descriptor(package, name))
    catalog.sort(key=lambda value: value["name"])
    index = {
        "formatVersion": 1,
        "retroShellVersion": VERSION,
        "cores": catalog,
        "unavailable": [
            {
                "name": core["name"],
                **CATALOG[core["name"]],
            }
            for core in sorted(all_cores, key=lambda value: value["name"])
            if (core.get("delivery") == "archived" and
                core["name"] not in available_native)
        ],
    }
    (output_dir / "index.json").write_text(
        json.dumps(index, indent=2, sort_keys=True) + "\n")
    lines = [
        "# RetroShell core directory",
        "",
        "Download a `.rscore.zip`, copy the unopened ZIP into `RETROSHELL/cores/`",
        "on the PSP Memory Stick, and restart RetroShell.",
        "",
        "| Core | Systems | Status | PSP models | Package |",
        "| --- | --- | --- | --- | --- |",
    ]
    for item in catalog:
        lines.append(
            f"| {item['displayName']} | {', '.join(item['systems'])} | "
            f"{item['status']} | {item['modelSupport']} | "
            f"[{item['file']}]({item['file']}) |"
        )
    lines.extend([
        "",
        "## Not offered",
        "",
        "These integrations remain in source history for audit purposes but",
        "are not downloadable because they failed a PSP safety or performance gate.",
        "",
        "| Core | Reason |",
        "| --- | --- |",
    ])
    for core in sorted(all_cores, key=lambda value: value["name"]):
        if (core.get("delivery") == "archived" and
                core["name"] not in available_native):
            item = CATALOG[core["name"]]
            lines.append(f"| {item['displayName']} | {item['notes']} |")
    lines.extend([
        "",
        "Core packages execute native code. Install packages only from a",
        "source you trust. Games and BIOS files are not included.",
        "",
    ])
    (output_dir / "README.md").write_text("\n".join(lines))
    return packages


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--include-candidates", action="store_true")
    parser.add_argument("--core-packages", action="store_true",
                        help="also create individual drag-and-drop .rscore.zip packages")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if args.include_candidates:
        cache = build / "CMakeCache.txt"
        if (not cache.is_file() or
                "RS_INCLUDE_TEST_CORES:BOOL=ON" not in cache.read_text(
                    errors="replace")):
            raise SystemExit(
                "candidate packaging requires a build configured with "
                "-DRS_INCLUDE_TEST_CORES=ON")
    lock = json.loads((ROOT / "core-provenance.lock.json").read_text())
    output = args.output
    if output is None:
        filename = f"RetroShell-PSP-v{VERSION}.zip"
        output = ROOT / "dist" / filename
    output.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output, "w") as archive:
        add_file(archive, build / "src/EBOOT.PBP",
                 "PSP/GAME/RetroShell/EBOOT.PBP")
        selected = [
            core for core in lock["cores"]
            if core.get("delivery") == "production" or
            (args.include_candidates and core.get("delivery") == "candidate")
        ]
        for core in sorted(selected, key=lambda value: value["name"]):
            name = core["name"]
            add_file(archive, build / f"cores/{name}.prx", f"RETROSHELL/cores/{name}.prx")
            add_file(archive, ROOT / f"cores/{name}/manifest.json",
                     f"RETROSHELL/cores/{name}.json")
            add_file(archive, ROOT / core["licenseFile"],
                     f"RETROSHELL/licenses/{name}-{Path(core['licenseFile']).name}")
        add_file(archive, ROOT / "core-provenance.lock.json",
                 "RETROSHELL/core-provenance.lock.json")
        add_file(archive, ROOT / "RELEASE_VERSION", "RETROSHELL/VERSION")
        add_file(archive, ROOT / "LICENSE", "RETROSHELL/LICENSE")
        add_file(archive, ROOT / "docs/INSTALL.md", "INSTALL.md")
        add_bytes(archive,
                  b"Optional emulator BIOS files belong in this directory.\n"
                  b"FrogGBA expects gba_bios.bin. BIOS files are not included.\n",
                  "RETROSHELL/system/README.txt")
        add_file(archive, ROOT / "THIRD_PARTY_NOTICES.md",
                 "RETROSHELL/THIRD_PARTY_NOTICES.md")
        add_file(archive, ROOT / "assets/fonts/OFL-Geist.txt",
                 "RETROSHELL/licenses/Geist-Pixel-OFL-1.1.txt")
    print(output)

    if args.core_packages:
        available = [
            core for core in lock["cores"]
            if core.get("delivery") in {"production", "candidate"}
        ]
        for package in write_core_directory(build, available, lock["cores"]):
            print(package)
        print(ROOT / "dist/core-directory/index.json")


if __name__ == "__main__":
    main()
