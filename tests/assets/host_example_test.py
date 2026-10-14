"""Real shipped documents -> real script -> strict package, no mocked host."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from ase_reader import read
from pack_prop import export, validate, read_request_snapshot


class HostExampleTests(unittest.TestCase):
    def test_shipped_examples_export_with_exact_channels_tags_and_timing(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            for mode in ('rgba', 'indexed'):
                source = directory / (mode + '.aseprite')
                source.write_bytes((ROOT / ('assets/props/example-' + mode + '.aseprite')).read_bytes())
                original = source.read_bytes()
                result = subprocess.run([os.environ['LIBRESPRITE'], '-b', str(source), '--script',
                                         str(ROOT / 'scripts/libresprite/export_prop.js')], capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                request_path = next(line.split('=', 1)[1] for line in result.stdout.splitlines()
                                    if line.startswith('CRAFT_PROP_REQUEST='))
                request = json.loads(Path(request_path).read_text())
                expected, actual = read(source), read_request_snapshot(request)
                self.assertEqual(actual['frames'], expected['frames'])
                self.assertEqual(actual['tags'], expected['tags'])
                package = directory / (mode + '.prop')
                export(actual, package)
                manifest = validate(package)
                self.assertEqual(manifest['tags'][0]['name'], 'idle')
                self.assertEqual([f['duration_ms'] for f in manifest['frames']], [90, 170])
                self.assertEqual(source.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
