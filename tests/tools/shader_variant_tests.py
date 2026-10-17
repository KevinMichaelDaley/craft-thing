"""Contracts for reproducible compiler experiments; no GPU submissions."""
import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "fluid_variants", ROOT / "tools/diagnostics/fluid_variants.py")
VARIANTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VARIANTS)


class ShaderVariantTests(unittest.TestCase):
    def test_pixel_render_flag_precedes_shared_geometry_include(self):
        source = (ROOT / "shaders/sim/pattern.comp").read_text()
        candidate = VARIANTS.generate(source, pixel_types=True)
        declaration = "layout(constant_id=1) const bool DIAGNOSTIC_PIXEL_ONLY=false;"
        macro = "#define DC_RIGID_PIXEL_ONLY DIAGNOSTIC_PIXEL_ONLY"
        self.assertIn(declaration, candidate)
        self.assertLess(candidate.index(macro),
                        candidate.index('#include "rigid_body.glsl"'))
        self.assertEqual(candidate.count("params.mode"), 1)

    def test_general_phase_experiment_keeps_shared_body_type_dynamic(self):
        source = (ROOT / "shaders/sim/mpm.comp").read_text()
        candidate = VARIANTS.generate(source)
        self.assertNotIn("DIAGNOSTIC_PIXEL_ONLY", candidate)
        self.assertEqual(candidate.count("params.mode"), 1)


if __name__ == "__main__":
    unittest.main()
