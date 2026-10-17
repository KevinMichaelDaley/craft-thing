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

    def test_direct_source_binaries_require_private_header_and_contact_shader(self):
        users = {target: deps for target, deps in self.rules.items()
                 if not target.endswith(".o") and "src/vulkan/gpu.c" in deps}
        self.assertTrue(users)
        required = {"src/vulkan/gpu_internal.h", "src/vulkan/contact_shader_contract.h",
                    "build/shaders/contacts.comp.spv"}
        missing = {target: sorted(required - deps) for target, deps in users.items()
                   if not required.issubset(deps)}
        self.assertEqual(missing, {})


if __name__ == "__main__":
    unittest.main()
