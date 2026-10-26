import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("smoke_trace", ROOT / "tools/diagnostics/smoke_trace.py")
TRACE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRACE)


class SmokeTraceTests(unittest.TestCase):
    def test_present_setup_accounts_tick_service_and_second_event_pump(self):
        rows = TRACE.frames([
            "Present setup tick31.0 service0.2 pump17.0\n",
            "app-frame,9,50.0ms\n",
        ])
        self.assertAlmostEqual(rows[0]["present_tick_ms"], 31.)
        self.assertAlmostEqual(rows[0]["body_service_ms"], .2)
        self.assertAlmostEqual(rows[0]["event_pump_ms"], 17.)

    def test_input_cpu_time_distinguishes_event_work_from_wall_wait(self):
        rows = TRACE.frames([
            "app-input,9,events0,wall17.239,cpu0.051\n",
            "app-frame,9,51.371ms\n",
        ])
        self.assertEqual(rows[0]["input_events"], 0)
        self.assertAlmostEqual(rows[0]["input_cpu_ms"], .051)
        self.assertAlmostEqual(rows[0]["input_wait_ms"], 17.188)

    def test_finish_and_commit_are_gpu_work_not_unexplained_frame_time(self):
        rows = TRACE.frames([
            "Acoustic GPU init4 pressure-group3 residual0.5 ms\n",
            "Acoustic GPU tail finish2 commit1 ms\n",
            "app-frame,9,15.0ms\n",
        ])
        self.assertAlmostEqual(rows[0]["finish_ms"], 2.)
        self.assertAlmostEqual(rows[0]["commit_ms"], 1.)
        self.assertAlmostEqual(rows[0]["gpu_ms"], 10.5)
        self.assertAlmostEqual(rows[0]["remaining_ms"], 4.5)

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

    def test_delayed_stall_accounts_retries_activity_waits_and_fixed_storage(self):
        rows = TRACE.frames([
            "Acoustic dt0.02 batch1 residual0.0002 error0 tiles40 advected80 sweeps32 group1 large1\n",
            "Gas interval dt0.02 accepted1 retries0 submits2 queue0.1 wait12.0 storage1024\n",
            "Present waits acquire0.1 fence0.2\n", "app-frame,9,20.0ms\n",
            "Acoustic dt0.01 batch3 residual0.1 error0 tiles2000 advected2800 sweeps16 group2 large1\n",
            "Gas retry dt0.01 speed200 large1 stages4 cell123\n",
            "Gas interval dt0.25 accepted30 retries20 submits90 queue1.0 wait900.0 storage1024\n",
            "Present waits acquire0.2 fence0.3\n", "app-frame,10,1000.0ms\n",
        ])
        self.assertEqual(rows[1]["retries"], 20)
        self.assertEqual(rows[1]["accepted"], 30)
        self.assertEqual(rows[1]["solve_tiles_peak"], 2000)
        self.assertAlmostEqual(rows[1]["gas_wait_ms"], 900.0)
        self.assertAlmostEqual(rows[1]["acquire_ms"], 0.2)
        self.assertEqual(rows[0]["storage_bytes"], rows[1]["storage_bytes"])


if __name__ == "__main__":
    unittest.main()
