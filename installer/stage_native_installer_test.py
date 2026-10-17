"""Packaging fixtures: reject unverified files before writing a payload."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import stage_native_installer as staging


class StagingTests(unittest.TestCase):
    def fixture(self, root, extra=None, changed=False):
        files = {'steamvr/setup_native.py': b'backend', 'steamvr/mod_identity.py': b'identity', 'bin/d2rl-pcvr.dll': b'plugin',
                 'bin/openxr_loader.dll': b'loader', **(extra or {})}
        manifest = {'platform': 'native-windows', 'source_commit': 'source',
                    'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
        package, runtime = root / 'mod.zip', root / 'python.zip'
        with zipfile.ZipFile(package, 'w') as archive:
            for name, data in files.items():
                archive.writestr('d2r-pcvr-preview/' + name, b'altered' if changed else data)
            archive.writestr('d2r-pcvr-preview/release-manifest.json', json.dumps(manifest))
        with zipfile.ZipFile(runtime, 'w') as archive:
            archive.writestr('python.exe', b'official runtime fixture')
            archive.writestr('LICENSE.txt', b'Python license fixture')
            archive.writestr('python312._pth', b'old import paths')
        return package, runtime, root / 'staged'

    def stage(self, root, extra=None, changed=False):
        package, runtime, destination = self.fixture(root, extra, changed)
        with patch.object(staging, 'PYTHON_SHA256', hashlib.sha256(runtime.read_bytes()).hexdigest()):
            return staging.stage(package, runtime, destination, root)

    def test_includes_private_runtime_license_and_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.stage(root)
            self.assertEqual(manifest['distribution'], 'graphical-windows-installer')
            self.assertIn('.runtime/python/LICENSE.txt', manifest['sha256'])
            self.assertNotIn('.runtime/python/python312._pth', manifest['sha256'])
            self.assertIn('../../steamvr', (root / 'staged/.runtime/python/python312._pth').read_text())
            for name, digest in manifest['sha256'].items():
                self.assertEqual(hashlib.sha256((root / 'staged' / name).read_bytes()).hexdigest(), digest)

    def test_rejects_game_assets_saves_logs_and_path_traversal(self):
        for name in ('D2R.exe', 'Character.d2s', 'game.log', 'steamvr/d2r_vr_icons/cache.json',
                     '../outside.txt', 'bin/C:escape.dll', 'bin\\escape.dll'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                with self.assertRaises(ValueError):
                    self.stage(root, {name: b'forbidden fixture'})
                self.assertFalse((root / 'staged').exists())

    def test_rejects_changed_manifest_payload_and_unmanifested_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                self.stage(root, changed=True)
            self.assertFalse((root / 'staged').exists())
            package, runtime, destination = self.fixture(root)
            with zipfile.ZipFile(package, 'a') as archive:
                archive.writestr('d2r-pcvr-preview/untracked.txt', 'untracked fixture')
            with patch.object(staging, 'PYTHON_SHA256', hashlib.sha256(runtime.read_bytes()).hexdigest()):
                with self.assertRaisesRegex(ValueError, 'not covered'):
                    staging.stage(package, runtime, destination, root)
            self.assertFalse(destination.exists())

    def test_rejects_wrong_python_checksum_and_existing_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package, runtime, destination = self.fixture(root)
            with self.assertRaisesRegex(ValueError, 'Python archive checksum'):
                staging.stage(package, runtime, destination, root)
            destination.mkdir()
            marker = destination / 'keep.txt'
            marker.write_bytes(b'preserve')
            with patch.object(staging, 'PYTHON_SHA256', hashlib.sha256(runtime.read_bytes()).hexdigest()):
                with self.assertRaisesRegex(ValueError, 'new staging directory'):
                    staging.stage(package, runtime, destination, root)
            self.assertEqual(marker.read_bytes(), b'preserve')


if __name__ == '__main__':
    unittest.main()
