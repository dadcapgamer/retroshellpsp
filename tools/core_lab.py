#!/usr/bin/env python3
"""Friendly, focused workflow for RetroShell core contributors.

Core Lab intentionally delegates build, audit, packaging, emulation, and
hardware grading to RetroShell's existing authoritative tools.  It adds core
selection, safer defaults, dry runs, and a small report suitable for a PR.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD = ROOT / "build-core-lab"
DEFAULT_RUNS = ROOT / "build-ppsspp-auto" / "core-lab-runs"
DEFAULT_REPORTS = ROOT / "dist" / "core-lab"


class CoreLabError(RuntimeError):
    """An actionable contributor error, without an argparse traceback."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        display = path.relative_to(ROOT)
    except ValueError:
        display = path
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise CoreLabError(f"could not read {display}: {exc}")
    if not isinstance(value, dict):
        raise CoreLabError(f"{display} must contain an object")
    return value


def core_record(name: str) -> tuple[dict[str, Any], dict[str, Any]]:
    manifest_path = ROOT / "cores" / name / "manifest.json"
    if not manifest_path.is_file():
        known = sorted(path.parent.name for path in
                       (ROOT / "cores").glob("*/manifest.json")
                       if path.parent.name != "dummy")
        raise CoreLabError(
            f"unknown core '{name}'. Available cores: {', '.join(known)}")
    manifest = load_json(manifest_path)
    lock = load_json(ROOT / "core-provenance.lock.json")
    provenance = next((item for item in lock.get("cores", [])
                       if item.get("name") == name), None)
    if not isinstance(provenance, dict):
        raise CoreLabError(
            f"{name} has no entry in core-provenance.lock.json")
    return manifest, provenance


def command_text(command: list[str]) -> str:
    return shlex.join(str(value) for value in command)


def run(command: list[str], dry_run: bool) -> None:
    print(f"+ {command_text(command)}", flush=True)
    if not dry_run:
        subprocess.run(command, cwd=ROOT, check=True)


def psp_cmake() -> str:
    configured = Path(os.environ.get("PSPDEV", Path.home() / "pspdev"))
    candidate = configured / "bin" / "psp-cmake"
    if candidate.is_file():
        return str(candidate)
    return shutil.which("psp-cmake") or str(candidate)


def jobs() -> str:
    return str(max(1, os.cpu_count() or 2))


def build_commands(name: str, build_dir: Path) -> list[list[str]]:
    target = f"rs_core_{name}"
    commands: list[list[str]] = []
    if not (build_dir / "CMakeCache.txt").is_file():
        commands.append([
            psp_cmake(), "-S", str(ROOT), "-B", str(build_dir),
            "-DRS_INCLUDE_TEST_CORES=ON",
        ])
    commands.append([
        "cmake", "--build", str(build_dir), "--target", target,
        "-j", jobs(),
    ])
    return commands


def build_core(name: str, build_dir: Path, dry_run: bool) -> None:
    core_record(name)
    print(f"Building only the {name} PRX in {build_dir}")
    for command in build_commands(name, build_dir):
        run(command, dry_run)


def cmd_doctor(args: argparse.Namespace) -> int:
    pspdev = Path(os.environ.get("PSPDEV", Path.home() / "pspdev"))
    psp_compiler = (str(pspdev / "bin/psp-g++")
                    if (pspdev / "bin/psp-g++").is_file()
                    else shutil.which("psp-g++"))
    checks = [
        ("Python 3", Path(sys.executable).is_file(), sys.version.split()[0]),
        ("CMake", shutil.which("cmake") is not None,
         shutil.which("cmake") or "not found"),
        ("PSP CMake", Path(psp_cmake()).is_file(), psp_cmake()),
        ("PSP compiler", psp_compiler is not None,
         psp_compiler or "not found"),
    ]
    required_ok = True
    print("RetroShell Core Lab environment\n")
    for label, ok, detail in checks:
        required_ok &= ok
        print(f"  {'OK' if ok else 'MISSING':7} {label}: {detail}")
    default_ppsspp = Path(
        "/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL")
    ppsspp = shutil.which("PPSSPPSDL") or (
        str(default_ppsspp) if default_ppsspp.is_file() else None)
    print(f"  {'OK' if ppsspp else 'OPTIONAL':7} PPSSPP: "
          f"{ppsspp or 'pass --ppsspp when running smoke'}")
    if required_ok:
        print("\nReady. Next: ./build.sh core gambatte")
        return 0
    print("\nInstall PSPDEV, set PSPDEV to its directory, and run doctor again.")
    return 1


