import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))


def document():
    return {'canvas': [3, 2], 'tags': [], 'mode': 'rgba', 'frames': [
        {'duration_ms': duration, 'channels': {
            'physics.material': bytes((128, 64, 0, 255, 0, 0, 0, 0)) * 3,
            'physics.role': bytes((0, 0, 255, 255, 0, 0, 0, 0)) * 3,
            'color': bytes((150 + index, 90, 20, 255, 0, 0, 0, 0)) * 3,
            'emissive': bytes((255, 80, 10, 255, 0, 0, 0, 0)) * 3}}
        for index, duration in enumerate((90, 170))]}


class PackageTests(unittest.TestCase):
    def setUp(self):
        import pack_prop
        self.packer = pack_prop

    def test_valid_multichannel_deterministic_and_typed_maps(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / 'a', Path(directory) / 'b'
            self.packer.export(document(), first)
            self.packer.export(document(), second)
            self.assertEqual((first / 'manifest.json').read_bytes(), (second / 'manifest.json').read_bytes())
            manifest = self.packer.validate(first)
            self.assertEqual(manifest['canvas'], [3, 2])
            self.assertEqual([f['duration_ms'] for f in manifest['frames']], [90, 170])
            self.assertEqual((first / '0000.material.u16').read_bytes(), bytes((2, 0, 0, 0)) * 3)
            self.assertTrue((first / 'asset.cprop').is_file())

    def test_rejects_partial_alpha_unknown_material_and_rigid_sand(self):
        for rgba in ((128, 64, 0, 254), (1, 2, 3, 255), (255, 224, 0, 255)):
            data = document()
            data['frames'][0]['channels']['physics.material'] = bytes(rgba) * 6
            with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
                self.packer.export(data, Path(directory) / 'bad')

    def test_physics_frames_invariant_or_explicit_variants(self):
        data = document()
        data['frames'][1]['channels']['physics.role'] = bytes((0, 255, 0, 255)) * 6
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(ValueError, 'frame 1.*pixel'):
            self.packer.export(data, Path(directory) / 'bad')

    def test_atomic_failure_preserves_previous_export(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'asset'
            self.packer.export(document(), output)
            before = (output / 'manifest.json').read_bytes()
            data = document()
            data['frames'][0]['channels']['physics.material'] = b''
            with self.assertRaises(ValueError):
                self.packer.export(data, output)
            self.assertEqual((output / 'manifest.json').read_bytes(), before)

    def test_rejects_corruption_and_escaping_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'asset'
            self.packer.export(document(), output)
            manifest = json.loads((output / 'manifest.json').read_text())
            manifest['files']['../escape'] = '0' * 64
            (output / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'path'):
                self.packer.validate(output)

    def test_holes_do_not_gain_physics_from_color(self):
        data = document()
        for frame in data['frames']:
            frame['channels']['color'] = bytes((200, 50, 20, 255)) * 6
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'asset'
            self.packer.export(data, output)
            self.assertEqual((output / '0000.material.u16').read_bytes(), bytes((2, 0, 0, 0)) * 3)


if __name__ == '__main__':
    unittest.main()
