from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import core_lab  # noqa: E402


class CoreLabTest(unittest.TestCase):
    def capture(self, argv: list[str]) -> tuple[int, str, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = core_lab.main(argv)
        return result, stdout.getvalue(), stderr.getvalue()

    def test_unknown_core_is_actionable(self) -> None:
        result, _, error = self.capture(["--dry-run", "build", "no-such-core"])
        self.assertEqual(result, 2)
        self.assertIn("unknown core 'no-such-core'", error)
        self.assertIn("gambatte", error)

    def test_build_dry_run_does_not_create_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary) / "never-created"
            result, output, error = self.capture([
                "--dry-run", "build", "gambatte", "--build-dir", str(build),
            ])
            self.assertEqual(result, 0, error)
            self.assertIn("rs_core_gambatte", output)
            self.assertIn("Next: ./build.sh core-package gambatte", output)
            self.assertFalse(build.exists())

    def test_archived_core_needs_explicit_override(self) -> None:
        result, _, error = self.capture([
            "--dry-run", "package", "mgba",
        ])
        self.assertEqual(result, 2)
        self.assertIn("archived", error)
        self.assertIn("--allow-archived", error)

    def test_smoke_rejects_unsupported_system_before_running(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            rom = Path(temporary) / "fixture.gbc"
            rom.write_bytes(b"test fixture")
            result, _, error = self.capture([
                "--dry-run", "smoke", "gambatte", "--rom", str(rom),
                "--system", "gba",
            ])
            self.assertEqual(result, 2)
            self.assertIn("supports gb|gbc, not gba", error)

    def test_smoke_dry_run_prints_authoritative_runner(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            rom = Path(temporary) / "fixture.gbc"
            rom.write_bytes(b"test fixture")
            runs = Path(temporary) / "runs"
            result, output, error = self.capture([
                "--dry-run", "smoke", "gambatte", "--rom", str(rom),
                "--system", "gbc", "--runs-dir", str(runs),
            ])
            self.assertEqual(result, 0, error)
            self.assertIn("run_ppsspp_automation.py", output)
            self.assertIn("--psp-model 0", output)
            self.assertIn("PPSSPP is a correctness check", output)
            self.assertFalse(runs.exists())

    def test_report_dry_run_does_not_write_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "result.json"
            output = root / "report.md"
            source.write_text(json.dumps({
                "result": "PASS",
                "system": "gbc",
                "core": "gambatte",
                "rom": {"sha256": "a" * 64},
                "checks": {"video_valid": True},
                "metrics": {"median_speed_percent": 100,
                            "median_p95_us": 1000,
                            "peak_arena_kb": 512,
                            "audio_drops_max": 0,
                            "video_valid": True},
            }))
            result, text, error = self.capture([
                "--dry-run", "report", "gambatte", "--result", str(source),
                "--output", str(output),
            ])
            self.assertEqual(result, 0, error)
            self.assertIn("# Core Lab report: gambatte", text)
            self.assertIn("PPSSPP is a correctness gate", text)
            self.assertFalse(output.exists())

    def test_packager_refuses_archived_core_by_default(self) -> None:
        completed = subprocess.run([
            sys.executable, str(ROOT / "tools/package_release.py"),
            "--core", "mgba", "--build-dir", str(ROOT / "missing-build"),
        ], cwd=ROOT, text=True, capture_output=True)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("refusing to package archived core", completed.stderr)


if __name__ == "__main__":
    unittest.main()