def cmd_build(args: argparse.Namespace) -> int:
    build_core(args.name, args.build_dir.resolve(), args.dry_run)
    print(f"\nNext: ./build.sh core-package {args.name}")
    return 0


def cmd_audit(args: argparse.Namespace) -> int:
    core_record(args.name)
    run([sys.executable, str(ROOT / "tools/audit_repository.py")],
        args.dry_run)
    if args.dry_run:
        print(f"\nAudit preview complete for {name_label(args.name)}.")
    else:
        print(f"\n{name_label(args.name)} passed repository policy checks.")
    print(f"Next: ./build.sh core-package {args.name}")
    return 0


def cmd_package(args: argparse.Namespace) -> int:
    manifest, provenance = core_record(args.name)
    if provenance.get("delivery") == "archived" and not args.allow_archived:
        raise CoreLabError(
            f"{args.name} is archived and cannot be packaged by default. "
            "Use --allow-archived only for local investigation.")
    if manifest.get("testOnly"):
        print(f"Note: {args.name} is a testing core; the package will retain "
              "that warning and is not release-qualified.")
    build_dir = args.build_dir.resolve()
    build_core(args.name, build_dir, args.dry_run)
    command = [
        sys.executable, str(ROOT / "tools/package_release.py"),
        "--build-dir", str(build_dir), "--core", args.name,
    ]
    if args.output_dir:
        command += ["--output", str(args.output_dir.resolve())]
    if args.allow_archived:
        command.append("--allow-archived")
    run(command, args.dry_run)
    print("\nPackage is a native-code testing artifact; install it only on a "
          "device whose saves are backed up.")
    print(f"Next: python3 tools/core_lab.py smoke {args.name} "
          "--rom /path/to/legal-test-rom")
    return 0


def cmd_smoke(args: argparse.Namespace) -> int:
    manifest, _ = core_record(args.name)
    rom = args.rom.expanduser().resolve()
    if not rom.is_file():
        raise CoreLabError(f"ROM not found: {rom}")
    supported = str(manifest.get("systems", "")).split("|")
    system = args.system or supported[0]
    if system not in supported:
        raise CoreLabError(
            f"{args.name} supports {'|'.join(supported)}, not {system}")
    command = [
        sys.executable, str(ROOT / "tools/run_ppsspp_automation.py"),
        "--system", system, "--core", args.name, "--rom", str(rom),
        "--build-dir", str(args.build_dir.resolve()),
        "--runs-dir", str(args.runs_dir.resolve()),
        "--psp-model", str(args.psp_model),
    ]
    if args.ppsspp:
        command += ["--ppsspp", str(args.ppsspp.expanduser().resolve())]
    if args.skip_build:
        command.append("--skip-build")
    run(command, args.dry_run)
    print("\nPPSSPP is a correctness check, not hardware qualification.")
    print(f"Next: python3 tools/core_lab.py report {args.name}")
    return 0


def result_files(name: str, runs_dir: Path) -> list[Path]:
    matches: list[Path] = []
    if runs_dir.is_dir():
        for path in runs_dir.rglob("result.json"):
            try:
                result = json.loads(path.read_text())
            except (OSError, json.JSONDecodeError):
                continue
            if result.get("core") == name:
                matches.append(path)
    return sorted(matches, key=lambda path: path.stat().st_mtime,
                  reverse=True)


def report_markdown(result: dict[str, Any], source: Path) -> str:
    metrics = result.get("metrics") or {}
    checks = result.get("checks") or {}
    rom = result.get("rom") or {}
    rows = [
        ("Result", result.get("result", "UNKNOWN")),
        ("System", result.get("system", "unknown")),
        ("Core", result.get("core", "unknown")),
        ("ROM SHA-256", rom.get("sha256", "unavailable")),
        ("Median speed", metric(metrics, "median_speed_percent", "%")),
        ("Median p95", metric(metrics, "median_p95_us", " us")),
        ("Peak arena", metric(metrics, "peak_arena_kb", " KB")),
        ("Audio drops", metrics.get("audio_drops_max", "unavailable")),
        ("Video valid", metrics.get("video_valid", "unavailable")),
    ]
    lines = [
        f"# Core Lab report: {result.get('core', 'unknown')}", "",
        "> PPSSPP is a correctness gate. Real PSP hardware is the performance authority.",
        "", "| Field | Value |", "| --- | --- |",
    ]
    lines.extend(f"| {key} | {value} |" for key, value in rows)
    lines.extend(["", "## Checks", ""])
    for key in sorted(checks):
        lines.append(f"- {'PASS' if checks[key] else 'FAIL'} — `{key}`")
    lines.extend(["", f"Source result: `{source}`", ""])
    return "\n".join(lines)


