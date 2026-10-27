"""Check direct-source GPU binaries against their private shader ABI."""
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class GpuDependencyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result = subprocess.run(["make", "-qp", "build/contact_gpu.o"],
                                cwd=ROOT, text=True, capture_output=True)
        if result.returncode not in (0, 1):
            raise RuntimeError(result.stderr)
        cls.rules = {}
        for line in result.stdout.splitlines():
            if not line.startswith("build/") or ":" not in line:
                continue
            target, prerequisites = line.split(":", 1)
            if " " not in target:
                cls.rules[target] = set(prerequisites.split())

    def test_contact_object_requires_current_shader(self):
        self.assertIn("build/shaders/contacts.comp.spv",
                      self.rules["build/contact_gpu.o"])
        self.assertIn("src/vulkan/contact_shader_contract.h",
                      self.rules["build/contact_gpu.o"])

    def test_structural_consumers_require_both_carve_shader_modules(self):
        users = {target: deps for target, deps in self.rules.items()
                 if "src/vulkan/structural_gpu.c" in deps}
        self.assertTrue(users)
        required = {"build/shaders/structural_carve.comp.spv",
                    "build/shaders/runtime/structural_carve.comp.spv"}
        missing = {target: sorted(required - deps) for target, deps in users.items()
                   if not required.issubset(deps)}
        self.assertEqual(missing, {})

    def test_carve_shaders_track_shared_physics_headers(self):
        required = {"include/dungeoncraft/body_storage_layout.h",
                    "include/dungeoncraft/material_density.h"}
        for target in ("build/shaders/structural_carve.comp.spv",
                       "build/shaders/runtime/structural_carve.comp.spv"):
            self.assertTrue(required.issubset(self.rules[target]), target)

    def test_direct_source_binaries_require_private_header_and_contact_shader(self):
        users = {target: deps for target, deps in self.rules.items()
                 if not target.endswith(".o") and "src/vulkan/gpu.c" in deps}
        self.assertTrue(users)
        required = {"src/vulkan/gpu_internal.h", "src/vulkan/contact_shader_contract.h",
                    "build/shaders/contacts.comp.spv"}
        missing = {target: sorted(required - deps) for target, deps in users.items()
                   if not required.issubset(deps)}
        self.assertEqual(missing, {})

    def test_minimal_fixture_consumers_rebuild_when_probe_changes(self):
        consumers = {}
        includes = ("tools/diagnostics/vulkan_probe.c", "fluid_transport_tests.c",
                    "fluid_displacement_tests.c")
        for target, deps in self.rules.items():
            sources = [ROOT / p for p in deps
                       if p.startswith("tests/vulkan/") and p.endswith(".c")]
            if any(any(name in source.read_text() for name in includes)
                   for source in sources):
                consumers[target] = deps
        self.assertTrue(consumers)
        missing = sorted(target for target, deps in consumers.items()
                         if "tools/diagnostics/vulkan_probe.c" not in deps)
        self.assertEqual(missing, [])

    def test_thermal_objects_recompile_when_their_source_changes(self):
        for name in ("thermal_gpu", "constructed_thermal_gpu"):
            source = f"src/vulkan/{name}.c"
            target = f"build/{name}.o"
            result = subprocess.run(["make", "-n", "-W", source, target],
                                    cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            commands = result.stdout.splitlines()
            self.assertTrue(any(" -c " in line and source in line and target in line
                                for line in commands), (target, result.stdout))


if __name__ == "__main__":
    unittest.main()
