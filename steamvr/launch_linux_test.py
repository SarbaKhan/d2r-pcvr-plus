import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import launch_linux as launch


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux/Proton integration fixtures run in the Linux CI job")
class LinuxLaunchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='plus paths é ')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.game = self.root / 'other disk/steamapps/common/Diablo II Resurrected'
        self.plugin = self.game / 'mods/D2R-PCVR/d2rloader/plugins'
        self.plugin.mkdir(parents=True)
        for path in ('D2R.exe', 'D2RLoader.exe', 'mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll'):
            (self.game / path).write_bytes(b'fixture')
        for profile, value in [('d2r_pcvr.ini', 1), ('d2r_pcvr_plus_flat.ini', 0)]:
            (self.plugin / profile).write_text(f'[mode]\nplatform={value}\n')
        self.config = {'game': str(self.game)}
        self.env = {'STEAM_COMPAT_DATA_PATH': str(self.root / 'custom prefix'),
                    'SteamAppId': '2536520', 'STEAM_COMPAT_LIBRARY_PATHS': '/other/library'}
        self.command = ['/runtime/entry point', '/proton', 'waitforexitandrun', str(self.game / 'D2R.exe'), '-arg', 'spaces here']

    def runtime(self, mode):
        library = self.root / ('libsteamxr.so' if mode == 'steamvr' else 'libwivrn.so')
        library.touch()
        manifest = self.root / (mode + '.json')
        manifest.write_text(json.dumps({'runtime': {'library_path': library.name}}))
        self.config[mode + '_runtime'] = str(manifest)
        return manifest

    def test_stock_exact_passthrough_without_config(self):
        self.assertEqual(launch.route(self.command, {}, self.env), (self.command, self.env, None))

    def test_flat_preserves_proton_context_and_arguments_without_xr(self):
        env = dict(self.env, XR_RUNTIME_JSON='/missing.json', PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES='1')
        command, result, info = launch.route(self.command + ['--plus-flat'], self.config, env)
        self.assertEqual(command[:3], self.command[:3])
        self.assertEqual(command[3], str(self.game / 'D2RLoader.exe'))
        self.assertEqual(command[4:], self.command[4:] + ['-mod', 'D2R-PCVR'])
        self.assertEqual(result['STEAM_COMPAT_DATA_PATH'], env['STEAM_COMPAT_DATA_PATH'])
        self.assertEqual(result['SteamAppId'], '2536520')
        self.assertEqual(result['D2R_PCVR_PLUS_MODE'], 'flat')
        self.assertNotIn('XR_RUNTIME_JSON', result)
        self.assertNotIn('PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES', result)
        self.assertIn('XR_RUNTIME_JSON', env)
        self.assertTrue(result['STEAM_COMPAT_LIBRARY_PATHS'].endswith(':/other/library'))

    def test_both_vr_routes_select_requested_runtime(self):
        for mode in ('steamvr', 'wivrn'):
            with self.subTest(mode=mode):
                manifest = self.runtime(mode)
                command, env, info = launch.route(self.command + ['--plus-' + mode], self.config, self.env)
                if mode == 'steamvr':
                    self.assertEqual(env['XR_RUNTIME_JSON'], str(manifest.resolve()))
                else:
                    self.assertNotIn('XR_RUNTIME_JSON', env)
                self.assertEqual(env['D2R_PCVR_PLUS_MODE'], 'vr')
                self.assertEqual(env['PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES'], '1')

    def test_wayland_is_mode_specific(self):
        inherited = dict(self.env, XDG_SESSION_TYPE='wayland', PROTON_ENABLE_WAYLAND='1', XR_RUNTIME_JSON='/wrong')
        _, env, _ = launch.route(self.command + ['--plus-flat'], self.config, inherited)
        self.assertEqual(env['PROTON_ENABLE_WAYLAND'], '1')
        for mode in ('steamvr', 'wivrn'):
            self.runtime(mode)
            _, env, _ = launch.route(self.command + ['--plus-' + mode], self.config, inherited)
            self.assertNotIn('PROTON_ENABLE_WAYLAND', env)
        _, env, _ = launch.route(self.command + ['--plus-flat'], self.config, dict(self.env, XDG_SESSION_TYPE='x11'))
        self.assertNotIn('PROTON_ENABLE_WAYLAND', env)

    def test_probe_replaces_game_and_uses_windows_log_path(self):
        self.runtime('wivrn')
        (self.root / 'bin').mkdir()
        (self.root / 'bin/openxr-probe.exe').touch()
        with patch.object(launch, 'ROOT', self.root):
            command, env, info = launch.route(self.command + ['--plus-wivrn', '--plus-probe'], self.config, self.env)
        self.assertEqual(Path(command[3]).name, 'openxr-probe.exe')
        self.assertTrue(command[4].startswith('Z:\\'))
        self.assertNotIn(str(self.game / 'D2RLoader.exe'), command)

    def test_invalid_requests_fail_before_execution(self):
        for flags in (['--plus-flat', '--plus-wivrn'], ['--plus-flat', '--plus-probe'], ['--plus-probe'], ['--plus-wivrn']):
            with self.subTest(flags=flags), self.assertRaises(ValueError):
                launch.route(self.command + flags, self.config, self.env)
        with self.assertRaises(ValueError):
            launch.route(self.command + ['--plus-flat'], self.config, {})
        with self.assertRaises(ValueError):
            launch.route(self.command + ['--plus-flat'], {'game': '/wrong'}, self.env)

    def test_missing_profile_fails(self):
        (self.plugin / 'd2r_pcvr_plus_flat.ini').unlink()
        with self.assertRaises(ValueError):
            launch.route(self.command + ['--plus-flat'], self.config, self.env)

    def test_wrong_runtime_is_not_silently_used(self):
        path = self.runtime('steamvr')
        with self.assertRaises(ValueError):
            launch.runtime_manifest(path, 'wivrn')
        (self.root / 'libsteamxr.so').unlink()
        with self.assertRaises(ValueError):
            launch.runtime_manifest(path, 'steamvr')

    def test_real_child_exit_environment_and_no_shell_interpretation(self):
        # Execute a harmless process through the actual wrapper main, no game/runtime.
        self.command[:3] = [sys.executable, '-c', 'import os,sys; assert os.environ["D2R_PCVR_PLUS_MODE"] == "flat"; sys.exit(17)']
        (self.root / 'linux-launch.json').write_text(json.dumps(self.config))
        with patch.object(launch, 'ROOT', self.root), patch.dict(os.environ, self.env):
            self.assertEqual(launch.main(self.command + ['--plus-flat']), 17)
        rows = (self.game / 'd2rloader/logs/plus-linux-launch.jsonl').read_text().splitlines()
        self.assertEqual(json.loads(rows[-1])['exit_code'], 17)


if __name__ == '__main__':
    unittest.main()
