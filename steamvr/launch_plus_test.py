import configparser
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import launch_plus as plus


class PlusTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.saves = self.root / 'Saved Games'
        self.saves.mkdir()
        (self.saves / 'Settings.json').write_text('{}')
        (self.saves / 'hero.d2s').write_bytes(b'fixture character')
        (self.saves / 'SharedStashSoftCoreV2.d2i').write_bytes(b'fixture stash')
        self.backups = self.root / 'backups'
        self.game = self.root / 'game'
        self.plugins = self.game / 'd2rloader/plugins'
        self.plugins.mkdir(parents=True)
        for name in ('D2R.exe', 'D2RLoader.exe'):
            (self.game / name).touch()
        (self.plugins / 'd2rl-vrcam.dll').touch()
        (self.plugins / 'd2r_vr.ini').write_text('background_full_speed=1\n[mode]\nplatform=1\n')
        (self.plugins / 'd2r_pcvr_plus_flat.ini').write_text('[mode]\nplatform=0\n')

    def test_snapshot_all_files_verified_and_never_overwrites(self):
        (self.saves / 'nested').mkdir()
        (self.saves / 'nested/hero.d2s').write_bytes(b'second fixture')
        a = plus.snapshot(self.saves, self.backups, 'flat')
        b = plus.snapshot(self.saves, self.backups, 'vr')
        self.assertNotEqual(a,b)
        manifest = json.loads((a / 'manifest.json').read_text())
        self.assertEqual(len(manifest['sha256']), 4)
        for name, sha in manifest['sha256'].items():
            self.assertEqual(sha, plus.digest(self.saves / name))
            self.assertEqual(sha, plus.digest(a / 'files' / name))

    def test_rejects_nested_backup_paths(self):
        for folder in (self.saves, self.saves / 'backup', self.root):
            with self.assertRaises(ValueError): plus.snapshot(self.saves, folder, 'flat')

    def test_missing_actual_save_folder(self):
        (self.saves / 'Settings.json').unlink()
        with self.assertRaises(ValueError): plus.snapshot(self.saves, self.backups, 'flat')

    def test_rejects_symlink(self):
        try:
            (self.saves / 'redirect').symlink_to(self.root, target_is_directory=True)
        except OSError as error:
            self.skipTest(f'Symlink creation unavailable: {error}')
        with self.assertRaises(ValueError): plus.snapshot(self.saves, self.backups, 'flat')

    def test_disk_failure_keeps_originals(self):
        with patch.object(plus.shutil, 'copy2', side_effect=OSError('disk full')):
            with self.assertRaises(OSError): plus.snapshot(self.saves, self.backups, 'flat')
        self.assertEqual((self.saves / 'hero.d2s').read_bytes(), b'fixture character')
        self.assertEqual(list(self.backups.iterdir()), [])

    def test_modified_source_aborts(self):
        real_copy = plus.shutil.copy2
        def changing(source,target):
            result = real_copy(source,target)
            source.write_bytes(b'changed concurrently')
            return result
        with patch.object(plus.shutil, 'copy2', side_effect=changing):
            with self.assertRaises(ValueError): plus.snapshot(self.saves,self.backups,'flat')
        self.assertEqual(list(self.backups.iterdir()), [])

    def test_lock_shared_by_modes_and_released_after_exception(self):
        with self.assertRaisesRegex(RuntimeError, 'fixture'):
            with plus.launch_lock(self.saves):
                with self.assertRaises(ValueError):
                    with plus.launch_lock(self.saves): pass
                raise RuntimeError('fixture')
        with plus.launch_lock(self.saves): pass

    def test_profiles_untouched_and_environment_local(self):
        before = {p:p.read_bytes() for p in self.plugins.glob('*.ini')}
        with patch.dict(plus.os.environ, {'D2R_PCVR_PLUS_MODE':'vr'}, clear=True):
            command, env = plus.prepare(self.game, 'flat')
            self.assertEqual(plus.os.environ['D2R_PCVR_PLUS_MODE'], 'vr')
            self.assertEqual(env['D2R_PCVR_PLUS_MODE'],'flat')
            self.assertNotIn('XR_RUNTIME_JSON',env)
        self.assertEqual(command, [str(self.game / 'D2RLoader.exe')])
        self.assertEqual(before,{p:p.read_bytes() for p in self.plugins.glob('*.ini')})

    def test_missing_flat_profile_does_not_fall_back_to_vr(self):
        (self.plugins / 'd2r_pcvr_plus_flat.ini').unlink()
        with self.assertRaises(ValueError): plus.prepare(self.game,'flat')

    def test_vr_profile_must_be_vr(self):
        (self.plugins / 'd2r_vr.ini').write_text('[mode]\nplatform=0')
        with self.assertRaises(ValueError): plus.prepare(self.game,'vr')

    def test_flat_launch_never_calls_vr_launcher(self):
        args=['--game',str(self.game),'--saves',str(self.saves),'--backups',str(self.backups),'--mode','flat']
        with patch.object(plus.sys,'platform','win32'), patch('game_install.validate_game'), \
             patch('process_guard.require_game_closed'), patch('launch_windows.main') as vr, \
             patch.object(plus.subprocess,'run') as run:
            run.return_value.returncode=0
            self.assertEqual(plus.main(args),0)
            vr.assert_not_called()
            self.assertEqual(run.call_args.kwargs['env']['D2R_PCVR_PLUS_MODE'],'flat')
            self.assertEqual(len(list(self.backups.iterdir())),1)

    def test_snapshot_failure_prevents_any_launch(self):
        args=['--game',str(self.game),'--saves',str(self.saves),'--backups',str(self.backups),'--mode','flat']
        with patch.object(plus.sys,'platform','win32'), patch('game_install.validate_game'), \
             patch('process_guard.require_game_closed'), patch.object(plus,'snapshot',side_effect=OSError('disk full')), \
             patch.object(plus.subprocess,'run') as run:
            with self.assertRaises(OSError): plus.main(args)
            run.assert_not_called()

    def test_vr_launch_uses_original_route_after_backup(self):
        args=['--game',str(self.game),'--saves',str(self.saves),'--backups',str(self.backups),'--mode','vr','--vdxr']
        with patch.object(plus.sys,'platform','win32'), patch('game_install.validate_game'), \
             patch('process_guard.require_game_closed'), patch('launch_windows.main',return_value=7) as vr:
            self.assertEqual(plus.main(args),7)
            vr.assert_called_once_with(['--frame-vr','--frame-vdxr',str(self.game.resolve()/'D2R.exe')])
            self.assertEqual(len(list(self.backups.iterdir())),1)

    def test_shipped_flat_profile(self):
        config=configparser.ConfigParser(interpolation=None,strict=False,allow_no_value=True)
        config.read_string('[global]\n'+(Path(__file__).parents[1]/'d2r_pcvr_plus_flat.ini').read_text())
        for section,key,value in [('mode','platform','0'),('stereo','afr','0'),('input','flat_layout','WASD'),('gameplay','motion_controls','0')]:
            self.assertEqual(config[section][key],value)

if __name__ == '__main__': unittest.main()
