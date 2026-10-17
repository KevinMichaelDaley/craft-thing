import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "gpu_frequency", Path(__file__).parents[2] / "tools/diagnostics/gpu_frequency.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FrequencyTests(unittest.TestCase):
    def test_reads_only_available_integer_frequency_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("gt_act_freq_mhz", "1300\n"),
                                ("gt_cur_freq_mhz", "100\n"),
                                ("gt_bad_freq_mhz", "unsupported")):
                (root / name).write_text(value)
            self.assertEqual(MODULE.snapshot(root), {
                "gt_act_freq_mhz": 1300, "gt_cur_freq_mhz": 100})

    def test_missing_device_is_explicit_empty_snapshot(self):
        self.assertEqual(MODULE.snapshot(Path("/nonexistent/gpu-frequency")), {})


if __name__ == "__main__":
    unittest.main()
