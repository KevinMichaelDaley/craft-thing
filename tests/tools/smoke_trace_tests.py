import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("smoke_trace", ROOT / "tools/diagnostics/smoke_trace.py")
TRACE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRACE)


class SmokeTraceTests(unittest.TestCase):
    def test_groups_accumulate_until_each_presented_frame(self):
        lines = [
            "Acoustic GPU init0.5 pressure-group2.0 residual0.1 ms\n",
            "Acoustic GPU init0.0 pressure-group1.0 residual0.2 ms\n",
            "app-frame,8,10.000ms\n",
            "Acoustic GPU init0.25 pressure-group1.5 residual0.05 ms\n",
            "app-frame,9,6.000ms\n",
        ]
        frames = TRACE.frames(lines)
        self.assertEqual(len(frames), 2)
        self.assertEqual(frames[0]["substeps"], 1)
        self.assertEqual(frames[0]["submissions"], 2)
        self.assertAlmostEqual(frames[0]["gpu_ms"], 3.8)
        self.assertAlmostEqual(frames[1]["remaining_ms"], 4.2)

    def test_truncated_tail_does_not_invent_a_complete_frame(self):
        self.assertEqual(TRACE.frames(["Acoustic GPU init1 pressure-group2 residual0 ms\n"]), [])


if __name__ == "__main__":
    unittest.main()
