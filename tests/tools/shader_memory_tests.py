#!/usr/bin/env python3
"""The fused hybrid transport must fit two groups in a 64-KiB shared budget."""
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("shader_memory", ROOT / "tools/diagnostics/shader_memory.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ShaderMemoryTests(unittest.TestCase):
    def test_invalid_spirv_rejected(self):
        with self.assertRaises(ValueError):
            module.workgroup_bytes(b"invalid")

    def test_hybrid_transport_shared_storage_fits_two_groups(self):
        for name in ("transport_hybrid", "transport_hybrid_wide"):
            with self.subTest(shader=name):
                size = module.workgroup_bytes((ROOT / f"build/shaders/gas_{name}.comp.spv").read_bytes())
                self.assertGreater(size, 0)
                self.assertLessEqual(size, 32768)


if __name__ == "__main__":
    unittest.main()
