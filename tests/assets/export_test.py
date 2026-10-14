import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from libresprite_fixture import fixture

ROOT = Path(__file__).resolve().parents[2]


class ExportTests(unittest.TestCase):
    def test_real_script_exports_active_snapshot_without_source_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / 'input.aseprite'
            fixture(source)
            original = source.read_bytes()
            script = directory / 'export.js'
            code = (ROOT / 'scripts/libresprite/export_prop.js').read_text()
            # The script reads ACTIVE pixels after this unsaved mutation.
            script.write_text('app.activeSprite.layer(2).cel(0).image.putPixel(0,0,pixelColor.rgba(1,2,3,255));\n' + code)
            result = subprocess.run([os.environ['LIBRESPRITE'], '-b', str(source), '--script', str(script)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            request_path = next(line.split('=', 1)[1] for line in result.stdout.splitlines()
                                if line.startswith('CRAFT_PROP_REQUEST='))
            request = json.loads(Path(request_path).read_text())
            self.assertEqual(request['source'], str(source))
            self.assertTrue(Path(request['snapshot']).is_file())
            spec = importlib.util.spec_from_file_location('ase_reader', ROOT / 'scripts/props/ase_reader.py')
            reader = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(reader)
            document = reader.read(Path(request['snapshot']))
            self.assertEqual(document['frames'][0]['duration_ms'], 90)
            self.assertEqual(document['frames'][1]['duration_ms'], 170)
            self.assertEqual(document['frames'][0]['channels']['color'][4 * 11:4 * 11 + 4], bytes((1, 2, 3, 255)))
            self.assertEqual(source.read_bytes(), original)

    def test_active_snapshot_request_cannot_pack_old_saved_pixels(self):
        import sys
        sys.path.insert(0, str(ROOT / 'scripts/props'))
        import pack_prop
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'old.aseprite'
            fixture(source)
            request = {'snapshot': str(source), 'proof': {'canvas': [5, 5], 'cels': [
                {'layer': 'color', 'frame': 0, 'xy': [1, 2], 'size': [2, 1],
                 'sha256': '0' * 64}]}}
            with self.assertRaisesRegex(ValueError, 'snapshot'):
                pack_prop.read_request_snapshot(request)


if __name__ == '__main__':
    unittest.main()
