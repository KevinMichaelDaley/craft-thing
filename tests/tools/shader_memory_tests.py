#!/usr/bin/env python3
"""Bound hybrid scratch; actual occupancy also depends on hardware/registers."""
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("shader_memory", ROOT / "tools/diagnostics/shader_memory.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ShaderMemoryTests(unittest.TestCase):
    def test_practical_only_axis_binaries_do_not_branch_on_runtime_policy(self):
        for name in ("transport_axis_compact", "transport_axis_compact_wide", "transport_axis_compact_large",
                     "transport_explicit", "transport_explicit_wide", "transport_explicit_large"):
            with self.subTest(shader=name):
                instructions = list(module._instructions((ROOT / f"build/shaders/gas_{name}.comp.spv").read_bytes()))
                policy_constants = {args[1] for opcode, args in instructions if opcode == 43 and args[2] == 32768}
                tests_policy = any(opcode == 199 and any(value in policy_constants for value in args[-2:])
                                   for opcode, args in instructions)
                self.assertFalse(tests_policy, "A practical-only backend must specialize its uniform policy branches")

    def test_invalid_spirv_rejected(self):
        with self.assertRaises(ValueError):
            module.workgroup_bytes(b"invalid")

    def test_hybrid_transport_shared_storage_stays_below_32kib(self):
        for name in ("transport_hybrid", "transport_hybrid_wide", "transport_hybrid_x", "transport_hybrid_y", "transport_hybrid_x_wide", "transport_hybrid_y_wide", "transport_hybrid_large", "transport_hybrid_x_large", "transport_hybrid_y_large"):
            with self.subTest(shader=name):
                size = module.workgroup_bytes((ROOT / f"build/shaders/gas_{name}.comp.spv").read_bytes())
                self.assertGreater(size, 0)
                self.assertLessEqual(size, 32768)

    def test_selected_y_axis_scratch_allows_three_groups_in_64kib(self):
        data = (ROOT / "build/shaders/gas_transport_hybrid_y_wide.comp.spv").read_bytes()
        self.assertLessEqual(module.workgroup_bytes(data), 65536 // 3)

    def test_large_hybrid_binary_uses_512_threads(self):
        for name in ("transport_hybrid_large", "transport_hybrid_x_large", "transport_hybrid_y_large"):
            with self.subTest(shader=name):
                self.assertEqual(module.local_size((ROOT / f"build/shaders/gas_{name}.comp.spv").read_bytes()), (512, 1, 1))


if __name__ == "__main__":
    unittest.main()
