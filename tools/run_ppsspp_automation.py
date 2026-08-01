#!/usr/bin/env python3
"""Build and run an isolated RetroShell PPSSPP correctness smoke test."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SYSTEMS = {
    "gb": (0, "GameBoy"),
    "gbc": (1, "GameBoyColor"),
    "gba": (2, "GBA"),
    "nes": (3, "NES"),
    "snes": (4, "SNES"),
    "md": (5, "Genesis"),
    "sms": (6, "MasterSystem"),
    "gg": (7, "GameGear"),
    "pce": (8, "PCEngine"),
}
PERF = re.compile(
    r"perf: (\d+) emu fps \((\d+)% speed\).*p95 (\d+) us.*"
    r"arena \d+/(\d+) KB.*allocfail (\d+).*"
    r"underrun (\d+) drop (\d+).*video (\S+)"
)
ARENA_RESERVED = re.compile(r"arena: reserved (\d+) KB")
EXPECTED_SHOTS = {
    "boot.png", "home.png", "list_target.png", "quick_actions.png",
    "game_a.png", "game_b.png",
    "game_c.png", "pause_menu.png", "pause_fast_scroll.png",
    "returned_home.png", "home_recent.png",
    "recent_focus.png", "favorites_home.png", "favorites_list.png",
    "settings.png", "accent_picker.png", "game_relaunch.png",
}


def run(command: list[str], env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--system", choices=SYSTEMS, default="gba")
    parser.add_argument("--core", default="gpsp")
    parser.add_argument("--rom", type=Path, required=True,
                        help="legally redistributable or user-owned test ROM")
    parser.add_argument(
        "--boxart", type=Path,
        help="optional PNG/JPG cover copied into the isolated visual test")
    parser.add_argument(
        "--ppsspp", type=Path,
        default=Path("/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"))
    parser.add_argument("--build-dir", type=Path,
                        default=ROOT / "build-ppsspp-auto" / "psp-build")
    parser.add_argument("--runs-dir", type=Path,
                        default=ROOT / "build-ppsspp-auto" / "runs")
    parser.add_argument("--timeout", type=int, default=75)
    parser.add_argument("--theme", choices=("dark", "light"), default="dark",
                        help="frontend theme used for visual validation")
    storage_group = parser.add_mutually_exclusive_group()
    storage_group.add_argument(
        "--legacy-storage", action="store_true",
        help="seed RETROSUITE instead of RETROSHELL to verify migration")
    storage_group.add_argument(
        "--mixed-storage", action="store_true",
        help="seed an old data tree beside a new package tree to verify merge migration")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    args.rom = args.rom.resolve()
    if args.boxart:
        args.boxart = args.boxart.resolve()
    args.build_dir = args.build_dir.resolve()
    args.runs_dir = args.runs_dir.resolve()
    if not args.rom.is_file():
        parser.error(f"ROM not found: {args.rom}")
    if args.boxart and (
            not args.boxart.is_file() or
            args.boxart.suffix.lower() not in (".png", ".jpg", ".jpeg")):
        parser.error(f"box art must be a PNG/JPG file: {args.boxart}")
    if not args.ppsspp.is_file():
        parser.error(f"PPSSPP executable not found: {args.ppsspp}")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,48}", args.core):
        parser.error("unsafe core name")
    # Capture identity before launching. The source may live on a removable
    # PSP Memory Stick that is disconnected while the isolated copy runs.
    rom_name = args.rom.name
    rom_sha256 = hashlib.sha256(args.rom.read_bytes()).hexdigest()

    system_index, legacy_rom_dir = SYSTEMS[args.system]
    pspdev = Path(os.environ.get("PSPDEV", Path.home() / "pspdev"))
    build_env = os.environ.copy()
    build_env["PSPDEV"] = str(pspdev)
    build_env["PATH"] = f"{pspdev / 'bin'}:{build_env.get('PATH', '')}"
    if not args.skip_build:
        run([
            str(pspdev / "bin" / "psp-cmake"), "-S", str(ROOT), "-B",
            str(args.build_dir), "-DRS_INCLUDE_TEST_CORES=ON",
            "-DRS_AUTOPILOT=ON",
            f"-DRS_AUTOPILOT_SYSTEM_INDEX={system_index}",
            "-DRS_SHIM_DIAG=ON",
        ], build_env)
        run([
            "cmake", "--build", str(args.build_dir), "--target",
            "retrosuite", f"rs_core_{args.core}", "-j8",
        ], build_env)

    eboot = args.build_dir / "src" / "EBOOT.PBP"
    core_prx = args.build_dir / "cores" / f"{args.core}.prx"
    core_json = args.build_dir / "cores" / f"{args.core}.json"
    for artifact in (eboot, core_prx, core_json):
        if not artifact.is_file():
            raise SystemExit(f"missing build artifact: {artifact}")

    stamp = time.strftime("%Y%m%d-%H%M%S")
    run_dir = args.runs_dir / f"{args.system}-{args.core}-{stamp}"
    home = run_dir / "home"
    memstick = home / ".config" / "ppsspp"
    game_dir = memstick / "PSP" / "GAME" / "RetroShell"
    system_dir = memstick / "PSP" / "SYSTEM"
    rs_dir = memstick / "RETROSHELL"
    legacy_seed = args.legacy_storage or args.mixed_storage
    seed_dir = memstick / ("RETROSUITE" if legacy_seed else "RETROSHELL")
    cores_dir = (rs_dir if args.mixed_storage else seed_dir) / "cores"
    # Keep the test ROM directly in the shared root. This exercises the
    # production scanner's extension-based console sorting rather than
    # relying on a console-specific folder name.
    target_rom_dir = memstick / "ROMS"
    for directory in (game_dir, system_dir, cores_dir, target_rom_dir):
        directory.mkdir(parents=True, exist_ok=True)
    shutil.copy2(eboot, game_dir / "EBOOT.PBP")
    shutil.copy2(core_prx, cores_dir / core_prx.name)
    shutil.copy2(core_json, cores_dir / core_json.name)
    if args.mixed_storage:
        legacy_cores = seed_dir / "cores"
        legacy_cores.mkdir(parents=True, exist_ok=True)
        shutil.copy2(core_prx, legacy_cores / core_prx.name)
        shutil.copy2(core_json, legacy_cores / core_json.name)
    shutil.copy2(args.rom, target_rom_dir / args.rom.name)
    if args.boxart:
        shutil.copy2(args.boxart,
                     target_rom_dir /
                     f"{args.rom.stem}{args.boxart.suffix.lower()}")
    write_json(seed_dir / "config.json", {
        # Candidate visibility is disabled; PPSSPP itself remains model 0
        # and RetroShell still receives the 17 MB PSP-1000 core arena.
        "psp1000SafeMode": False,
        "showFps": True,
        "autoSave": False,
        "theme": args.theme,
    })
    (system_dir / "ppsspp.ini").write_text(
        "[General]\nFirstRun = False\nCheckForNewVersion = False\n"
        "[Graphics]\nBackend = 3\nSoftwareRendering = True\n"
        "FrameSkip = 0\nAutoFrameSkip = False\n"
        "[SystemParam]\nPSPModel = 0\nPSPFirmwareVersion = 660\n"
        "[CPU]\nFastMemoryAccess = True\n"
    )

    launch_env = os.environ.copy()
    launch_env["HOME"] = str(home)
    command = [
        str(args.ppsspp), "--windowed", "--graphics=software",
        "--escape-exit", str(game_dir / "EBOOT.PBP"),
    ]
    print("+", " ".join(command), flush=True)
    output_path = run_dir / "ppsspp-output.log"
    with output_path.open("wb") as output:
        proc = subprocess.Popen(command, cwd=ROOT, env=launch_env,
                                stdout=output, stderr=subprocess.STDOUT)
        log_path = rs_dir / "retroshell.log"
        deadline = time.monotonic() + args.timeout
        complete = False
        while time.monotonic() < deadline:
            if log_path.is_file():
                text = log_path.read_text(errors="replace")
                if ("autopilot: complete" in text and
                        "clean exit" in text):
                    complete = True
                    break
            if proc.poll() is not None:
                break
            time.sleep(0.5)
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.terminate()
                proc.wait(timeout=5)

    text = log_path.read_text(errors="replace") if log_path.is_file() else ""
    arena_matches = ARENA_RESERVED.findall(text)
    initial_arena_kb = int(arena_matches[0]) if arena_matches else 0
    checks = {
        "autopilot_complete": complete,
        # PPSSPP's model-0 maximum block can vary by one page depending on
        # loader alignment. Both values represent the 32 MB PSP-1000 model.
        "psp1000_arena": 17_000 <= initial_arena_kb <= 18_000,
        "core_loaded_twice": text.count(f"core: '{args.core}' ready") >= 2,
        "rom_loaded_twice": text.count("session: ROM loaded") >= 2,
        "state_saved": "save: state slot 0 ok" in text,
        "state_loaded": "save: state slot 0 load ok" in text,
        "returned_to_frontend": "returning to frontend" in text,
        "clean_exit": "clean exit" in text,
        "no_error_log": "[E]" not in text,
        "no_allocation_failure": "allocfail 0" in text and
                                 "failures 0" in text,
    }
    if args.legacy_storage or args.mixed_storage:
        checks["legacy_storage_removed"] = not seed_dir.exists()
        checks["storage_migration_reported"] = (
            "storage: legacy RETROSUITE migration renamed" in text or
            "storage: legacy RETROSUITE migration merged" in text)
        checks["legacy_config_preserved"] = (rs_dir / "config.json").is_file()
    if args.mixed_storage:
        checks["core_collision_preserved"] = (
            (rs_dir / "cores" / f"{args.core}.prx.legacy").is_file() and
            (rs_dir / "cores" / f"{args.core}.json.legacy").is_file())
    if args.boxart:
        checks["sibling_boxart_loaded"] = "boxart: loaded beside ROM" in text
    shots_dir = rs_dir / "shots"
    shots = {p.name: p for p in shots_dir.glob("*.png")}
    hashes = {
        name: hashlib.sha256(path.read_bytes()).hexdigest()
        for name, path in shots.items()
    }
    game_hashes = {hashes.get(name) for name in
                   ("game_a.png", "game_b.png", "game_c.png",
                    "game_relaunch.png")}
    checks["all_screenshots"] = EXPECTED_SHOTS <= set(shots)
    checks["screenshots_not_stale"] = (
        None not in game_hashes and
        len(game_hashes) >= 2 and
        hashes.get("home.png") not in game_hashes
    )

    perf = [match.groups() for match in PERF.finditer(text)]
    metrics = {}
    if perf:
        video_states = [sample[7] for sample in perf]
        invalid_video = sum(
            state in ("missing", "invalid") for state in video_states)
        black_video = sum(state == "black" for state in video_states)
        video_valid = invalid_video == 0 and any(
            state == "ok" for state in video_states)
        checks["video_valid"] = video_valid
        audio_drops_max = max(int(sample[6]) for sample in perf)
        checks["no_audio_drops"] = audio_drops_max == 0
        metrics = {
            "samples": len(perf),
            "median_speed_percent": statistics.median(
                int(sample[1]) for sample in perf),
            "median_p95_us": statistics.median(
                int(sample[2]) for sample in perf),
            "peak_arena_kb": max(int(sample[3]) for sample in perf),
            "allocation_failures": max(int(sample[4]) for sample in perf),
            "audio_underruns_final_counter": int(perf[-1][5]),
            "audio_drops_max": audio_drops_max,
            # A core may legitimately show an all-black boot or transition
            # frame. Treat those as telemetry, while missing/invalid output
            # or a run that never produces a valid frame remains a failure.
            "video_valid": video_valid,
            "transient_black_samples": black_video,
            "invalid_video_samples": invalid_video,
        }
    else:
        checks["video_valid"] = False
        checks["no_audio_drops"] = False
    result = {
        "result": "PASS" if all(checks.values()) else "FAIL",
        "system": args.system,
        "core": args.core,
        "rom": {
            "name": rom_name,
            "sha256": rom_sha256,
        },
        "checks": checks,
        "metrics": metrics,
        "run_directory": str(run_dir),
        "note": "PPSSPP is a correctness gate; real PSP hardware is the performance authority.",
    }
    write_json(run_dir / "result.json", result)
    print(json.dumps(result, indent=2))
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
