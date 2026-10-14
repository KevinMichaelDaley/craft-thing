import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.schema = json.loads((ROOT / 'assets/props/schema-v1.json').read_text())

    def test_material_names_have_explicit_distinct_namespaces(self):
        materials = {m['name']: m for m in self.schema['materials']}
        self.assertEqual(materials['wood']['terrain'], 7)
        self.assertEqual(materials['wood']['body'], 2)
        self.assertEqual(materials['dense_wood']['terrain'], 8)
        self.assertEqual(materials['dense_wood']['body'], 4)
        self.assertIsNone(materials['mortar']['terrain'])
        self.assertEqual(materials['mortar']['body'], 3)
        self.assertIsNone(materials['sand']['body'])

    def test_exact_opaque_legends_and_channel_contract(self):
        for legend in ('materials', 'roles'):
            colors = [tuple(item['rgba']) for item in self.schema[legend]]
            self.assertEqual(len(colors), len(set(colors)))
            self.assertTrue(all(c[3] == 255 for c in colors))
        self.assertEqual(self.schema['physics_animation'], 'visual_only')
        self.assertEqual(self.schema['connectivity'], 4)
        self.assertEqual(self.schema['missing_cel'], 'transparent')
        self.assertEqual(self.schema['appearance_runtime'], 'fr_appearance_texel_t-v1')
        self.assertIn('emissive', self.schema['channels'])


if __name__ == '__main__':
    unittest.main()
