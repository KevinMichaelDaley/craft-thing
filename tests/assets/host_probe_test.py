"""Run the compatibility probe in the actual pinned LibreSprite host."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from libresprite_fixture import fixture

ROOT = Path(__file__).resolve().parents[2]


class HostProbe(unittest.TestCase):
    def test_real_host_snapshot_channels_and_timing(self):
        host = os.environ['LIBRESPRITE']
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / 'input.aseprite'
            fixture(source)
            before = hashlib.sha256(source.read_bytes()).digest()
            script = directory / 'probe.js'
            code = (ROOT / 'scripts/libresprite/probe.js').read_text()
            script.write_text(code.replace('__PROBE_OUTPUT__', str(directory / 'probe.json')))
            result = subprocess.run([host, '-b', str(source), '--script', str(script)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            output = json.loads((directory / 'probe.json').read_text())
            self.assertEqual(output['canvas'], [5, 5])
            self.assertEqual([layer['name'] for layer in output['layers']], ['physics.material', 'physics.role', 'color'])
            self.assertEqual(output['layers'][0]['cels'][1]['frame'], 1)
            self.assertEqual(output['layers'][0]['cels'][0]['xy'], [1, 2])
            self.assertFalse(output['layers'][0]['visible'])
            self.assertEqual(hashlib.sha256(source.read_bytes()).digest(), before)


if __name__ == '__main__':
    unittest.main()
