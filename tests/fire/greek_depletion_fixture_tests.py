"""Exercise the locked standing-pool verdict with controlled probe output."""
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

flag, load, soak, cap = sys.argv[1:]
assert flag == "--depletion-record" and soak == "3"
with open(os.environ["CALLS"], "a") as log:
    print(load, cap, file=log)

fault = os.environ["FAULT"]
limit = min(int(cap), 3) if load == "greek" else int(cap)
if fault == "short_oil" and load == "oil":
    limit -= 1
if fault == "process_error":
    sys.exit(1)

for second in range(limit + 1):
    fuel = 327680
    if load == "greek":
        fuel = max(0, fuel - 109227 * second)
    if fault == "above_zero" and load == "greek":
        fuel = max(fuel, 1)
    if fault == "unmatched" and load == "oil":
        fuel -= 1
    if fault == "oil_loses_fuel" and load == "oil" and second:
        fuel -= second
    wet = 35 if fault == "dry_face" and second == 2 else 36
    water = 56416 if second == 0 else 63732
    if fault == "mismatched_soak" and load == "oil" and second == 0:
        water -= 1
    ticks = 60 * second
    if fault == "partial_tick" and second == 2:
        ticks -= 1
    if fault == "gap" and second == 2:
        continue
    outside = int(fault == "escaped" and second == 2)
    mobile = int(fault == "dripped" and second == 2)
    print("POOL_SAMPLE", second, ticks, float(second), fuel, water, wet, 36,
          outside, mobile)
print("POOL_END", limit)
"""


class GreekDepletionFixtureTests(unittest.TestCase):
    def run_probe(self, fault=""):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "probe"
            binary.write_text(PROBE)
            binary.chmod(0o700)
            calls = Path(directory) / "calls"
            result = subprocess.run(
                [sys.executable, str(TOOL), "--greek-depletion-binary", str(binary)],
                cwd=ROOT,
                env={**os.environ, "FAULT": fault, "CALLS": str(calls)},
                capture_output=True,
                text=True,
                check=False,
            )
            return result, calls.read_text().splitlines() if calls.exists() else []

    def test_zero_crossing_requires_twice_the_completed_oil_time_in_both_replays(self):
        result, calls = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(calls, ["greek 1200", "oil 6", "greek 1200", "oil 6"])
        self.assertEqual(result.stdout.count("Greek depleted by 3 s"), 2)
        self.assertEqual(result.stdout.count("oil retained fuel through 6 s"), 2)

    def test_incomplete_or_unmatched_trajectories_cannot_pass(self):
        failures = {
            "short_oil": "complete oil interval",
            "above_zero": "reach zero fuel",
            "unmatched": "matched initial fuel",
            "oil_loses_fuel": "oil lost fuel",
            "mismatched_soak": "matched post-soak water",
            "dry_face": "submerged",
            "partial_tick": "simulation interval",
            "gap": "simulation interval",
            "escaped": "escaped fuel",
            "dripped": "escaped fuel",
            "process_error": "exit status 1",
        }
        for fault, expected in failures.items():
            with self.subTest(fault=fault):
                result, _ = self.run_probe(fault)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    unittest.main()
