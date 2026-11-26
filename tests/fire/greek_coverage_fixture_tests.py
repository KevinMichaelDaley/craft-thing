"""Verify that the displayed Greek siege gate checks the complete shot."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/diagnostics/fire_fixture.py"

PROBE = """#!/usr/bin/env python3
import os
import sys

flag, seed, ticks, opacity, emission = sys.argv[1:]
assert (flag, seed, ticks, opacity, emission) == (
    "--coverage-record", "991397", "60", "0.2", "0.1")
fault = os.environ["FAULT"]
if fault == "process_error":
    sys.exit(1)
release = 3
last = release + int(ticks) - (fault == "short")
for tick in range(last + 1):
    if tick == 2 and fault == "skipped":
        continue
    seconds = tick / 60
    if tick == 2 and fault == "short_clock":
        seconds -= 0.01
    pixels = 129600
    if tick == 2 and fault == "viewport":
        pixels += 1
    smoke, flame, covered = 0, 0, 0
    if tick >= release and fault != "dark":
        smoke, flame, covered = 20000, 20000, 32400
    if fault == "burst" and tick == release + 1:
        covered += 1
    if fault == "before_release_burst" and tick == 1:
        smoke, covered = 32401, 32401
    if fault == "invalid_union" and tick == release + 1:
        covered = 19999
    print("COVERAGE_FRAME", tick, tick, seconds, pixels, smoke, flame, covered)
    if tick == release and fault != "never_released":
        print("COVERAGE_RELEASE", release)
"""


class GreekCoverageFixtureTests(unittest.TestCase):
    def run_probe(self, fault=""):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "probe"
            binary.write_text(PROBE)
            binary.chmod(0o700)
            return subprocess.run(
                [sys.executable, str(TOOL), "--greek-coverage-binary", str(binary)],
                cwd=ROOT,
                env={**os.environ, "FAULT": fault},
                capture_output=True,
                text=True,
                check=False,
            )

    def test_largest_union_counts_overlap_once_and_accepts_boundary(self):
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("32400 of 129600 pixels", result.stdout)

    def test_short_dark_or_over_budget_shot_cannot_pass(self):
        failures = {
            "short": "complete first simulated second",
            "skipped": "complete simulation tick",
            "short_clock": "complete simulation tick",
            "viewport": "locked viewport",
            "dark": "no visible flame",
            "burst": "exceeded viewport coverage",
            "before_release_burst": "exceeded viewport coverage",
            "invalid_union": "Invalid smoke and flame union",
            "never_released": "complete first simulated second",
            "process_error": "exit status 1",
        }
        for fault, expected in failures.items():
            with self.subTest(fault=fault):
                result = self.run_probe(fault)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    unittest.main()
