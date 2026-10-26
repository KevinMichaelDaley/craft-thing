#!/usr/bin/env python3
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("shader_stats", ROOT / "tools/diagnostics/shader_stats.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ShaderStatsTests(unittest.TestCase):
    def test_markers_associate_simd_statistics(self):
        result = module.rows([
            "Shader compile: build/shaders/gas_finish.comp.spv\n",
            "NIR (SSA form) for compute shader:\n",
            "SIMD8 shader: 123 instructions. 2 loops. 456 cycles. 7:9 spills:fills, 12 sends, scheduled with mode top-down.\n",
            "Shader compile: build/shaders/gas_commit.comp.spv\n",
            "SIMD16 shader: 10 instructions. 0 loops. 20 cycles. 0:0 spills:fills, 3 sends, scheduled with mode non-lifo.\n",
        ])
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["shader"], "build/shaders/gas_finish.comp.spv")
        self.assertEqual(result[0]["spills"], 7)
        self.assertEqual(result[0]["fills"], 9)
        self.assertEqual(result[1]["simd"], 16)
        self.assertEqual(result[1]["spills"], 0)

    def test_unmarked_statistics_are_explicitly_unknown(self):
        result = module.rows(["SIMD8 shader: 1 instructions. 0 loops. 2 cycles. 0:0 spills:fills, 3 sends, scheduled with mode top-down.\n"])
        self.assertEqual(result[0]["shader"], "unknown")


if __name__ == "__main__":
    unittest.main()
