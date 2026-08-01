#!/usr/bin/env python3
"""Grade every core session recorded in one real-PSP RetroShell log."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE = re.compile(r"core: '([^']+)' ready")
PERF = re.compile(
    r"perf: (\d+) emu fps \((\d+)% speed\) \| avg (\d+) us \| "
    r"p95 (\d+) us .* arena (\d+)/(\d+) KB \| allocfail (\d+) \| "
    r"audio buf (\d+) underrun (\d+) drop (\d+) \| video (\S+)"
)


def expected_cores() -> set[str]:
    lock = json.loads((ROOT / "core-provenance.lock.json").read_text())
    return {
        core["name"] for core in lock["cores"]
        if core.get("delivery") in {"production", "candidate"}
    }


def grade(name: str, sessions: list[list[tuple]], warmup: int) -> dict:
    kept = [sample for session in sessions for sample in session[warmup:]]
    if not kept:
        return {
            "core": name, "result": "FAIL", "sessions": len(sessions),
            "samples": 0, "failures": ["no samples remained after warmup"],
        }

    speeds = [int(sample[1]) for sample in kept]
    p95_values = [int(sample[3]) for sample in kept]
    target_fps = [
        int(sample[0]) * 100.0 / int(sample[1])
        for sample in kept if int(sample[1]) > 0
    ]
    budget = 1_000_000.0 / statistics.median(target_fps)
    failures = []
    median_speed = statistics.median(speeds)
    median_p95 = statistics.median(p95_values)
    allocation_failures = max(int(sample[6]) for sample in kept)
    drops = max(int(sample[9]) for sample in kept)
    bad_video = sum(
        sample[10] in ("black", "missing", "invalid") for sample in kept)
    if median_speed < 95:
        failures.append("median emulation speed below 95%")
    if median_p95 > budget:
        failures.append("median p95 exceeds native frame budget")
    if allocation_failures:
        failures.append("core allocation failures recorded")
    if drops:
        failures.append("audio frames were dropped")
    if bad_video:
        failures.append("black, missing, or invalid video samples recorded")

    # Underrun counters reset at each launch. Judge the steady ten-sample tail
    # of every session instead of subtracting counters across launches.
    tail_deltas = []
    for session in sessions:
        tail = session[max(warmup, len(session) - 10):]
        if len(tail) >= 2:
            tail_deltas.append(int(tail[-1][8]) - int(tail[0][8]))
    worst_tail = max(tail_deltas, default=0)
    if worst_tail > 2:
        failures.append("audio underruns persisted through a steady tail")

    return {
        "core": name,
        "result": "PASS" if not failures else "FAIL",
        "sessions": len(sessions),
        "samples": len(kept),
        "median_speed_percent": median_speed,
        "median_p95_us": median_p95,
        "native_frame_budget_us": round(budget),
        "peak_arena_kb": max(int(sample[5]) for sample in kept),
        "allocation_failures": allocation_failures,
        "worst_steady_tail_underrun_delta": worst_tail,
        "dropped_audio_frames": drops,
        "bad_video_samples": bad_video,
        "failures": failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    sessions: dict[str, list[list[tuple]]] = {}
    active: list[tuple] | None = None
    for line in args.log.read_text(errors="replace").splitlines():
        if match := CORE.search(line):
            active = []
            sessions.setdefault(match.group(1), []).append(active)
        elif active is not None and (match := PERF.search(line)):
            active.append(match.groups())

    expected = expected_cores()
    reports = [
        grade(name, sessions[name], args.warmup)
        for name in sorted(sessions)
        if name in expected
    ]
    seen = set(sessions)
    missing = sorted(expected - seen)
    archived_observed = sorted(seen - expected)
    failed = [report["core"] for report in reports
              if report["result"] == "FAIL"]
    result = {
        "result": "PASS" if not missing and not failed else "FAIL",
        "expected_cores": len(expected),
        "tested_cores": len(seen & expected),
        "missing_cores": missing,
        "archived_cores_observed": archived_observed,
        "failed_cores": failed,
        "cores": reports,
    }
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"{result['result']}: {result['tested_cores']}/"
              f"{result['expected_cores']} cores recorded")
        for report in reports:
            speed = report.get("median_speed_percent", "-")
            p95 = report.get("median_p95_us", "-")
            print(f"  {report['result']:4} {report['core']:<18} "
                  f"speed {speed}%  p95 {p95} us  "
                  f"samples {report['samples']}")
            for failure in report["failures"]:
                print(f"       - {failure}")
        if missing:
            print("  Missing: " + ", ".join(missing))
        if archived_observed:
            print("  Archived cores also present: " +
                  ", ".join(archived_observed))
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
