import os
import pathlib
import subprocess
import tempfile
import unittest


class HaloFixtureTests(unittest.TestCase):
    def test_context_creation_failure_reports_vulkan_error(self):
        with tempfile.TemporaryDirectory() as directory:
            missing_driver = str(pathlib.Path(directory) / "missing.json")
            env = dict(os.environ, VK_DRIVER_FILES=missing_driver, VK_ICD_FILENAMES=missing_driver)
            output = subprocess.run(["./build/halo_tests"], env=env, text=True,
                                    capture_output=True, check=False)

        self.assertEqual(output.returncode, 1, output.stdout + output.stderr)
        failures = [line for line in output.stdout.splitlines() if line.startswith("FAIL ")]
        self.assertEqual(len(failures), 4, output.stdout)
        for line in failures:
            self.assertIn("Cannot create Vulkan 1.3 instance", line)
        self.assertNotIn("OK   ", output.stdout)
        self.assertIn("0 passed, 4 failed", output.stdout)


if __name__ == "__main__":
    unittest.main()
