import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
PROFILE = ROOT / "tools/smoke_profile"


class SmokeProfileTests(unittest.TestCase):
    def environment(self, overrides=None):
        env = {k: v for k, v in os.environ.items() if not k.startswith("DC_GAS_")}
        env.update(overrides or {})
        result = subprocess.run(
            ["bash", "-c", 'source "$1" && env', "profile-test", str(PROFILE)],
            env=env, capture_output=True, text=True, check=True)
        return dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)

    def test_runnable_profile_uses_bounded_measured_backend(self):
        env = self.environment()
        for option in ("PRACTICAL", "DEVICE_WORK", "FIXED_AXIS", "FUSED_HYBRID",
                       "DIRECT_FIRST_AXIS", "GROUP_RATE", "HALF_PRESSURE",
                       "MULTIGRID", "FUSED_METADATA", "TEMPORAL_SOR", "COARSE_WG",
                       "COARSE", "ROTATE_PUBLISH", "HYBRID_LINES", "FUSED_AXIS",
                       "FUSED_APPLY", "WIDE_SCAN", "LOCAL_GEOMETRY"):
            self.assertEqual(env.get("DC_GAS_" + option), "1", option)
        for option in ("ZERO_ENERGY", "LEAN_METADATA", "CACHED_NEIGHBORS",
                       "COMPUTE_BARRIERS", "PACKED_PRESSURE", "MG_BOUNDARY"):
            self.assertEqual(env.get("DC_GAS_" + option, "0"), "0", option)
        self.assertNotIn("DC_GAS_LINE_GROUP", env)

    def test_diagnostic_overrides_remain_explicit(self):
        env = self.environment({"DC_GAS_DEVICE_WORK": "0", "DC_GAS_LINE_GROUP": "512", "DC_GAS_LOCAL_GEOMETRY": "0"})
        self.assertEqual(env["DC_GAS_DEVICE_WORK"], "0")
        self.assertEqual(env["DC_GAS_LINE_GROUP"], "512")
        self.assertEqual(env["DC_GAS_LOCAL_GEOMETRY"], "0")


if __name__ == "__main__":
    unittest.main()
