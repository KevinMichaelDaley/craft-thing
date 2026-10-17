"""Guard a shared-predicate handoff without submitting a deadlocking shader."""
import pathlib
import re
import unittest


def unsafe_shared_return(source):
    source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
    for predicate in re.finditer(r"if\s*\(group_found\s*==\s*0u\)", source):
        statement = source[predicate.end():].lstrip()
        if statement.startswith("{"):
            depth, end = 1, 1
            while depth and end < len(statement):
                depth += (statement[end] == "{") - (statement[end] == "}")
                end += 1
            statement = statement[:end]
        else:
            statement = statement.split(";", 1)[0]
        if re.search(r"\breturn\b", statement):
            return True
    return False


class ContactBarrierTests(unittest.TestCase):
    def test_guard_accepts_stats_reads_and_rejects_shared_group_returns(self):
        self.assertFalse(unsafe_shared_return(
            "if (lane == 0u) { if(group_found==0u) atomicAdd(skipped,1u); }"))
        self.assertTrue(unsafe_shared_return(
            "if(group_found==0u) { atomicAdd(skipped,1u); return; }"))
        self.assertTrue(unsafe_shared_return(
            "if (group_found == 0u) return;"))
        self.assertTrue(unsafe_shared_return(
            "if(group_found==0u) { /* } */ if(lane==0u) { atomicAdd(x,1u); } return; }"))

    def test_terrain_return_predicate_is_captured_before_shared_reset(self):
        source = (pathlib.Path(__file__).resolve().parents[2] /
                  "shaders/sim/contacts.comp").read_text()
        # A subgroup may reach the reset while another has not read the predicate.
        # Every lane must capture it, rendezvous, then branch on its private copy.
        self.assertRegex(source, re.compile(
            r"bool\s+empty_terrain\s*=\s*group_found\s*==\s*0u\s*;"
            r"\s*barrier\(\);\s*if\s*\(empty_terrain\)", re.S))
        self.assertFalse(unsafe_shared_return(source))

    def test_hydro_floor_is_captured_before_reusing_reduction_scratch(self):
        source = (pathlib.Path(__file__).resolve().parents[2] /
                  "shaders/sim/pixel_hydro.comp").read_text()
        self.assertRegex(source, re.compile(
            r"uint\s+floor_y\s*=\s*counts\[0\];\s*barrier\(\);", re.S))

    def test_hydro_water_start_is_captured_before_reusing_counts(self):
        source = (pathlib.Path(__file__).resolve().parents[2] /
                  "shaders/sim/pixel_hydro.comp").read_text()
        self.assertRegex(source, re.compile(
            r"uint\s+water_begin\s*=\s*counts\[0\];\s*barrier\(\);", re.S))
        self.assertNotRegex(source, r"for\s*\(uint\s+y\s*=\s*counts\[0\]")


if __name__ == "__main__":
    unittest.main()
