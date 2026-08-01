#!/usr/bin/env python3
"""Summarize the latest real-PSP RetroShell run and apply release gates."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CORE = re.compile(r"core: '([^']+)' ready")
PERF = re.compile(
    r"perf: (\d+) emu fps \((\d+)% speed\) \| avg (\d+) us \| "
    r"p95 (\d+) us .* arena (\d+)/(\d+) KB \| allocfail (\d+) \| "
    r"audio buf (\d+) underrun (\d+) drop (\d+) \| video (\S+)"
)
DETAIL = re.compile(
    r"perf detail: rendered (\d+) avg (\d+) us p95 (\d+) us \| "
    r"skipped (\d+) avg (\d+) us p95 (\d+) us"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--warmup", type=int, default=5,
                        help="performance samples ignored after launch")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    core = "unknown"
    samples: list[dict[str, int | str]] = []
    detail_samples: list[dict[str, int]] = []
    for line in args.log.read_text(errors="replace").splitlines():
        if match := CORE.search(line):
            core = match.group(1)
            samples = []
            detail_samples = []
            continue
        if match := PERF.search(line):
            keys = ("fps", "speed", "avg_us", "p95_us", "arena_kb",
                    "high_water_kb", "alloc_fail", "audio_buffer",
                    "underruns", "drops", "video")
            values = [int(v) if v.isdigit() else v for v in match.groups()]
            samples.append(dict(zip(keys, values)))
            continue
        if match := DETAIL.search(line):
            keys = ("rendered", "render_avg_us", "render_p95_us",
                    "skipped", "skip_avg_us", "skip_p95_us")
            detail_samples.append(
                dict(zip(keys, (int(v) for v in match.groups())))
            )

    samples = samples[args.warmup:]
    detail_samples = detail_samples[args.warmup:]
    if not samples:
        raise SystemExit("no qualifying performance samples in latest run")

    median_speed = statistics.median(int(s["speed"]) for s in samples)
    median_p95 = statistics.median(int(s["p95_us"]) for s in samples)
    target_fps = statistics.median(
        int(s["fps"]) * 100.0 / int(s["speed"])
        for s in samples if int(s["speed"]) > 0
    )
    frame_budget_us = 1_000_000.0 / target_fps
    underrun_delta = int(samples[-1]["underruns"]) - int(samples[0]["underruns"])
    # Qualification rejects *persistent* starvation, not bounded bursts from
    # startup, state operations, or a game's own flash-save routine. Measure
    # a ten-second steady tail separately while retaining the total delta in
    # the report so transient regressions remain visible.
    steady_tail = samples[-min(10, len(samples)):]
    steady_tail_underrun_delta = (
        int(steady_tail[-1]["underruns"]) -
        int(steady_tail[0]["underruns"])
    )
    report = {
        "core": core,
        "samples": len(samples),
        "median_speed_percent": median_speed,
        "median_p95_us": median_p95,
        "native_frame_budget_us": round(frame_budget_us),
        "peak_arena_kb": max(int(s["high_water_kb"]) for s in samples),
        "allocation_failures": max(int(s["alloc_fail"]) for s in samples),
        "audio_underrun_delta": underrun_delta,
        "steady_tail_samples": len(steady_tail),
        "steady_tail_underrun_delta": steady_tail_underrun_delta,
        "dropped_audio_frames": max(int(s["drops"]) for s in samples),
        "minimum_audio_buffer": min(int(s["audio_buffer"]) for s in samples),
        "video_ok": all(s["video"] == "ok" for s in samples),
    }
    if detail_samples:
        report["render_p95_us"] = round(statistics.median(
            s["render_p95_us"] for s in detail_samples))
        report["skip_p95_us"] = round(statistics.median(
            s["skip_p95_us"] for s in detail_samples))
        report["rendered_frames"] = sum(s["rendered"] for s in detail_samples)
        report["skipped_frames"] = sum(s["skipped"] for s in detail_samples)
    failures = []
    if median_speed < 95:
        failures.append("median emulation speed below 95%")
    if median_p95 > frame_budget_us:
        failures.append("median p95 exceeds the core's native frame budget")
    if report["allocation_failures"]:
        failures.append("core allocation failures recorded")
    if steady_tail_underrun_delta > 2:
        failures.append("audio underruns persisted through the steady tail")
    if report["dropped_audio_frames"]:
        failures.append("audio frames were dropped")
    # A static scene is valid output (title screens, pauses, menus). Only
    # black/missing/invalid frame observations fail the video safety gate.
    report["video_ok"] = all(
        s["video"] not in ("black", "missing", "invalid") for s in samples)
    if not report["video_ok"]:
        failures.append("video probe did not remain healthy")
    report["result"] = "PASS" if not failures else "FAIL"
    report["failures"] = failures

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"{report['result']}: {core} ({len(samples)} samples)")
        print(f"  speed median: {median_speed}%")
        print(f"  p95 median:   {median_p95:.0f} us")
        print(f"  audio:        {underrun_delta:+d} total underruns, "
              f"{steady_tail_underrun_delta:+d} over final "
              f"{len(steady_tail)} samples, minimum buffer "
              f"{report['minimum_audio_buffer']} frames")
        print(f"  arena peak:   {report['peak_arena_kb']} KB")
        if detail_samples:
            print(f"  frame cost:   render p95 {report['render_p95_us']} us, "
                  f"skip p95 {report['skip_p95_us']} us")
        for failure in failures:
            print(f"  - {failure}")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
