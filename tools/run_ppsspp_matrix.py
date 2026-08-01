#!/usr/bin/env python3
"""Run one isolated PPSSPP smoke test for every non-dummy core."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--rom", action="append", default=[], metavar="SYSTEM=PATH",
        help="repeat for gb, gbc, gba, nes, snes, md, sms, gg, or pce")
    parser.add_argument(
        "--runs-dir", type=Path,
        default=ROOT / "build-ppsspp-auto" / "matrix-runs")
    parser.add_argument("--ppsspp", type=Path)
    parser.add_argument("--timeout", type=int, default=75)
    args = parser.parse_args()

    roms: dict[str, Path] = {}
    for value in args.rom:
        if "=" not in value:
            parser.error(f"expected SYSTEM=PATH, got {value!r}")
        system, raw_path = value.split("=", 1)
        path = Path(raw_path).expanduser().resolve()
        if system in roms:
            parser.error(f"duplicate ROM for {system}")
        if not path.is_file():
            parser.error(f"ROM not found: {path}")
        roms[system] = path

    lock = json.loads((ROOT / "core-provenance.lock.json").read_text())
    active = {
        core["name"] for core in lock["cores"]
        if core.get("delivery") in {"production", "candidate"}
    }
    manifests = []
    for path in sorted((ROOT / "cores").glob("*/manifest.json")):
        manifest = json.loads(path.read_text())
        if manifest["name"] not in active:
            continue
        systems = manifest["systems"].split("|")
        chosen = next((system for system in systems if system in roms), None)
        manifests.append((manifest["name"], chosen, systems))

    stamp = time.strftime("%Y%m%d-%H%M%S")
    matrix_dir = args.runs_dir.resolve() / stamp
    core_runs = matrix_dir / "runs"
    matrix_dir.mkdir(parents=True, exist_ok=True)
    results = []
    runner = ROOT / "tools" / "run_ppsspp_automation.py"
    for core, system, supported in manifests:
        if system is None:
            results.append({
                "core": core, "result": "SKIP",
                "reason": f"no ROM supplied for {'|'.join(supported)}",
            })
            continue
        command = [
            sys.executable, str(runner), "--system", system, "--core", core,
            "--rom", str(roms[system]), "--runs-dir", str(core_runs),
            "--timeout", str(args.timeout),
        ]
        if args.ppsspp:
            command += ["--ppsspp", str(args.ppsspp.resolve())]
        print(f"\n=== {core} / {system} ===", flush=True)
        completed = subprocess.run(command, cwd=ROOT)
        candidates = sorted(core_runs.glob(
            f"{system}-{core}-*/result.json"))
        if not candidates:
            results.append({
                "core": core, "system": system, "result": "FAIL",
                "reason": f"runner exited {completed.returncode} without result",
            })
            continue
        result = json.loads(candidates[-1].read_text())
        result["runner_exit_code"] = completed.returncode
        results.append(result)

    summary = {
        "result": (
            "PASS" if results and
            all(result["result"] == "PASS" for result in results)
            else "FAIL"
        ),
        "cores": len(results),
        "passed": sum(result["result"] == "PASS" for result in results),
        "failed": sum(result["result"] == "FAIL" for result in results),
        "skipped": sum(result["result"] == "SKIP" for result in results),
        "results": results,
        "matrix_directory": str(matrix_dir),
    }
    output = matrix_dir / "matrix-result.json"
    output.write_text(json.dumps(summary, indent=2) + "\n")
    print("\n=== MATRIX SUMMARY ===")
    print(json.dumps({
        key: summary[key]
        for key in ("result", "cores", "passed", "failed", "skipped",
                    "matrix_directory")
    }, indent=2))
    return 0 if summary["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
