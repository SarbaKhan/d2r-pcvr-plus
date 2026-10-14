"""Updater fixtures never operate on a real game installation."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import portable_plus as portable


class UpdateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.game = self.root / 'game'
        self.plugin = self.game / 'd2rloader/plugins'
        self.plugin.mkdir(parents=True)
        self.package = self.root / 'package'
        (self.package / 'payload').mkdir(parents=True)
        for path, data in [(self.game/'D2R.exe', b'fixture'), (self.game/'D2RLoader.exe', b'loader'),
                           (self.plugin/portable.DLL, b'original'), (self.plugin/'d2r_vr.ini', b'custom VR'),
                           (self.game/'d2rloader/pcvr-install.json', b'original installer state'),
                           (self.game/'hero.d2s', b'character'),
                           (self.package/'payload'/portable.DLL, b'new'),
                           (self.package/'payload'/portable.FLAT, b'flat')]:
            path.write_bytes(data)
        self.hashes = {name:portable.digest(self.package/'payload'/name) for name in (portable.DLL, portable.FLAT)}
        (self.package/'payload-sha256.json').write_text(json.dumps(self.hashes))
        self.guard = patch('portable_plus.require_game_closed')
        self.guard.start()
        self.addCleanup(self.guard.stop)

    def test_update_and_rollback_preserve_installation_and_settings(self):
        (self.plugin/portable.FLAT).write_bytes(b'custom flat')
        before = {p:p.read_bytes() for p in self.game.rglob('*') if p.is_file() and p.name != portable.DLL}
        portable.update(self.game, self.package)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'new')
        portable.rollback(self.game)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'original')
        for p, content in before.items():
            self.assertEqual(p.read_bytes(), content)

    def test_existing_update_refused_and_backup_retained(self):
        portable.update(self.game, self.package)
        with self.assertRaises(ValueError):
            portable.update(self.game, self.package)
        portable.rollback(self.game)
        self.assertEqual(len(list((self.game/'d2rloader/pcvr-plus-backups').glob('*/'+portable.DLL))), 1)

    def test_damaged_payload_never_changes_game(self):
        (self.package/'payload'/portable.DLL).write_bytes(b'damaged')
        with self.assertRaises(ValueError):
            portable.update(self.game, self.package)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'original')

    def test_failed_copy_restores_original(self):
        real = portable.atomic_copy
        def copy(source, target):
            if source == self.package/'payload'/portable.DLL:
                raise OSError('disk full')
            return real(source, target)
        with patch('portable_plus.atomic_copy', side_effect=copy), self.assertRaises(OSError):
            portable.update(self.game, self.package)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'original')
        self.assertFalse((self.game/'d2rloader/pcvr-plus-update.json').exists())

    def test_modified_dll_not_overwritten_on_rollback(self):
        portable.update(self.game, self.package)
        (self.plugin/portable.DLL).write_bytes(b'other update')
        with self.assertRaises(ValueError):
            portable.rollback(self.game)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'other update')

    def test_modified_backup_refused(self):
        portable.update(self.game, self.package)
        next((self.game/'d2rloader/pcvr-plus-backups').glob('*/'+portable.DLL)).write_bytes(b'bad')
        with self.assertRaises(ValueError):
            portable.rollback(self.game)

    def test_running_game_aborts_before_changes(self):
        with patch('portable_plus.require_game_closed', side_effect=SystemExit('running')), self.assertRaises(SystemExit):
            portable.update(self.game, self.package)
        self.assertEqual((self.plugin/portable.DLL).read_bytes(), b'original')
        self.assertFalse((self.game/'d2rloader/pcvr-plus-update.json').exists())

    def test_interrupted_before_dll_replace_can_rollback(self):
        portable.update(self.game, self.package)
        (self.plugin/portable.DLL).write_bytes(b'original')
        portable.rollback(self.game)

    def test_redirect_outside_game_refused(self):
        with self.assertRaises(ValueError):
            portable.checked(self.game, '../outside')

    def test_manifest_traversal_refused(self):
        portable.update(self.game, self.package)
        state = self.game/'d2rloader/pcvr-plus-update.json'
        record = json.loads(state.read_text()); record['backup'] = '../../outside'
        state.write_text(json.dumps(record))
        with self.assertRaises(ValueError):
            portable.rollback(self.game)


if __name__ == '__main__':
    unittest.main()
