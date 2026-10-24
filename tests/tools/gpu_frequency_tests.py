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

    def test_drm_clients_deduplicate_shared_descriptors_and_processes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for pid, fd, client, device, count in (
                    (123, 4, 17, "0000:00:02.0", 200),
                    (123, 5, 17, "0000:00:02.0", 250),
                    (456, 4, 17, "0000:00:02.0", 300),
                    (456, 5, 17, "0000:01:00.0", 100)):
                folder = root / str(pid)
                (folder / "fdinfo").mkdir(parents=True, exist_ok=True)
                (folder / "comm").write_text("browser\n")
                (folder / "fdinfo" / str(fd)).write_text(
                    f"drm-client-id:\t{client}\ndrm-pdev:\t{device}\n"
                    f"drm-engine-render:\t{count} ns\n"
                    "drm-engine-copy:\t0 ns\ndrm-engine-capacity-render:\t1\n")
            clients = MODULE.drm_clients(root)
            self.assertEqual(len(clients), 2)
            self.assertEqual(clients["0000:00:02.0/17"], {
                "pids": [123, 456], "names": ["browser"],
                "engines_ns": {"render": 300, "copy": 0}})

    def test_drm_client_snapshot_ignores_incomplete_or_gone_processes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            folder = root / "123" / "fdinfo"
            folder.mkdir(parents=True)
            (folder / "4").write_text("pos: 0\n")
            (folder / "5").write_text("drm-client-id: 2\ndrm-engine-render: bad ns\n")
            self.assertEqual(MODULE.drm_clients(root), {})
        self.assertEqual(MODULE.drm_clients(Path("/nonexistent/drm-proc")), {})


if __name__ == "__main__":
    unittest.main()
