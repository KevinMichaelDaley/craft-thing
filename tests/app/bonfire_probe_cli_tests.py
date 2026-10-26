"""The displayed bonfire benchmark must honor its scripted contact input."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class BonfireProbeCliTests(unittest.TestCase):
    def test_invalid_scripted_probe_is_rejected_before_scene_creation(self):
        root = Path(__file__).resolve().parents[2]
        environment = dict(os.environ, SDL_VIDEODRIVER="wayland",
                           DC_BENCHMARK_HEAT_PROBE="999,999,0")
        with tempfile.TemporaryDirectory(prefix="bonfire_cli_", dir=root / "build") as world:
            result = subprocess.run(
                ["tools/with-vulkan", "./build/dungeoncraft_quarter_native",
                 "--bonfire", "--world-dir", world, "--benchmark-frames", "1"],
                cwd=root, env=environment, text=True, capture_output=True, timeout=45)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("DC_BENCHMARK_HEAT_PROBE requires", result.stderr)


if __name__ == "__main__":
    unittest.main()
