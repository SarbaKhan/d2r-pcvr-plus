"""Full Plus setup routing on disposable Steam/game fixtures only."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import launch_windows
import launch_plus
import setup_native
import setup_native_test
import setup_windows
import steam_config


class PlusSetupTests(unittest.TestCase):
    def fixture(self, root, store='steam'):
        return setup_native_test.NativeSetupTests().fixture(root, store)

    def test_all_connections_for_every_headset_and_both_editions(self):
        for store in ('steam', 'battlenet'):
            for headset in setup_native.HEADSETS:
                with self.subTest(store=store, headset=headset), tempfile.TemporaryDirectory() as temp:
                    app, game, client, saves, profile, values = self.fixture(Path(temp), store)
                    runtime = client / 'steamapps/common/SteamVR/steamxr_win64.json'
                    with patch.object(setup_windows, 'verify_release', return_value={'sha256': {'bin/CascLib.dll': 'fixture'}}), patch.object(launch_windows, 'vdxr_manifest', return_value=runtime):
                        result = setup_native.validate_choices({**values, 'headset': headset, 'connections': 'steamvr,vdxr,flat'}, app)
                    self.assertEqual(result['connections'], ['steamvr', 'vdxr', 'flat'])

    def test_flat_selection_needs_no_installed_vr_runtime(self):
        for store in ('steam', 'battlenet'):
            with self.subTest(store=store), tempfile.TemporaryDirectory() as temp:
                app, game, client, saves, profile, values = self.fixture(Path(temp), store)
                (client / 'steamapps/common/SteamVR/steamxr_win64.json').unlink()
                with patch.object(setup_windows, 'verify_release', return_value={'sha256': {'bin/CascLib.dll': 'fixture'}}), patch.object(launch_windows, 'vdxr_manifest', side_effect=AssertionError('Flat must not inspect VDXR')):
                    result = setup_native.validate_choices({**values, 'connections': 'flat', 'steamvr': ''}, app)
                self.assertIsNone(result['runtime'])

    def test_three_shortcuts_are_repeatable_and_removable_without_unrelated_changes(self):
        for store in ('steam', 'battlenet'):
            with self.subTest(store=store), tempfile.TemporaryDirectory() as temp:
                app, game, client, saves, profile, values = self.fixture(Path(temp), store)
                routing = {} if store == 'steam' else dict(store=store, python=app / '.runtime/python/python.exe', wrapper=app / 'steamvr/launch_windows.py', runtime=client / 'steamapps/common/SteamVR/steamxr_win64.json')
                other = {'appname': 'Unrelated game', 'LaunchOptions': 'keep this', 'tags': {'0': 'favorite'}}
                original = steam_config.encode_binary({'shortcuts': {'0': other}})
                updated = steam_config.update_shortcuts(original, client, game, ['steamvr', 'vdxr', 'flat'], **routing)
                self.assertEqual(steam_config.update_shortcuts(updated, client, game, ['steamvr', 'vdxr', 'flat'], **routing), updated)
                entries = steam_config.decode_binary(updated)['shortcuts']
                self.assertEqual(len(entries), 4)
                self.assertEqual(entries['0'], other)
                flat = next(e for e in entries.values() if '--frame-flat' in e['LaunchOptions'])
                self.assertEqual(flat['OpenVR'], 0)
                self.assertNotIn('--frame-vr', flat['LaunchOptions'])
                self.assertNotIn('--frame-openxr', flat['LaunchOptions'])
                self.assertNotIn('.cmd', flat['Exe'])
                target = profile / 'config/shortcuts.vdf'
                target.write_bytes(updated)
                removed = steam_config.uninstall_updates(profile, game, store)
                self.assertEqual(steam_config.decode_binary(removed[target]), {'shortcuts': {'0': other}})

    def test_flat_steam_wrapper_ignores_default_vr_runtime_and_routes_to_flat(self):
        with tempfile.TemporaryDirectory(prefix='Plus accents é ') as temp:
            game = Path(temp)
            with patch.object(launch_plus, 'main', return_value=0) as flat, patch.object(launch_windows, 'launch_environment', side_effect=AssertionError('No XR for Flat')), patch.object(launch_windows, 'launch_from_desktop', side_effect=AssertionError('No VR handoff')):
                self.assertEqual(launch_windows.main(['--frame-openxr-runtime=missing.json', str(game / 'D2R.exe'), '--frame-flat']), 0)
            self.assertEqual(flat.call_args.args[0], ['--game', str(game.resolve()), '--mode', 'flat', '--store', 'steam'])

    def test_flat_battlenet_wrapper_preserves_explicit_opt_in(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(launch_plus, 'main', return_value=0) as flat:
            launch_windows.main(['--battlenet-game=' + temp, '--experimental-battlenet', '--frame-flat'])
            self.assertEqual(flat.call_args.args[0], ['--game', str(Path(temp).resolve()), '--mode', 'flat', '--store', 'battlenet', '--experimental-battlenet'])

    def test_mixed_modes_are_rejected_before_any_launch(self):
        for flag in ('--frame-vr', '--frame-vdxr'):
            with self.subTest(flag=flag), patch.object(launch_plus, 'main') as launch, self.assertRaises(SystemExit):
                launch_windows.main(['--frame-flat', flag, 'D2R.exe'])
            launch.assert_not_called()

    def test_stock_play_still_uses_original_executable(self):
        with patch.object(launch_windows.subprocess, 'run') as run:
            run.return_value.returncode = 0
            launch_windows.main(['--frame-openxr-runtime=absent.json', 'D2R.exe', '-original'])
        self.assertEqual(run.call_args.args[0], ['D2R.exe', '-original'])
        self.assertIsNone(run.call_args.kwargs['env'])


if __name__ == '__main__':
    unittest.main()
