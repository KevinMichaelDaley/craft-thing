"""Build the real Python compiler output consumed by the C regression binary."""
from pathlib import Path
import sys

from package_test import document

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from pack_prop import export

export(document(), ROOT / 'build/prop_fixture')
