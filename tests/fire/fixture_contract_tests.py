"""Locked gameplay fixtures are inputs, never implementation-adjusted oracles."""
import copy
import importlib.util
import json
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets/fixtures/fire_smoke_v1.json"
TOOL = ROOT / "tools/diagnostics/fire_fixture.py"


class FireFixtureContractTests(unittest.TestCase):
    def inputs(self):
        self.assertTrue(MANIFEST.is_file(), "The locked fixture manifest is missing")
        self.assertTrue(TOOL.is_file(), "The permanent fixture tool is missing")
        spec = importlib.util.spec_from_file_location("fire_fixture", TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        data = json.loads(MANIFEST.read_text())
        module.validate(data)
        return module, data

    def test_complete_frame_and_physical_clock_contract_is_locked(self):
        _, data = self.inputs()
        self.assertEqual(data["schema"], 1)
        self.assertEqual(data["view"], [480, 270])
        self.assertEqual(data["simulation"], [640, 448])
        self.assertEqual(data["clock"]["rigid_tick_seconds"], [1, 60])
        self.assertEqual(data["clock"]["rigid_substeps"], 8)
        self.assertEqual(data["clock"]["jacobi_iterations"], 8)
        self.assertEqual(data["clock"]["mpm_substeps"], 2)
        self.assertEqual(data["acceptance"]["frame_budget_ms"], 30)
        self.assertEqual(data["acceptance"]["consecutive_runs"], 2)
        self.assertEqual(data["acceptance"]["measured_frames"], 1000)

    def test_all_three_fixture_maps_are_reproducible_and_keep_material_roles(self):
        module, data = self.inputs()
        ids = {p["name"]: p["id"] for p in data["materials"]}
        self.assertEqual(ids["wood"], 7)
        self.assertEqual(ids["leaves"], 9)
        self.assertNotEqual(ids["ash"], ids["dirt"])
        self.assertNotEqual(ids["ash"], ids["leaf_debris"])
        for name in ("forest_bonfire", "coal_siege", "greek_siege"):
            a = module.build_map(data, name)
            b = module.build_map(data, name)
            self.assertEqual(a, b)
            self.assertEqual(len(a["material"]), 480 * 270 * 2)
            self.assertEqual(len(a["role"]), 480 * 270)
            self.assertEqual(len(a["object"]), 480 * 270 * 4)
            self.assertEqual(len(a["wetness"]), 480 * 270 * 4)
            self.assertEqual(set(a), {"material", "role", "object", "wetness"})
        forest = module.build_map(data, "forest_bonfire")
        stone = struct.unpack_from("<H", forest["material"], (208*480+228)*2)[0]
        self.assertEqual(stone, ids["stone"])
        self.assertEqual(forest["role"][208*480+228], 1)
        broken = module.build_map(data, "forest_bonfire", "dry_bridge")
        self.assertNotEqual(forest["material"], broken["material"])

    def test_locked_siege_ballistic_path_reaches_the_wall_without_teleportation(self):
        _, data = self.inputs()
        siege = data["scenes"]["coal_siege"]
        x, y = siege["release"]["position"]
        vx, vy = siege["release"]["velocity_cells_per_second"]
        wall = siege["wall"]
        time = (wall[0] - x) / vx
        impact_y = y + vy*time + 0.5*data["clock"]["gravity_cells_per_second2"]*time*time
        self.assertGreater(time, 0)
        self.assertGreaterEqual(impact_y, wall[1]+4)
        self.assertLessEqual(impact_y, wall[1]+wall[3]-4)
        self.assertEqual(wall[2], 8)
        self.assertLessEqual(abs(vx), data["clock"]["maximum_rigid_cells_per_second"])

    def test_gameplay_controls_and_finite_chemical_parameters_are_locked(self):
        _, data = self.inputs()
        scenes = data["scenes"]
        self.assertEqual(scenes["forest_bonfire"]["duration_seconds"], 180)
        self.assertEqual(scenes["coal_siege"]["duration_seconds"], 60)
        self.assertEqual(scenes["coal_siege"]["wall_loss_fraction"], [0.1, 0.35])
        self.assertGreaterEqual(scenes["coal_siege"]["wet_control_saturation"], 0.9)
        greek = scenes["greek_siege"]
        self.assertEqual(greek["delayed_launch_seconds"], 10)
        self.assertEqual(len(greek["risk_seeds"]), 64)
        self.assertEqual(len(set(greek["risk_seeds"])), 64)
        self.assertEqual(greek["minimum_launcher_ignitions"], 48)
        self.assertEqual(greek["matched_fuel_mass"], 16)
        self.assertGreater(greek["bound_oxidizer_mass"], 0)
        for profile in data["materials"]:
            self.assertGreater(profile["density"], 0)
            self.assertGreater(profile["heat_capacity"], 0)
            self.assertAlmostEqual(sum(profile["fuel_yields"]), 1)
            self.assertTrue(all(x >= 0 for x in profile["fuel_yields"]))

    def test_invalid_ids_unbalanced_yields_and_out_of_bounds_maps_are_rejected(self):
        module, data = self.inputs()
        bad = copy.deepcopy(data)
        bad["materials"][1]["id"] = bad["materials"][0]["id"]
        with self.assertRaises(ValueError): module.validate(bad)
        bad = copy.deepcopy(data)
        bad["materials"][7]["fuel_yields"][0] = 2
        with self.assertRaises(ValueError): module.validate(bad)
        bad = copy.deepcopy(data)
        bad["scenes"]["forest_bonfire"]["primitives"][0]["rect"][0] = -1
        with self.assertRaises(ValueError): module.validate(bad)


if __name__ == "__main__":
    unittest.main()
