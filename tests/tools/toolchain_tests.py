import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("bootstrap_vulkan", REPO / "tools/bootstrap_vulkan.py")
BOOTSTRAP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BOOTSTRAP)


class ToolchainTests(unittest.TestCase):
    def test_verified_package_is_reused_offline(self):
        with tempfile.TemporaryDirectory(dir=REPO / "build") as directory:
            base = pathlib.Path(directory)
            source = base / "source.deb"
            source.write_bytes(b"verified package fixture")
            package = {"name": "fixture", "url": source.as_uri(),
                       "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
            cached = BOOTSTRAP.fetch_package(package, base / "cache", offline=False)
            source.unlink()
            self.assertEqual(BOOTSTRAP.fetch_package(package, base / "cache", offline=True), cached)

    def test_bad_download_is_never_published(self):
        with tempfile.TemporaryDirectory(dir=REPO / "build") as directory:
            base = pathlib.Path(directory)
            source = base / "source.deb"
            source.write_bytes(b"corrupted package")
            package = {"name": "fixture", "url": source.as_uri(), "sha256": "0" * 64}
            with self.assertRaisesRegex(ValueError, "SHA256"):
                BOOTSTRAP.fetch_package(package, base / "cache", offline=False)
            self.assertFalse(list((base / "cache").glob("*.deb")))

    def test_corrupted_cache_cannot_be_used_offline(self):
        with tempfile.TemporaryDirectory(dir=REPO / "build") as directory:
            base = pathlib.Path(directory)
            source = base / "source.deb"
            source.write_bytes(b"original")
            package = {"name": "fixture", "url": source.as_uri(),
                       "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
            cached = BOOTSTRAP.fetch_package(package, base / "cache", offline=False)
            cached.write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "SHA256"):
                BOOTSTRAP.fetch_package(package, base / "cache", offline=True)

    def test_environment_uses_install_and_preserves_driver_selection(self):
        root = REPO / ".tools/vulkan/install-test/root"
        original = {"PATH": "/usr/bin", "CPATH": "/custom/include",
                    "VK_ICD_FILENAMES": "/custom/driver.json"}
        saved = dict(original)
        env = BOOTSTRAP.tool_environment(root, original)
        self.assertTrue(env["PATH"].startswith(str(root / "usr/bin") + ":"))
        self.assertIn("/custom/include", env["CPATH"])
        self.assertEqual(env["VK_ICD_FILENAMES"], original["VK_ICD_FILENAMES"])
        self.assertEqual(original, saved)

    def test_every_locked_tool_has_a_version_and_digest(self):
        lock = json.loads((REPO / "tools/vulkan.lock.json").read_text())
        names = set()
        for package in lock["packages"]:
            self.assertNotIn(package["name"], names)
            names.add(package["name"])
            self.assertTrue(package["version"])
            self.assertRegex(package["sha256"], r"^[a-f0-9]{64}$")
            self.assertTrue(package["url"].startswith("https://archive.ubuntu.com/ubuntu/"))
        self.assertEqual(names, {"glslang-tools", "spirv-tools", "libvulkan-dev",
                                "libvulkan1", "vulkan-validationlayers", "vulkan-tools"})


if __name__ == "__main__":
    (REPO / "build").mkdir(exist_ok=True)
    unittest.main()
