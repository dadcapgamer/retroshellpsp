#!/usr/bin/env python3
"""Fast, dependency-free release invariants for RetroShell."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "core-provenance.lock.json"
VALID_SYSTEMS = {"gb", "gbc", "gba", "nes", "snes", "md", "sms", "gg", "pce"}
SHIPPING_SYSTEMS = {"gb", "gbc", "gba", "nes", "snes", "md", "sms", "gg", "pce"}
SHA256 = re.compile(r"^[0-9a-f]{64}$")
COMMIT = re.compile(r"^[0-9a-f]{40}$")


def tree_hash(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def fail(errors: list[str], message: str) -> None:
    errors.append(message)

def png_dimensions(path: Path) -> tuple[int, int] | None:
    data = path.read_bytes()[:24] if path.is_file() else b""
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return (int.from_bytes(data[16:20], "big"),
            int.from_bytes(data[20:24], "big"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    errors: list[str] = []

    lock = json.loads(LOCK.read_text())
    locked = {entry["name"]: entry for entry in lock["cores"]}
    catalog_document = json.loads((ROOT / "core-catalog.json").read_text())
    catalog = catalog_document.get("cores", {})
    if catalog_document.get("formatVersion") != 1:
        fail(errors, "core catalog formatVersion must be 1")
    if set(catalog) != set(locked):
        fail(errors, "core catalog and provenance lock must contain the same cores")
    for name, item in catalog.items():
        if item.get("status") not in {"Included", "Testing", "Experimental", "Removed"}:
            fail(errors, f"{name}: invalid catalog status")
        for field in ("displayName", "modelSupport", "notes"):
            if not isinstance(item.get(field), str) or not item[field].strip():
                fail(errors, f"{name}: catalog {field} must be a non-empty string")
    manifests: dict[str, dict] = {}

    for path in sorted((ROOT / "cores").glob("*/manifest.json")):
        try:
            data = json.loads(path.read_text())
        except (OSError, json.JSONDecodeError) as exc:
            fail(errors, f"{path}: invalid JSON: {exc}")
            continue
        name = data.get("name")
        if name != path.parent.name or not re.fullmatch(r"[a-z0-9_-]{1,31}", name or ""):
            fail(errors, f"{path}: unsafe or mismatched name")
        systems = set(str(data.get("systems", "")).split("|"))
        if not systems or not systems <= VALID_SYSTEMS:
            fail(errors, f"{path}: unsupported system set {sorted(systems)}")
        if type(data.get("priority")) is not int:
            fail(errors, f"{path}: priority must be an integer")
        if type(data.get("testOnly")) is not bool:
            fail(errors, f"{path}: testOnly must be a boolean")
        if type(data.get("psp1000Safe")) is not bool:
            fail(errors, f"{path}: psp1000Safe must be a boolean")
        if "requiresFullContent" in data and type(data["requiresFullContent"]) is not bool:
            fail(errors, f"{path}: requiresFullContent must be a boolean")
        if "preferVfs" in data and type(data["preferVfs"]) is not bool:
            fail(errors, f"{path}: preferVfs must be a boolean")
        if data.get("requiresFullContent") and data.get("preferVfs"):
            fail(errors, f"{path}: cannot require full content and prefer VFS")
        if name in manifests:
            fail(errors, f"{path}: duplicate core name {name}")
        manifests[name] = data

    native_manifests: dict[str, dict] = {}
    for manifest_path in sorted((ROOT / "native-emulators").glob("*/manifest.json")):
        adapter_path = manifest_path.with_name("adapter.json")
        try:
            data = json.loads(manifest_path.read_text())
            adapter = json.loads(adapter_path.read_text())
        except (OSError, json.JSONDecodeError) as exc:
            fail(errors, f"{manifest_path.parent}: invalid native adapter: {exc}")
            continue
        name = data.get("name")
        if (name != manifest_path.parent.name or
                not re.fullmatch(r"[a-z0-9_-]{1,31}", name or "")):
            fail(errors, f"{manifest_path}: unsafe or mismatched name")
            continue
        if adapter.get("formatVersion") not in (1, 2) or adapter.get("name") != name:
            fail(errors, f"{adapter_path}: invalid format or mismatched name")
        if data.get("backend") != "native":
            fail(errors, f"{manifest_path}: backend must be native")
        if adapter.get("formatVersion") == 2:
            contract = {
                "adapterProtocol": 1,
                "pauseMode": "embedded",
                "pauseHotkey": "L+R+SELECT",
                "returnMode": "loadexec",
            }
            for field, expected in contract.items():
                if adapter.get(field) != expected or data.get(field) != expected:
                    fail(errors, f"{manifest_path.parent}: invalid adapter contract field {field}")
            capabilities = data.get("capabilities")
            if (not isinstance(capabilities, list) or
                    not {"pause", "resume", "exitToShell"}.issubset(capabilities)):
                fail(errors, f"{manifest_path}: incomplete adapter capabilities")
        if data.get("executable") != f"emulators/{name}/EBOOT.PBP":
            fail(errors, f"{manifest_path}: executable must use the canonical path")
        systems = set(str(data.get("systems", "")).split("|"))
        if not systems or not systems <= VALID_SYSTEMS:
            fail(errors, f"{manifest_path}: unsupported system set {sorted(systems)}")
        for field, expected_type in (("priority", int), ("testOnly", bool),
                                     ("psp1000Safe", bool)):
            if type(data.get(field)) is not expected_type:
                fail(errors, f"{manifest_path}: {field} has the wrong type")
        if not COMMIT.fullmatch(adapter.get("commit", "")):
            fail(errors, f"{adapter_path}: commit is not a full Git object ID")
        payload = adapter.get("payload")
        if not isinstance(payload, list) or not payload:
            fail(errors, f"{adapter_path}: payload must not be empty")
            payload = []
        destinations: set[str] = set()
        for item in payload:
            source = item.get("source", "") if isinstance(item, dict) else ""
            destination = item.get("destination", "") if isinstance(item, dict) else ""
            if (not source or not destination or Path(source).is_absolute() or
                    Path(destination).is_absolute() or ".." in Path(source).parts or
                    ".." in Path(destination).parts):
                fail(errors, f"{adapter_path}: unsafe payload entry")
                continue
            if destination in destinations:
                fail(errors, f"{adapter_path}: duplicate destination {destination}")
            destinations.add(destination)
        if "EBOOT.PBP" not in destinations:
            fail(errors, f"{adapter_path}: payload must include EBOOT.PBP")
        for patch in adapter.get("patches", []):
            if not isinstance(patch, str) or not (ROOT / patch).is_file():
                fail(errors, f"{adapter_path}: missing adapter patch {patch}")
        license_file = adapter.get("licenseFile")
        source_license = adapter.get("sourceLicenseFile")
        if not ((isinstance(license_file, str) and (ROOT / license_file).is_file()) or
                (isinstance(source_license, str) and source_license.strip())):
            fail(errors, f"{adapter_path}: no verifiable license source")
        native_manifests[name] = data

    # A native adapter replaces the RetroShell process: launching one exits to
    # the XMB instead of returning to the frontend, and it needs its own BIOS
    # and saves. CoreRegistry::defaultFor picks the highest priority, so an
    # adapter that outranks an in-process core would silently become the
    # default for its systems. Adapters are alternates and must rank below.
    for native_name, native in native_manifests.items():
        native_systems = set(native.get("systems", "").split("|"))
        for prx_name, prx in manifests.items():
            # Archived cores are never offered, so outranking them is fine and
            # intended — the adapter replaces them. A same-name PRX is the
            # retired conversion of the adapter itself, not a rival.
            if prx_name == "dummy" or prx_name == native_name:
                continue
            if locked.get(prx_name, {}).get("delivery") == "archived":
                continue
            if not native_systems & set(prx.get("systems", "").split("|")):
                continue
            if native.get("priority", 0) >= prx.get("priority", 0):
                fail(errors,
                     f"native adapter {native_name} priority "
                     f"{native.get('priority')} must rank below in-process "
                     f"core {prx_name} ({prx.get('priority')}) for shared "
                     f"systems; adapters are alternates, not defaults")

    integrated = set(manifests) - {"dummy"}
    production = {name for name, data in manifests.items() if not data["testOnly"]}
    locked_production = {
        name for name, entry in locked.items() if entry.get("delivery") == "production"
    }
    if integrated != set(locked):
        fail(errors, f"provenance set {sorted(locked)} != integrated set {sorted(integrated)}")
    if production != locked_production:
        fail(errors, f"manifest production set {sorted(production)} != lockfile set {sorted(locked_production)}")
    for system in sorted(SHIPPING_SYSTEMS):
        # Several production cores may serve a system (an alternate such as
        # SMS Plus beside PicoDrive); exactly one must rank highest, so the
        # default never depends on discovery order.
        serving = [
            name for name in production
            if system in manifests[name]["systems"].split("|")
        ]
        top = max((manifests[name]["priority"] for name in serving), default=None)
        defaults = [name for name in serving if manifests[name]["priority"] == top]
        if len(defaults) != 1:
            fail(errors, f"{system}: expected one production default, found {sorted(defaults)}")

    for name, entry in locked.items():
        if entry.get("delivery") not in {"production", "candidate", "archived"}:
            fail(errors, f"{name}: invalid delivery classification")
        if not COMMIT.fullmatch(entry.get("commit", "")):
            fail(errors, f"{name}: commit is not a full Git object ID")
        expected = entry.get("sourceTreeSha256", "")
        if not SHA256.fullmatch(expected):
            fail(errors, f"{name}: invalid source tree hash")
            continue
        upstream = ROOT / entry["retainedSource"]
        actual = tree_hash(upstream)
        if actual != expected:
            fail(errors, f"{name}: source tree hash mismatch ({actual})")
        if not (ROOT / entry["licenseFile"]).is_file():
            fail(errors, f"{name}: missing retained license")

    cmake = (ROOT / "CMakeLists.txt").read_text()
    if "RS_STATIC_CORES is disabled" not in cmake:
        fail(errors, "static multi-core build is not explicitly blocked")
    frontend_cmake = (ROOT / "src" / "CMakeLists.txt").read_text()
    if 'TITLE "RetroShell"' not in frontend_cmake:
        fail(errors, "PSP package title is not RetroShell")
    branding_assets = {
        "assets/branding/retroshell-splash-2x.png": (960, 544),
        "assets/SPLASH_MASK.PNG": (480, 272),
        "assets/SPLASH_ICONS.PNG": (480, 272),
        "assets/branding/retroshell-xmb-background-2x.png": (960, 544),
        "assets/branding/retroshell-logo-light-2x.png": (124, 124),
        "assets/ICON0.PNG": (144, 80),
        "assets/PIC1.PNG": (480, 272),
    }
    for relative, expected in branding_assets.items():
        actual = png_dimensions(ROOT / relative)
        if actual != expected:
            fail(errors, f"{relative}: expected PNG dimensions "
                         f"{expected}, found {actual}")
    package_script = (ROOT / "tools" / "package_release.py").read_text()
    if ("RetroShell-PSP" not in package_script or
            "PSP/GAME/RetroShell/EBOOT.PBP" not in package_script):
        fail(errors, "release packaging does not use RetroShell identity")
    api_header = (ROOT / "src" / "core_api" / "rs_core_api.h").read_text()
    api_match = re.search(r"#define\s+RS_CORE_API_VERSION\s+(\d+)u", api_header)
    if not api_match or f"CORE_API_VERSION = {api_match.group(1)}" not in package_script:
        fail(errors, "core package descriptor is not synchronized with the Core API")
    if "add_subdirectory(dummy)" in (ROOT / "cores" / "CMakeLists.txt").read_text().replace(
        "if(RS_INCLUDE_TEST_CORES)\n  add_subdirectory(dummy)\nendif()", ""
    ):
        fail(errors, "dummy core is included outside the test-only guard")

    if args.build_dir:
        build = args.build_dir.resolve()
        if not (build / "src/EBOOT.PBP").is_file():
            fail(errors, "build is missing src/EBOOT.PBP")
        for name, entry in locked.items():
            artifact = build / f"cores/{name}.prx"
            if not artifact.is_file():
                fail(errors, f"build is missing {name}.prx")
                continue
            actual = hashlib.sha256(artifact.read_bytes()).hexdigest()
            if actual != entry["expectedArtifactSha256"]:
                fail(errors, f"{name}: artifact hash mismatch ({actual})")

    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    active_candidates = {
        name for name, entry in locked.items()
        if entry.get("delivery") == "candidate"
    }
    archived = {
        name for name, entry in locked.items()
        if entry.get("delivery") == "archived"
    }
    print(f"OK: {len(production)} production, {len(active_candidates)} "
          f"candidate, {len(archived)} archived cores, and "
          f"{len(native_manifests)} native adapters; manifests, provenance, "
          "and execution policy valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
