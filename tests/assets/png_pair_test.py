"""Paired PNG imports preserve pixel alignment and explicit metadata labels."""
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from png_pair_export import read_pair
from profile_export import normalize


class PairTests(unittest.TestCase):
    def test_exact_pixels_transparency_and_material_mapping(self):
        with tempfile.TemporaryDirectory() as directory:
            color, material = Path(directory) / 'color.png', Path(directory) / 'material.png'
            image = Image.new('RGBA', (2, 1))
            image.putdata([(73, 91, 123, 255), (91, 23, 40, 255)])
            image.save(color)
            image.putdata([(255, 0, 0, 255), (0, 0, 0, 0)])
            image.save(material)
            document = normalize(read_pair(color, material), ROOT / 'assets/props/profiles/level_piece.json')
            channels = document['frames'][0]['channels']
            self.assertEqual(document['canvas'], [2, 1])
            self.assertEqual(channels['color'], bytes([73, 91, 123, 255, 0, 0, 0, 0]))
            self.assertEqual(channels['physics.material'][:4], bytes([128, 64, 0, 255]))

    def test_mismatched_dimensions_rejected_without_resampling(self):
        with tempfile.TemporaryDirectory() as directory:
            color, material = Path(directory) / 'color.png', Path(directory) / 'material.png'
            Image.new('RGBA', (2, 1)).save(color)
            Image.new('RGBA', (1, 2)).save(material)
            with self.assertRaisesRegex(ValueError, 'dimensions'):
                read_pair(color, material)


if __name__ == '__main__':
    unittest.main()
