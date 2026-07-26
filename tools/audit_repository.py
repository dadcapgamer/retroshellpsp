#!/usr/bin/env python3
"""Fast, dependency-free release invariants for RetroSuite."""

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
SHIPPING_SYSTEMS = {"gb", "gbc", "nes", "snes", "md", "sms", "gg"}
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    errors: list[str] = []

    lock = json.loads(LOCK.read_text())
    locked = {entry["name"]: entry for entry in lock["cores"]}
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
        if name in manifests:
            fail(errors, f"{path}: duplicate core name {name}")
        manifests[name] = data

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
        defaults = [
            name for name in production
            if system in manifests[name]["systems"].split("|")
        ]
        if len(defaults) != 1:
            fail(errors, f"{system}: expected one production default, found {sorted(defaults)}")

    for name, entry in locked.items():
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
    print(f"OK: {len(production)} production and {len(integrated - production)} candidate cores; manifests, provenance, and PRX policy valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