def metric(metrics: dict[str, Any], key: str, suffix: str) -> str:
    value = metrics.get(key)
    return "unavailable" if value is None else f"{value}{suffix}"


def cmd_report(args: argparse.Namespace) -> int:
    core_record(args.name)
    if args.result:
        candidates = [args.result.expanduser().resolve()]
        if not candidates[0].is_file():
            raise CoreLabError(f"result file not found: {candidates[0]}")
    else:
        candidates = result_files(args.name, args.runs_dir.resolve())
    if not candidates:
        raise CoreLabError(
            f"no PPSSPP result found for {args.name}. Run smoke first.")
    source = candidates[0]
    result = load_json(source)
    if result.get("core") != args.name:
        raise CoreLabError(
            f"{source} belongs to {result.get('core')!r}, not {args.name!r}")
    output = (args.output.resolve() if args.output else
              DEFAULT_REPORTS / f"{args.name}-report.md")
    content = report_markdown(result, source)
    if args.dry_run:
        print(f"Would write {output}\n")
        print(content)
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(content)
        print(output)
    print("\nNext: attach this report to the PR. Before promotion, run on a "
          "real PSP and grade the log with ./build.sh qualify LOG.")
    return 0


def name_label(name: str) -> str:
    return f"Core '{name}'"


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Build, test, package, and report on one RetroShell core")
    result.add_argument(
        "--dry-run", action="store_true",
        help="print commands and planned outputs without changing files")
    sub = result.add_subparsers(dest="command", required=True)

    doctor = sub.add_parser("doctor", help="check the local toolchain")
    doctor.set_defaults(handler=cmd_doctor)

    build = sub.add_parser("build", help="build one core PRX")
    build.add_argument("name")
    build.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    build.set_defaults(handler=cmd_build)

    audit = sub.add_parser("audit", help="run repository policy checks")
    audit.add_argument("name")
    audit.set_defaults(handler=cmd_audit)

    package = sub.add_parser("package", help="build and package one core")
    package.add_argument("name")
    package.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    package.add_argument("--output-dir", type=Path)
    package.add_argument("--allow-archived", action="store_true")
    package.set_defaults(handler=cmd_package)

    smoke = sub.add_parser("smoke", help="run PPSSPP correctness automation")
    smoke.add_argument("name")
    smoke.add_argument("--rom", type=Path, required=True,
                       help="user-owned or redistributable test ROM")
    smoke.add_argument("--system",
                       choices=("gb", "gbc", "gba", "nes", "snes",
                                "md", "sms", "gg", "pce"))
    smoke.add_argument("--psp-model", type=int, choices=(0, 1), default=0)
    smoke.add_argument("--ppsspp", type=Path)
    smoke.add_argument("--build-dir", type=Path,
                       default=ROOT / "build-ppsspp-auto/psp-build")
    smoke.add_argument("--runs-dir", type=Path, default=DEFAULT_RUNS)
    smoke.add_argument("--skip-build", action="store_true")
    smoke.set_defaults(handler=cmd_smoke)

    report = sub.add_parser("report", help="write a PR-ready PPSSPP report")
    report.add_argument("name")
    report.add_argument("--result", type=Path,
                        help="specific result.json; otherwise use latest")
    report.add_argument("--runs-dir", type=Path, default=DEFAULT_RUNS)
    report.add_argument("--output", type=Path)
    report.set_defaults(handler=cmd_report)
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        return int(args.handler(args))
    except CoreLabError as exc:
        print(f"Core Lab: {exc}", file=sys.stderr)
        print("Next: correct the value above, or run "
              "python3 tools/core_lab.py --help.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"Core Lab: command failed ({exc.returncode}): "
              f"{command_text(exc.cmd)}", file=sys.stderr)
        print("Fix the first error above, then rerun the same Core Lab command.",
              file=sys.stderr)
        return exc.returncode or 1


if __name__ == "__main__":
    raise SystemExit(main())
