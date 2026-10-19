#!/usr/bin/env python3
"""Bounded real-application capture must exit through the normal save path."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DisplayBenchmarkTest(unittest.TestCase):
    def test_displayed_scene_capture_saves_and_exits(self):
        work = Path(tempfile.mkdtemp(prefix="display_benchmark_", dir=ROOT / "build"))
        capture = work / "scene.bmp"
        app = os.environ.get("DC_TEST_APP", "build/dungeoncraft_quarter_native")
        result = subprocess.run(
            [app, "--world-dir", str(work / "world"), "--scene",
             "assets/props/level_piece03.prop/asset.cprop",
             "--benchmark-frames", "4", "--benchmark-capture", str(capture)],
            cwd=ROOT, text=True, capture_output=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Displayed full frame", result.stdout)
        self.assertIn("paused pixels identical", result.stdout)
        self.assertTrue(capture.is_file())
        self.assertTrue(any((work / "world").rglob("*.bin")))


if __name__ == "__main__":
    unittest.main()
