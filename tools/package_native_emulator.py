#!/usr/bin/env python3
"""Create a bounded RetroShell native-emulator drag-and-drop package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EPOCH = (1980, 1, 1, 0, 0, 0)
SAFE_NAME = re.compile(r"^[a-z0-9_-]{1,48}$")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def add_bytes(archive: zipfile.ZipFile, data: bytes, destination: str) -> None:
    info = zipfile.ZipInfo(destination, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data, compresslevel=9)


def safe_relative(value: str) -> bool:
    path = Path(value)
    return bool(value) and not path.is_absolute() and ".." not in path.parts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("name", help="adapter directory name")
    parser.add_argument("source", type=Path,
                        help="built upstream emulator directory")
    parser.add_argument("--output-dir", type=Path,
                        default=ROOT / "dist/core-directory")
    args = parser.parse_args()

    if not SAFE_NAME.fullmatch(args.name):
        raise SystemExit("unsafe adapter name")
    adapter_dir = ROOT / "native-emulators" / args.name
    adapter = json.loads((adapter_dir / "adapter.json").read_text())
    manifest_bytes = (adapter_dir / "manifest.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    if adapter.get("name") != args.name or manifest.get("name") != args.name:
        raise SystemExit("adapter/manifest name mismatch")
    executable = manifest.get("executable")
    expected_executable = f"emulators/{args.name}/EBOOT.PBP"
    if manifest.get("backend") != "native" or executable != expected_executable:
        raise SystemExit("native manifest executable is not canonical")
    if adapter.get("formatVersion") == 2:
        expected_contract = {
            "adapterProtocol": 1,
            "pauseMode": "embedded",
            "pauseHotkey": "L+R+SELECT",
            "returnMode": "loadexec",
        }
        if any(adapter.get(key) != value or manifest.get(key) != value
               for key, value in expected_contract.items()):
            raise SystemExit("native adapter contract mismatch")

    source = args.source.resolve()
    payload: list[tuple[Path, str, bytes]] = []
    hashes: dict[str, str] = {}
    for item in adapter.get("payload", []):
        source_relative = item.get("source", "")
        destination = item.get("destination", "")
        if not safe_relative(source_relative) or not safe_relative(destination):
            raise SystemExit("unsafe native payload path")
        artifact = source / source_relative
        if not artifact.is_file():
            raise SystemExit(f"missing native payload: {artifact}")
        data = artifact.read_bytes()
        if not data or len(data) > 8 * 1024 * 1024:
            raise SystemExit(f"invalid native payload size: {artifact}")
        archive_path = f"RETROSHELL/emulators/{args.name}/{destination}"
        payload.append((artifact, archive_path, data))
        hashes[destination] = sha256(data)
    if not any(path.endswith("/EBOOT.PBP") for _, path, _ in payload):
        raise SystemExit("native package has no EBOOT.PBP")

    descriptor = {
        "formatVersion": 5 if manifest.get("adapterProtocol") else 4,
        "kind": "retroshell-emulator",
        "backend": "native",
        "name": args.name,
        "displayName": adapter["displayName"],
        "version": manifest["version"],
        "systems": manifest["systems"].split("|"),
        "status": adapter["status"],
        "modelSupport": adapter["modelSupport"],
        "notes": adapter["notes"],
        "executable": executable,
        "psp1000Safe": manifest["psp1000Safe"],
        "testOnly": manifest["testOnly"],
        "adapterProtocol": manifest.get("adapterProtocol", 0),
        "pauseMode": manifest.get("pauseMode", "legacy"),
        "pauseHotkey": manifest.get("pauseHotkey", "emulator-defined"),
        "returnMode": manifest.get("returnMode", "emulator-defined"),
        "capabilities": manifest.get("capabilities", []),
        "payloadSha256": hashes,
    }
    provenance = {
        "formatVersion": 1,
        "kind": "retroshell-emulator",
        "repository": adapter["repository"],
        "commit": adapter["commit"],
        "license": adapter["license"],
        "patches": adapter.get("patches", []),
        "payloadSha256": hashes,
    }
    readme = (
        f"RetroShell native emulator: {adapter['displayName']}\n\n"
        "Copy this unopened .rscore.zip into RETROSHELL/cores/ and restart "
        "RetroShell. Native emulators replace the RetroShell process while "
        "playing. Protocol-v1 adapters open the RetroShell-ordered pause "
        "menu with L+R+Select; Emulator Settings opens the emulator's own "
        "settings, and Exit returns to RetroShell.\n"
        "Games and BIOS files are not included.\n"
    ).encode()
    if "sourceLicenseFile" in adapter:
        license_path = source / adapter["sourceLicenseFile"]
    else:
        license_path = ROOT / adapter["licenseFile"]
    if not license_path.is_file():
        raise SystemExit(f"missing native emulator license: {license_path}")
    license_bytes = license_path.read_bytes()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / f"{args.name}-{manifest['version']}.rscore.zip"
    with zipfile.ZipFile(output, "w") as archive:
        for _, destination, data in payload:
            add_bytes(archive, data, destination)
        add_bytes(archive, manifest_bytes,
                  f"RETROSHELL/cores/{args.name}.json")
        add_bytes(archive, license_bytes,
                  f"RETROSHELL/licenses/{args.name}-COPYRIGHT.txt")
        add_bytes(archive, json.dumps(descriptor, indent=2,
                                     sort_keys=True).encode() + b"\n",
                  f"RETROSHELL/core-packages/{args.name}/package.json")
        add_bytes(archive, json.dumps(provenance, indent=2,
                                     sort_keys=True).encode() + b"\n",
                  f"RETROSHELL/core-packages/{args.name}/provenance.json")
        add_bytes(archive, readme,
                  f"RETROSHELL/core-packages/{args.name}/README.txt")
    print(output)


if __name__ == "__main__":
    main()
