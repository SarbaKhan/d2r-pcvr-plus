import json
import os
from pathlib import Path
import tempfile
import shutil
import unittest
from unittest.mock import patch

import setup_linux as setup
import steam_config as vdf


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='Plus setup é spaces ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.environment = patch.dict(os.environ, {'XDG_DATA_HOME': str(self.root / 'desktop-data')})
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.steam = self.root / 'Steam data'
        self.profile = self.steam / 'userdata/123'
        (self.profile / 'config').mkdir(parents=True)
        self.local = self.profile / 'config/localconfig.vdf'
        self.local.write_text('"UserLocalConfigStore" { "Other" "keep" }')
        self.game = self.root / 'another disk/steamapps/common/Diablo II Resurrected'
        self.game.mkdir(parents=True)
        (self.game / 'D2R.exe').touch()
        self.saves = setup.default_saves(self.game)
        self.saves.mkdir(parents=True)
        (self.saves / 'Settings.json').write_text('{}')
        (self.saves / 'temporary.d2s').write_bytes(b'character must never change')
        self.steam_exe = self.root / 'steam'
        self.steam_exe.write_text('#!/bin/sh\nexit 0\n')
        self.steam_exe.chmod(0o755)
        self.c = dict(game=str(self.game), steam_root=str(self.steam), steam=str(self.steam_exe),
                      profile=str(self.profile), saves=str(self.saves), destination=str(self.game / 'D2R-PCVR-Plus'),
                      modes=['flat'], layout='ZQSD', text_locale='frFR', audio_locale='', reviewed_options='')

    def profiles(self):
        plugin = self.game / 'd2rloader/plugins'
        plugin.mkdir(parents=True, exist_ok=True)
        (plugin / 'd2r_pcvr_plus_flat.ini').write_text('[input]\nflat_layout=WASD\ncustom=keep\n')
        (plugin / 'd2r_vr.ini').write_text('[hud]\ndistance=1.25\n')
        config = self.game / 'd2rloader/config'
        config.mkdir(parents=True, exist_ok=True)
        (config / 'd2rloader.toml').write_text('text_locale = "enUS"\naudio_locale = "deDE"\ncustom = true\n')

    def test_discovery_external_library_and_custom_account(self):
        (self.steam / 'steamapps').mkdir()
        library = self.game.parent.parent.parent
        (self.steam / 'steamapps/libraryfolders.vdf').write_text('"libraryfolders" { "0" { "path" ' + vdf.quote(str(library)) + ' } }')
        detected = setup.discover(self.steam)
        self.assertEqual(detected['games'], [str(self.game)])
        self.assertEqual(setup.steam_profiles(self.steam), [self.profile])

    def test_validation_rejects_wrong_profile_unknown_destination_and_missing_runtime(self):
        self.assertEqual(setup.validate(self.c)['layout'], 'ZQSD')
        for change in [dict(profile='/wrong'), dict(layout='wrong'), dict(text_locale='wrong'),
                       dict(modes=['wivrn']), dict(destination=str(self.game))]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                setup.validate(dict(self.c, **change))
        dest = Path(self.c['destination'])
        dest.mkdir()
        (dest / 'unrelated').touch()
        with self.assertRaises(ValueError):
            setup.validate(self.c)

    def test_destination_is_automatic_and_rejects_symlinks(self):
        choices = dict(self.c)
        choices.pop('destination')
        destination = self.game / 'D2R-PCVR-Plus'
        self.assertEqual(setup.validate(choices)['destination'], str(destination))
        target = self.root / 'unrelated'
        target.mkdir()
        try:
            destination.symlink_to(target, target_is_directory=True)
        except OSError:
            self.skipTest('Symlinks are unavailable on this runner')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            setup.validate(choices)
        self.assertEqual(list(target.iterdir()), [])

    def test_preferences_preserve_other_settings_and_empty_default(self):
        self.profiles()
        before = (self.game / 'd2rloader/plugins/d2r_vr.ini').read_bytes()
        updates = setup.preference_updates(self.game, self.c)
        for path, data in updates.items():
            path.write_bytes(data)
        prefs = setup.preferences(self.game)
        self.assertEqual(prefs, dict(layout='ZQSD', text_locale='frFR', audio_locale=''))
        self.assertIn(b'custom = true', updates[self.game / 'd2rloader/config/d2rloader.toml'])
        self.assertEqual(before, (self.game / 'd2rloader/plugins/d2r_vr.ini').read_bytes())
        self.assertIn('flat_layout=ZQSD', setup.replace_value('[input]\nother=1\n', 'flat_layout', 'ZQSD', 'input'))
        with self.assertRaises(ValueError):
            setup.replace_value('audio_locale=""\naudio_locale=""\n', 'audio_locale', '"frFR"')

    def test_shortcuts_repeat_run_preserves_unrelated_and_does_not_start_steamvr_for_wivrn(self):
        unrelated = {'appname': 'Keep my game', 'Exe': 'custom', 'tags': {'0': 'mine'}, 'appid': 123}
        original = vdf.encode_binary({'shortcuts': {'0': unrelated}})
        first = setup.shortcuts(original, self.steam_exe, self.game, list(setup.NAMES))
        second = setup.shortcuts(first, self.steam_exe, self.game, list(setup.NAMES))
        self.assertEqual(first, second)
        rows = vdf.decode_binary(second)['shortcuts']
        self.assertEqual(rows['0'], unrelated)
        self.assertEqual(len(rows), 4)
        for row in list(rows.values())[1:]:
            mode = row['LaunchOptions'].split('--plus-')[1]
            self.assertEqual(row['OpenVR'], int(mode == 'steamvr'))
            self.assertEqual(row['Exe'], '"' + str(self.steam_exe) + '"')

    def test_configuration_write_rolls_back_partial_failure(self):
        one, two = self.root / 'one', self.root / 'two'
        one.write_bytes(b'old')
        real = setup.atomic_write
        def fail(path, data):
            if path == two:
                raise OSError('disk full')
            real(path, data)
        with patch.object(setup, 'require_closed'), patch.object(setup, 'atomic_write', side_effect=fail):
            with self.assertRaises(OSError):
                setup.commit_files({one: b'new', two: b'new'}, self.root / 'backups')
        self.assertEqual(one.read_bytes(), b'old')
        self.assertFalse(two.exists())

    def test_full_orchestration_preserves_loader_preferences_and_character(self):
        self.profiles()
        bundle = self.root / 'download'
        payload = bundle / '_internal/payload'
        payload.mkdir(parents=True)
        shutil.copytree(Path(__file__).resolve().parent.parent / 'artwork', payload / 'artwork')
        executable = bundle / 'D2R-PCVR-Plus-Setup'
        executable.write_text('fixture')
        executable.chmod(0o755)
        old = self.root / 'old external installation'
        old.mkdir()
        (old / 'historical-snapshot').write_bytes(b'keep')
        previous = setup.shlex.quote(str(old / executable.name)) + ' --route %command%'
        self.local.write_text(vdf.set_launch_options(self.local.read_text(), previous))
        self.c['reviewed_options'] = previous
        def installer(command, check):
            self.assertEqual(command[1], '--install-game')
            self.assertIn('--preserve-settings', command)
            (self.game / 'd2rloader/config/d2rloader.toml').write_text('text_locale=""\naudio_locale=""\ncustom=false\n')
        with patch.object(setup, 'require_closed'), patch.object(setup.subprocess, 'run', side_effect=installer):
            setup.install_choices(self.c, bundle, payload, executable)
        self.assertEqual((self.saves / 'temporary.d2s').read_bytes(), b'character must never change')
        self.assertIn('custom = true', (self.game / 'd2rloader/config/d2rloader.toml').read_text())
        self.assertIn(' --route %command%', setup.current_options(self.profile))
        self.assertIn(str(self.game / 'D2R-PCVR-Plus'), setup.current_options(self.profile))
        self.assertEqual(setup.preferences(self.game)['text_locale'], 'frFR')
        self.assertTrue((Path(self.c['destination']) / '_internal/payload/linux-launch.json').is_file())
        self.assertNotIn(str(old), setup.current_options(self.profile))
        self.assertEqual((old / 'historical-snapshot').read_bytes(), b'keep')

    def test_options_changed_since_review_aborts_before_installation(self):
        self.local.write_text(vdf.set_launch_options(self.local.read_text(), 'different %command%'))
        with patch.object(setup, 'require_closed'), self.assertRaises(ValueError):
            setup.install_choices(self.c, self.root / 'download', self.root / 'payload', self.root / 'exe')
        self.assertFalse(Path(self.c['destination']).exists())

    def test_failed_update_restores_old_launcher_path(self):
        self.profiles()
        dest = Path(self.c['destination'])
        dest.mkdir()
        (dest / 'plus-linux-managed.json').write_text('{}')
        (dest / 'old-launcher').write_text('keep')
        bundle = self.root / 'download'
        (bundle / 'payload').mkdir(parents=True)
        (bundle / 'setup').write_text('fixture')
        with patch.object(setup, 'require_closed'), patch.object(setup.subprocess, 'run', side_effect=OSError('failed')):
            with self.assertRaises(OSError):
                setup.install_choices(self.c, bundle, bundle / 'payload', bundle / 'setup')
        self.assertEqual((dest / 'old-launcher').read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
