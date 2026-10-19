from pathlib import Path
import struct
import sys
import tempfile
import unittest

from libresprite_fixture import fixture

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from ase_reader import read
from make_examples import ase, document


class ReaderTests(unittest.TestCase):
    def test_rgba_indexed_hidden_linked_and_tags_identical(self):
        expected = document()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'example.aseprite'
            for indexed in (False, True):
                path.write_bytes(ase(expected, indexed))
                actual = read(path)
                self.assertEqual(actual['frames'], expected['frames'])
                self.assertEqual(actual['tags'], expected['tags'])
                self.assertFalse(actual['layers'][0]['flags'] & 1)

    def test_real_offsets_and_transparent_holes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'offset.aseprite'
            fixture(path)
            data = read(path)
            for frame in data['frames']:
                material = frame['channels']['physics.material']
                self.assertEqual(material[44:48], bytes((96, 96, 96, 255)))
                self.assertEqual(material[48:52], bytes(4))
                self.assertEqual(material[:44], bytes(44))

    def test_rejects_duplicate_reserved_layers(self):
        data = document()
        encoded = ase(data)
        encoded = encoded.replace(b'physics.role', b'physics.role')
        # Equal-length reserved name collision preserves binary layout.
        encoded = encoded.replace(b'physics.object', b'physics.anchor')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'duplicate.aseprite'
            path.write_bytes(encoded)
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                read(path)

    def test_rejects_oversized_frame_and_truncation(self):
        encoded = bytearray(ase(document()))
        struct.pack_into('<I', encoded, 128, 0xffffffff)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad.aseprite'
            path.write_bytes(encoded)
            with self.assertRaises(ValueError):
                read(path)
            path.write_bytes(bytes(10))
            with self.assertRaises(ValueError):
                read(path)

    def test_rejects_indexed_partial_alpha_in_actual_host_format(self):
        data = document()
        emission = bytearray(data['frames'][0]['channels']['emissive'])
        emission[(5 * 48 + 5) * 4 + 3] = 128
        data['frames'][0]['channels']['emissive'] = bytes(emission)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'unsupported.aseprite'
            path.write_bytes(ase(data, True))
            with self.assertRaisesRegex(ValueError, 'RGBA'):
                read(path)


if __name__ == '__main__':
    unittest.main()
