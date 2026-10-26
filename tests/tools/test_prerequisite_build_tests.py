import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("prerequisites", ROOT / "tools/build_test_prerequisites.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PrerequisiteBuildTests(unittest.TestCase):
    def test_nested_phony_dependencies_select_only_reachable_build_files(self):
        database = """test: build/alpha test_extra
build/alpha: src/alpha.c build/common.o
test_extra: build/beta
build/beta: build/common.o
build/common.o: src/common.c
build/unrelated: src/unrelated.c
"""
        self.assertEqual(MODULE.build_targets(database), ["build/alpha", "build/beta", "build/common.o"])

    def test_cycles_do_not_repeat_or_hang(self):
        self.assertEqual(MODULE.build_targets("test: check\ncheck: test build/right\n"), ["build/right"])

    def test_variables_comments_and_recipes_are_not_targets(self):
        database = "VALUE := build/wrong\n# test: build/also_wrong\n\tbuild/not_a_target: false\ntest: build/right\n"
        self.assertEqual(MODULE.build_targets(database), ["build/right"])

    def test_missing_test_root_fails_explicitly(self):
        with self.assertRaises(ValueError):
            MODULE.build_targets("build/unrelated: source.c\n")


if __name__ == "__main__":
    unittest.main()
