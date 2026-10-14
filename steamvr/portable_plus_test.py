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


class RuntimeRoutingTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.settings=self.root/'portable-settings.json'

    def run_launch(self, settings, action='vr', extra=()):
        self.settings.write_text(json.dumps({'game':str(self.root), **settings}))
        with patch.object(portable,'ROOT',self.root), patch.object(portable,'launch',return_value=0) as launch:
            portable.main([action,*extra])
            return launch.call_args.args[0]

    def test_saved_steamvr_manifest_forwarded_and_flat_ignores_it(self):
        settings={'vr_runtime':'steamvr','openxr_runtime':'C:/Steam VR/runtime.json'}
        self.assertEqual(self.run_launch(settings)[-2:],['--openxr-runtime',settings['openxr_runtime']])
        self.assertNotIn('--openxr-runtime',self.run_launch(settings,'flat'))

    def test_legacy_vdxr_is_preserved(self):
        self.assertIn('--vdxr',self.run_launch({'vdxr':True}))

    def test_legacy_non_vdxr_requires_explicit_choice(self):
        with self.assertRaisesRegex(ValueError,'not explicitly configured'):
            self.run_launch({'vdxr':False})

    def test_cli_choice_overrides_saved_runtime(self):
        self.assertNotIn('--vdxr',self.run_launch({'vdxr':True},extra=['--openxr-runtime','C:/Steam/runtime.json']))
        self.assertNotIn('--openxr-runtime',self.run_launch({'openxr_runtime':'saved'},extra=['--vdxr']))
        args=self.run_launch({'vdxr':True},extra=['--system-openxr'])
        self.assertNotIn('--vdxr',args);self.assertNotIn('--openxr-runtime',args)

    def test_explicit_system_setting_has_no_override(self):
        args=self.run_launch({'vr_runtime':'system'})
        self.assertNotIn('--vdxr',args);self.assertNotIn('--openxr-runtime',args)

    def test_old_save_settings_are_ignored_without_accessing_paths(self):
        settings={'vr_runtime':'system','saves':'missing-save-directory','backups':'missing-backup-directory'}
        for mode in ('flat','vr'):
            args=self.run_launch(settings,mode)
            self.assertNotIn('--saves',args)
            self.assertNotIn('--backups',args)
            self.assertNotIn(settings['saves'],args)
            self.assertNotIn(settings['backups'],args)

    def test_configure_saves_explicit_steamvr_choice(self):
        (self.root/'D2RLoader.exe').write_bytes(b'fixture')
        manifest=self.root/'runtime.json';manifest.write_text('{}')
        answers=[str(self.root),'steam','steamvr',str(manifest)]
        with patch.object(portable,'ROOT',self.root), patch('builtins.input',side_effect=answers):
            portable.configure()
        settings=json.loads(self.settings.read_text())
        self.assertEqual(settings['vr_runtime'],'steamvr')
        self.assertEqual(settings['openxr_runtime'],str(manifest.resolve()))
        self.assertFalse(settings['vdxr'])
        self.assertNotIn('saves',settings)
        self.assertNotIn('backups',settings)


if __name__ == '__main__':
    unittest.main()
