"""Prevent accidental replacement of the installer-shipped VR implementation.

This checks source identity, not headset behavior. Update the manifest only after
an intentional baseline migration and hardware comparison with that release.
"""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent


class ShippedVrBaselineTests(unittest.TestCase):
    def test_native_sources_match_installer_runtime(self):
        manifest = json.loads((ROOT / 'steamvr/vr_baseline.json').read_text())
        for name, expected in manifest['unchanged_sources'].items():
            with self.subTest(source=name):
                data = (ROOT / name).read_bytes().replace(b'\r\n', b'\n')
                self.assertEqual(hashlib.sha256(data).hexdigest(), expected)


if __name__ == '__main__':
    unittest.main()
