"""Steam desktop handoff lifecycle; game calls use temporary fixtures only."""
import base64
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

import launch_windows as launch


class DesktopLaunchTests(unittest.TestCase):
    def test_shortcut_identity_uses_owned_game_without_mutating_parent(self):
        with patch.dict(os.environ, {'SteamAppId': '2752970393',
                'SteamGameId': '11823917804824821760', 'SteamOverlayGameId': '2752970393'}, clear=True):
            before = dict(os.environ)
            environment = {k.lower(): v for k, v in launch.launch_environment(['--frame-vr']).items()}
            self.assertEqual(environment['steamappid'], '2536520')
            self.assertEqual(environment['steamgameid'], '2536520')
            self.assertNotIn('steamoverlaygameid', environment)
            self.assertEqual(dict(os.environ), before)

    def test_flat_steam_hands_off_once_without_direct_fallback_or_runtime_lookup(self):
        import launch_plus
        with tempfile.TemporaryDirectory() as temporary:
            game = Path(temporary).resolve()
            arguments = [str(game / 'D2R.exe'), '--frame-flat', '--frame-openxr-runtime=missing.json']
            with patch.object(launch.sys, 'platform', 'win32'), \
                    patch.object(launch, 'launch_from_desktop', return_value=1) as broker, \
                    patch.object(launch, 'launch_environment', side_effect=AssertionError('Flat must not select XR')), \
                    patch.object(launch_plus, 'main') as direct:
                self.assertEqual(launch.main(arguments), 1)
                broker.assert_called_once_with(arguments, str(game), {})
                direct.assert_not_called()

    def test_flat_desktop_child_runs_once_with_flat_environment_and_no_xr(self):
        import launch_plus
        for code in (0, 0xc0000005):
            with self.subTest(code=code), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary).resolve()
                plugins = root / 'mods/D2R-PCVR/d2rloader/plugins'
                plugins.mkdir(parents=True)
                for name in ('D2R.exe', 'D2RLoader.exe', 'mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll'):
                    (root / name).write_bytes(b'fixture never executed')
                (plugins / 'd2r_pcvr_plus_flat.ini').write_text('[mode]\nplatform=0\n')
                request = root / 'request.json'
                request.write_text(json.dumps({'arguments': [str(root / 'D2R.exe'), '--frame-flat'], 'runtime': 'missing.json'}))
                with patch.object(launch.sys, 'platform', 'win32'), \
                        patch('game_install.validate_game'), patch('process_guard.require_game_closed'), \
                        patch.object(launch, 'launch_from_desktop', side_effect=AssertionError('No recursion')), \
                        patch.object(launch, 'launch_environment', side_effect=AssertionError('No XR')), \
                        patch.object(launch_plus.subprocess, 'run', return_value=Mock(returncode=code)) as run:
                    self.assertEqual(launch.desktop_child(request), code)
                run.assert_called_once()
                self.assertEqual(run.call_args.args[0], [str(root / 'D2RLoader.exe'), '-mod', 'D2R-PCVR'])
                environment = run.call_args.kwargs['env']
                self.assertEqual(environment['D2R_PCVR_PLUS_MODE'], 'flat')
                self.assertEqual(environment['SteamAppId'], '2536520')
                self.assertNotEqual(environment.get('XR_RUNTIME_JSON'), 'missing.json')
                self.assertEqual(json.loads((root / 'status.json').read_text()), {'state': 'finished', 'result': code})
                self.assertIn(f'Flat loader exited code=0x{code:08x}', (root / 'd2rloader/logs/pcvr-launch.log').read_text())

    def test_flat_startup_failure_is_logged_and_reported(self):
        import launch_plus
        with tempfile.TemporaryDirectory() as temporary:
            game = Path(temporary).resolve()
            request = game / 'request.json'
            request.write_text(json.dumps({'arguments': [str(game / 'D2R.exe'), '--frame-flat']}))
            with patch.object(launch_plus, 'main', side_effect=ValueError('Missing Flat profile')) as start:
                self.assertEqual(launch.desktop_child(request), 1)
                start.assert_called_once()
            self.assertEqual(json.loads((game / 'status.json').read_text())['result'], 1)
            self.assertIn('Missing Flat profile', (game / 'd2rloader/logs/pcvr-launch.log').read_text())

    def test_stock_uses_original_subprocess_without_desktop_handoff(self):
        with patch.object(launch, 'prepare', return_value=(['D2R.exe', 'argument'], None)), \
                patch.object(launch, 'launch_from_desktop') as broker, \
                patch.object(launch.subprocess, 'run', return_value=Mock(returncode=7)) as run:
            self.assertEqual(launch.main(['D2R.exe', 'argument']), 7)
            broker.assert_not_called()
            run.assert_called_once_with(['D2R.exe', 'argument'], cwd=None, env=None)

    @unittest.skipUnless(os.name == 'nt', 'Native Windows launch routing')
    def test_steam_vr_hands_off_once_without_a_direct_fallback(self):
        with patch.object(launch, 'prepare', return_value=(['D2RLoader.exe'], 'game')), \
                patch.object(launch, 'launch_environment', return_value={'SteamAppId': '2536520'}), \
                patch.object(launch, 'launch_from_desktop', return_value=1) as broker, \
                patch.object(launch.subprocess, 'run') as run:
            self.assertEqual(launch.main(['D2R.exe', '--frame-vr']), 1)
            broker.assert_called_once_with(['D2R.exe', '--frame-vr'], 'game', {'SteamAppId': '2536520'})
            run.assert_not_called()

    def test_battlenet_keeps_its_existing_direct_route(self):
        arguments = ['--battlenet-game=game', '--frame-vr']
        with patch.object(launch, 'prepare', return_value=(['D2RLoader.exe'], 'game')), \
                patch.object(launch, 'launch_environment', return_value={'FIXTURE': 'keep'}), \
                patch.object(launch, 'launch_from_desktop') as broker, \
                patch.object(launch.subprocess, 'run', return_value=Mock(returncode=0)) as run:
            self.assertEqual(launch.main(arguments), 0)
            broker.assert_not_called()
            run.assert_called_once_with(['D2RLoader.exe'], cwd='game', env={'FIXTURE': 'keep'})

    def test_child_waits_for_one_game_and_preserves_native_exit_codes(self):
        for result in (0, 0xc0000005):
            with self.subTest(result=result), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                request = root / 'request.json'
                request.write_text(json.dumps({'arguments': ['D2R.exe', '--frame-vr'], 'runtime': 'selected-runtime'}))
                process = Mock(pid=456)
                def wait():
                    self.assertEqual(json.loads((root / 'status.json').read_text())['state'], 'running')
                    return result
                process.wait.side_effect = wait
                with patch.object(launch, 'prepare', return_value=(['loader', 'argument'], str(root))), \
                        patch.object(launch, 'launch_environment', return_value={'SteamAppId': '2536520'}), \
                        patch.object(launch.subprocess, 'Popen', return_value=process) as popen:
                    self.assertEqual(launch.desktop_child(request), result)
                    popen.assert_called_once_with(['loader', 'argument'], cwd=str(root),
                        env={'SteamAppId': '2536520', 'XR_RUNTIME_JSON': 'selected-runtime'})
                    process.wait.assert_called_once_with()
                self.assertEqual(json.loads((root / 'status.json').read_text()), {'state': 'finished', 'result': result})
                self.assertIn(f'code=0x{result:08x}', (root / 'd2rloader/logs/pcvr-launch.log').read_text())

    def test_child_start_error_is_reported_without_retry(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            request = root / 'request.json'
            request.write_text(json.dumps({'arguments': ['D2R.exe', '--frame-vr']}))
            with patch.object(launch, 'prepare', return_value=(['loader'], str(root))), \
                    patch.object(launch.subprocess, 'Popen', side_effect=OSError('fixture refused')) as popen:
                self.assertEqual(launch.desktop_child(request), 1)
                popen.assert_called_once()
            status = json.loads((root / 'status.json').read_text())
            self.assertEqual(status['state'], 'finished')
            self.assertEqual(status['result'], 1)
            self.assertIn('fixture refused', status['error'])

    def test_child_rejects_stock_or_battlenet_requests_before_creating_game(self):
        for arguments in (['D2R.exe'], ['--battlenet-game=game', '--frame-vr']):
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                request = root / 'request.json'
                request.write_text(json.dumps({'arguments': arguments}))
                with patch.object(launch.subprocess, 'Popen') as popen:
                    self.assertEqual(launch.desktop_child(request), 1)
                    popen.assert_not_called()
                self.assertEqual(json.loads((root / 'status.json').read_text())['result'], 1)

    def test_paths_and_arguments_are_json_data_not_powershell_syntax(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            captured = {}
            args = [str(root / "O'Brien `$(example) % folder/D2R.exe"), '--frame-vr', 'argument with spaces']
            def powershell(command, **kwargs):
                script = base64.b64decode(command[-1]).decode('utf-16-le')
                payload = json.loads(base64.b64decode(re.search(r"FromBase64String\('([^']+)'\)", script)[1]))
                captured.update(payload)
                self.assertNotIn('O\'Brien', script)
                self.assertTrue(kwargs['check'])
                self.assertEqual(kwargs['timeout'], 20)
            def wait(status):
                request = status.with_name('request.json')
                captured['request'] = request
                data = json.loads(request.read_text())
                self.assertEqual(data, {'arguments': args, 'runtime': 'chosen-runtime'})
                return 0
            with patch.object(launch.subprocess, 'run', side_effect=powershell), \
                    patch.object(launch, 'wait_for_desktop', side_effect=wait):
                self.assertEqual(launch.launch_from_desktop(args, str(root), {'XR_RUNTIME_JSON': 'chosen-runtime'}), 0)
            self.assertEqual(captured['executable'], str(Path(sys.executable).resolve()))
            self.assertIn(launch.DESKTOP_REQUEST, captured['arguments'])
            self.assertFalse(captured['request'].exists())

    def test_broker_failure_never_launches_a_second_game(self):
        with tempfile.TemporaryDirectory() as temporary, \
                patch.object(launch.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'powershell')), \
                patch.object(launch, 'wait_for_desktop') as wait, \
                patch.object(launch.subprocess, 'Popen') as popen:
            self.assertEqual(launch.launch_from_desktop(['D2R.exe', '--frame-vr'], temporary, {}), 1)
            wait.assert_not_called()
            popen.assert_not_called()

    @unittest.skipUnless(os.name == 'nt', 'Windows process-handle lifecycle')
    def test_wait_observes_completion_and_helper_crash_without_hanging(self):
        for complete in (True, False):
            with self.subTest(complete=complete), tempfile.TemporaryDirectory() as temporary:
                status = Path(temporary) / 'status.json'
                script = ('import os,sys,time;from pathlib import Path;'
                          'from launch_windows import publish_status;'
                          'p=Path(sys.argv[1]);publish_status(p,state="running",pid=os.getpid());'
                          'time.sleep(.2);' +
                          ('publish_status(p,state="finished",result=37)' if complete else 'os._exit(7)'))
                process = subprocess.Popen([sys.executable, '-c', script, str(status)], cwd=Path(launch.__file__).parent)
                try:
                    if complete:
                        self.assertEqual(launch.wait_for_desktop(status, timeout=5), 37)
                    else:
                        with self.assertRaisesRegex(OSError, 'without a result'):
                            launch.wait_for_desktop(status, timeout=5)
                finally:
                    process.wait(timeout=5)

    @unittest.skipUnless(os.name == 'nt', 'Windows process-handle lifecycle')
    def test_missing_desktop_acknowledgement_has_bounded_wait(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(OSError, 'did not acknowledge'):
                launch.wait_for_desktop(Path(temporary) / 'missing.json', timeout=.02)


if __name__ == '__main__':
    unittest.main()
