import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import direct_linux as linux
import direct_windows as windows
import launch_linux


class DirectLaunchTests(unittest.TestCase):
    def test_windows_modes_use_existing_loader_routes(self):
        config = {'game': 'Game with spaces', 'store': 'steam', 'modes': ['flat', 'steamvr', 'vdxr'],
                  'steamvr_runtime': 'SteamVR/steamxr_win64.json'}
        self.assertNotIn('--openxr-runtime', windows.arguments(config, 'flat'))
        self.assertIn('--openxr-runtime', windows.arguments(config, 'steamvr'))
        self.assertIn('--vdxr', windows.arguments(config, 'vdxr'))
        config['modes'] = ['flat']
        with self.assertRaises(ValueError): windows.arguments(config, 'vdxr')
        config['store'] = 'battlenet'
        self.assertIn('--experimental-battlenet', windows.arguments(config, 'flat'))

    def test_linux_replays_container_without_steam_or_reaper_and_preserves_prefix(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve(); game = root / 'steamapps/common/Game with spaces'
            plugin = game / 'd2rloader/plugins'; plugin.mkdir(parents=True)
            for name in ('D2R.exe', 'D2RLoader.exe'): (game / name).touch()
            (plugin / 'd2rl-vrcam.dll').touch()
            (plugin / 'd2r_pcvr_plus_flat.ini').write_text('[mode]\nplatform=0\n')
            (plugin / 'd2r_vr.ini').write_text('[mode]\nplatform=1\n')
            prefix = root / 'steamapps/compatdata/2536520'; (prefix / 'pfx').mkdir(parents=True)
            proton = root / 'Proton/proton'; proton.parent.mkdir(); proton.touch()
            container = root / 'SteamLinuxRuntime_sniper/_v2-entry-point'; container.parent.mkdir(); container.touch()
            command = ['/steam/reaper', 'SteamLaunch', 'AppId=2536520', '--', str(container),
                       '--verb=waitforexitandrun', '--', str(proton), 'waitforexitandrun', str(game / 'D2R.exe'), '--plus-flat']
            env = {'STEAM_COMPAT_DATA_PATH': str(prefix), 'STEAM_COMPAT_CLIENT_INSTALL_PATH': str(root),
                   'SECRET_TOKEN': 'never save', 'DISPLAY': ':999'}
            linux.remember(root, command, env, game)
            saved = json.loads((root / linux.CONTEXT).read_text())
            self.assertEqual(saved['command'][0], str(container))
            self.assertNotIn('SECRET_TOKEN', saved['environment']); self.assertNotIn('DISPLAY', saved['environment'])
            (root / 'linux-launch.json').write_text(json.dumps({'game': str(game)}))
            with patch('process_guard.require_game_closed'), patch.dict(os.environ, {'XDG_SESSION_TYPE': 'wayland'}), patch.object(subprocess, 'run') as run:
                run.return_value.returncode = 0
                self.assertEqual(linux.launch(root, 'flat'), 0)
                args, options = run.call_args
                self.assertEqual(args[0][0], str(container)); self.assertNotIn('/steam/reaper', args[0])
                self.assertIn(str(game / 'D2RLoader.exe'), args[0])
                self.assertEqual(options['env']['STEAM_COMPAT_DATA_PATH'], str(prefix))
                self.assertEqual(options['env']['PROTON_ENABLE_WAYLAND'], '1')
                self.assertEqual(options['env']['SteamAppId'], '2536520')
            proton.unlink()
            with self.assertRaisesRegex(ValueError, 'Proton was removed'): linux.launch(root, 'flat')

    def test_missing_context_requires_one_time_learning_not_new_prefix(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaisesRegex(ValueError, 'One-time setup'): linux.launch(temp, 'flat')
            self.assertEqual(list(Path(temp).iterdir()), [])

    @unittest.skipUnless(sys.platform == 'win32', 'Native executable fixture requires Windows')
    def test_native_exe_invokes_private_python_and_propagates_exit(self):
        repo = Path(__file__).resolve().parent.parent
        exe = repo / 'build-frame/windows-native/plus-desktop-launcher.exe'
        if not exe.exists(): self.skipTest('Build the executable first')
        with tempfile.TemporaryDirectory(prefix='Plus launcher with spaces ') as temp:
            root = Path(temp); runtime = root / '.runtime/python'
            shutil.copytree(repo / 'deps/python', runtime)
            (root / 'steamvr').mkdir()
            (root / 'steamvr/direct_windows.py').write_text('import sys\nraise SystemExit({"flat":17,"steamvr":18,"vdxr":19}[sys.argv[1]])\n')
            for mode, code in [('Flat', 17), ('SteamVR', 18), ('VDXR', 19)]:
                target = root / ('D2R-PCVR-Plus-' + mode + '.exe'); shutil.copy2(exe, target)
                self.assertEqual(subprocess.run([str(target)], timeout=30).returncode, code)

if __name__ == '__main__': unittest.main()
