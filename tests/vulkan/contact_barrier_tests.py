"""Guard a shared-predicate handoff without submitting a deadlocking shader."""
import pathlib
import re
import unittest


class ContactBarrierTests(unittest.TestCase):
    def test_terrain_return_predicate_is_captured_before_shared_reset(self):
        source = (pathlib.Path(__file__).resolve().parents[2] /
                  "shaders/sim/contacts.comp").read_text()
        # A subgroup may reach the reset while another has not read the predicate.
        # Every lane must capture it, rendezvous, then branch on its private copy.
        self.assertRegex(source, re.compile(
            r"bool\s+empty_terrain\s*=\s*group_found\s*==\s*0u\s*;"
            r"\s*barrier\(\);\s*if\s*\(empty_terrain\)", re.S))
        self.assertNotRegex(source, r"if\s*\(group_found\s*==\s*0u\)")


if __name__ == "__main__":
    unittest.main()
