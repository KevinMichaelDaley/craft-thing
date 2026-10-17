"""Numbered-layer exports preserve exact labels and leave level holes untouched."""
from pathlib import Path
import copy
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from ase_reader import read
from make_examples import ase, document
from pack_prop import export, validate


class ProfileTests(unittest.TestCase):
    def test_numbered_layers_map_without_changing_source(self):
        data = document()
        data['frames'] = data['frames'][:1]
        data['tags'] = []
        data['frames'][0]['channels'] = {
            'Layer 1': data['frames'][0]['channels']['color'],
            'Layer 2': data['frames'][0]['channels']['physics.material'],
            'Layer 3': data['frames'][0]['channels']['physics.role']}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source.aseprite'
            encoded = ase(data)
            path.write_bytes(encoded)
            result = read(path, layer_names={'Layer 1': 'color',
                          'Layer 2': 'physics.material', 'Layer 3': 'physics.role'})
            self.assertIn('color', result['frames'][0]['channels'])
            self.assertEqual(path.read_bytes(), encoded)

    def sample(self):
        return {'canvas': [5, 1], 'tags': [], 'mode': 'rgba', 'frames': [{
            'duration_ms': 100, 'channels': {
                'color': bytes((80, 40, 20, 255)) * 5,
                'physics.material': b''.join(bytes(p) for p in (
                    (255, 0, 0, 255), (0, 0, 0, 255), (255, 255, 0, 255),
                    (0, 0, 255, 255), (90, 50, 10, 0))),
                'physics.role': bytes((255, 255, 255, 255)) * 5}}]}

    def test_exact_palette_terrain_and_preserve_holes_roundtrip(self):
        from profile_export import normalize
        profile = ROOT / 'assets/props/profiles/level_piece.json'
        source = self.sample()
        before = copy.deepcopy(source)
        converted = normalize(source, profile)
        self.assertEqual(source, before)
        channels = converted['frames'][0]['channels']
        self.assertEqual(channels['color'][-4:], bytes(4))
        self.assertEqual(channels['physics.role'][-4:], bytes(4))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'piece.prop'
            export(converted, output)
            manifest = validate(output)
            paths = manifest['frames'][0]['channels']
            materials = (output / paths['material']).read_bytes()
            self.assertEqual([int.from_bytes(materials[i:i+2], 'little')
                              for i in range(0, 10, 2)], [2, 1, 7, 6, 0])
            self.assertEqual(list((output / paths['role']).read_bytes()), [1, 1, 1, 1, 0])

    def test_unknown_or_antialiased_labels_are_rejected(self):
        from profile_export import normalize
        for pixel in ((254, 0, 0, 255), (255, 0, 0, 128)):
            source = self.sample()
            source['frames'][0]['channels']['physics.material'] = (
                bytes(pixel) + source['frames'][0]['channels']['physics.material'][4:])
            with self.assertRaisesRegex(ValueError, 'frame 0 pixel 0'):
                normalize(source, ROOT / 'assets/props/profiles/level_piece.json')


if __name__ == '__main__':
    unittest.main()
