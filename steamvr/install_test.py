#!/usr/bin/env python3
"""Installer reversibility and portable launch routing, using temporary fixtures."""
import contextlib
import configparser
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import install
import launch_windows


class InstallationTests(unittest.TestCase):
    def test_install_restore_keeps_progress_and_stock_executable(self):
        self.install_round_trip('steam')

    @unittest.skipUnless(os.name == 'nt', 'Battle.net preview targets native Windows')
    def test_battlenet_install_restore_keeps_progress_and_stock_executable(self):
        self.install_round_trip('battlenet')

    def install_round_trip(self, store):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'library/steamapps/common/Diablo II Resurrected'
            game.mkdir(parents=True)
            exe = ('3.3.93787' if store == 'steam' else '3.3.93847').encode('utf-16-le')
            (game / 'D2R.exe').write_bytes(exe)
            (game / 'Data').mkdir()
            (game / ('Data/.build.config' if store == 'steam' else '.build.info')).write_text('fixture')
            (game / 'ReShade.ini').write_text('existing config')
            icon_dir = game / 'd2rloader/plugins/d2r_vr_icons'
            icon_dir.mkdir(parents=True)
            (icon_dir / 'manifest.tsv').write_text('old local cache')
            potion_dir = game / 'd2rloader/plugins/d2r_vr_potions'
            potion_dir.mkdir()
            (potion_dir / 'hp1.pvr').write_bytes(b'previous potion')
            # On Windows default_saves points at the user's REAL Saved Games.
            # Fixtures must never install into or restore that directory.
            saves = root / 'saves'
            saves.mkdir(parents=True)
            (saves / 'Settings.json').write_text('{"Resolution Scale": 80}')
            (saves / 'Friend.d2s').write_bytes(b'original progress')
            deps = root / 'deps'
            deps.mkdir()
            archive = deps / 'D2RLoader-1.3.1-beta.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('D2RLoader.exe', b'fixture loader')
                z.writestr('d2rloader/config/d2rloader.toml', 'fixture config')
                z.comment = b'current loader archive fixture'
            current_archive = archive.read_bytes()
            historical_archive = io.BytesIO(current_archive)
            with zipfile.ZipFile(historical_archive, 'a') as z:
                z.comment = b'historical loader archive fixture'
            historical_archive = historical_archive.getvalue()
            # Distinct ZIP packaging with the same harmless install payload.
            # Both pins must pass final preflight, independently of downloading.
            loader_hashes = tuple(hashlib.sha256(data).hexdigest()
                                  for data in (current_archive, historical_archive))
            (deps / 'reshade-6.8').mkdir()
            (deps / 'reshade-6.8/ReShade64.dll').write_bytes(b'fixture reshade')
            compiler = deps / 'd3dcompiler47/modern/d3dcompiler_47.dll'
            compiler.parent.mkdir(parents=True)
            compiler.write_bytes(b'fixture compiler')
            plugin = root / 'plugin.dll'
            plugin.write_bytes(b'fixture plugin')
            loader = root / 'openxr_loader.dll'
            loader.write_bytes(b'fixture xr loader')
            common = ['install.py', '--game', str(game), '--deps', str(deps),
                      '--plugin', str(plugin), '--openxr-loader', str(loader),
                      '--saves', str(saves), '--backup-dir', str(root / 'backups')]
            if store == 'battlenet':
                casc = root / 'CascLib.dll'
                casc.write_bytes(b'fixture CASC library, cache decoding is mocked')
                common += ['--store', store, '--experimental-battlenet', '--casclib', str(casc)]
            def icons(game, output, **kwargs):
                out = Path(output)
                for name in ('fixture.png', 'manifest.tsv', 'cache.json'):
                    (out / name).write_bytes(b'local generated fixture')
                return {'images': ['fixture.png'], 'icons': 1}
            def potions(game, output, **kwargs):
                for name in [code + '.pvr' for code in install.POTION_CODES] + ['cache.json']:
                    (Path(output) / name).write_bytes(b'local potion fixture')
                return {'models': len(install.POTION_CODES)}
            with patch.object(install, 'require_game_closed'), contextlib.redirect_stdout(io.StringIO()), \
                    patch.object(install, 'COMPILER_HASH', hashlib.sha256(compiler.read_bytes()).hexdigest()), \
                    patch.object(install, 'prepare_icons', side_effect=icons), \
                    patch.object(install, 'prepare_potions', side_effect=potions):
                unapproved_hashes = ('0' * 64, '1' * 64)
                with patch.object(sys, 'argv', common), patch.object(install, 'LOADER_HASHES', unapproved_hashes):
                    with self.assertRaises(SystemExit) as caught:
                        install.main()
                message = str(caught.exception)
                self.assertIn(str(archive), message)
                for digest in (*unapproved_hashes, loader_hashes[0]):
                    self.assertIn(digest, message)
                self.assertIn('No game files changed', message)
                self.assertNotIn('\n', message)
                self.assertFalse((root / 'backups').exists())
                read_bytes = Path.read_bytes
                def blocked_archive(path):
                    if path == archive:
                        raise OSError(22, 'Invalid argument', str(path))
                    return read_bytes(path)
                with patch.object(sys, 'argv', common), patch.object(install, 'LOADER_HASHES', loader_hashes), \
                        patch.object(Path, 'read_bytes', blocked_archive):
                    with self.assertRaisesRegex(SystemExit, 'Protection history'):
                        install.main()
                self.assertFalse((root / 'backups').exists())
                self.assertFalse((game / 'D2RLoader.exe').exists())
                self.assertEqual((saves / 'Settings.json').read_text(), '{"Resolution Scale": 80}')
                self.assertEqual((saves / 'Friend.d2s').read_bytes(), b'original progress')
                with patch.object(sys, 'argv', common), patch.object(install, 'LOADER_HASHES', loader_hashes):
                    install.main()
                snapshot = next((root / 'backups').iterdir())
                self.assertEqual(list((snapshot / 'saves').iterdir()), [snapshot / 'saves/Settings.json'])
                flat = game / 'd2rloader/plugins/d2r_pcvr_plus_flat.ini'
                self.assertIn(b'flat_layout=WASD', flat.read_bytes().replace(b' ', b''))
                record = install.read_record(game)
                self.assertEqual(record['snapshots'], [str(snapshot.resolve())])
                self.assertEqual(record['store'], store)
                self.assertEqual((game / 'D2R.exe').read_bytes(), exe)
                self.assertFalse((game / 'dxgi.dll').exists())
                self.assertEqual((icon_dir / 'fixture.png').read_bytes(), b'local generated fixture')
                self.assertEqual((potion_dir / 'hp1.pvr').read_bytes(), b'local potion fixture')
                self.assertTrue((potion_dir / 'rvl.pvr').is_file())
                self.assertEqual((game / 'd2rloader/runtime/d3dcompiler_47.dll').read_bytes(), compiler.read_bytes())
                self.assertFalse((game / 'd3dcompiler_47.dll').exists())
                cfg=configparser.ConfigParser(allow_no_value=True)
                cfg.read(game / 'd2rloader/plugins/d2r_vr.ini')
                # Python merges duplicate sections differently from Win32.
                # Check the actual API used by the mod, including the near plane.
                if os.name == 'nt':
                    import ctypes
                    read_profile = ctypes.WinDLL('kernel32').GetPrivateProfileStringW
                    read_profile.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p,
                                             ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_wchar_p]
                    for key, expected in [('near', '0.2'), ('fov', '64'), ('height', '6.3')]:
                        value = ctypes.create_unicode_buffer(64)
                        read_profile('camera', key, 'missing', value, len(value),
                                     str(game / 'd2rloader/plugins/d2r_vr.ini'))
                        self.assertEqual(value.value, expected)
                self.assertTrue(cfg.getboolean('render','symmetric_projection'))
                self.assertTrue(cfg.getboolean('hud','monster_draw_order'))
                self.assertFalse(cfg.getboolean('render','defer_gpu_wait'))
                self.assertEqual(json.loads((saves / 'Settings.json').read_text())['NVIDIA DLSS'],0)
                self.assertEqual(json.loads((saves / 'Settings.json').read_text())['Resolution Scale'], 100)
                # An upgrade refreshes files without resetting calibration/video.
                custom_ini = b'[body_calibration]\nheight=1.83\nspan=1.91\n'
                custom_video = b'{"Resolution Scale": 92, "custom": "preserved"}\r\n'
                (game / 'd2rloader/plugins/d2r_vr.ini').write_bytes(custom_ini)
                (saves / 'Settings.json').write_bytes(custom_video)
                (game / 'ReShade.ini').write_bytes(b'custom reshade settings')
                flat.write_bytes(b'[input]\nflat_layout=ZQSD\n')
                plugin.write_bytes(b'updated fixture plugin')
                archive.write_bytes(historical_archive)
                with patch.object(sys, 'argv', common + ['--preserve-settings']), \
                        patch.object(install, 'LOADER_HASHES', loader_hashes):
                    install.main()
                self.assertEqual((game / 'd2rloader/plugins/d2r_vr.ini').read_bytes(), custom_ini)
                self.assertEqual((saves / 'Settings.json').read_bytes(), custom_video)
                self.assertEqual((game / 'ReShade.ini').read_bytes(), b'custom reshade settings')
                self.assertEqual(flat.read_bytes(), b'[input]\nflat_layout=ZQSD\n')
                self.assertEqual((game / 'd2rloader/plugins/d2rl-vrcam.dll').read_bytes(), b'updated fixture plugin')
                record = install.read_record(game)
                self.assertEqual(len(record['snapshots']), 2)
                self.assertEqual(record['snapshots'][0], str(snapshot.resolve()))
                (saves / 'Friend.d2s').write_bytes(b'new progress')
                with patch.object(sys, 'argv', ['install.py', '--game', str(game), '--restore', str(snapshot)]):
                    install.main()
                self.assertEqual((saves / 'Friend.d2s').read_bytes(), b'new progress')
                self.assertEqual((game / 'ReShade.ini').read_text(), 'existing config')
                self.assertEqual((game / 'D2R.exe').read_bytes(), exe)
                self.assertFalse((game / 'D2RLoader.exe').exists())
                self.assertFalse((game / install.RECORD).exists())
                self.assertFalse((icon_dir / 'fixture.png').exists())
                self.assertEqual((icon_dir / 'manifest.tsv').read_text(), 'old local cache')
                self.assertEqual((potion_dir / 'hp1.pvr').read_bytes(), b'previous potion')
                self.assertFalse((potion_dir / 'rvl.pvr').exists())
                self.assertFalse((game / 'openxr_loader.dll').exists())
                self.assertFalse((game / 'd2rloader/runtime/d3dcompiler_47.dll').exists())
                self.assertEqual(json.loads((saves / 'Settings.json').read_text())['Resolution Scale'], 80)

    def test_archive_paths_reject_windows_and_linux_traversal(self):
        for path in ('../D2R.exe', '/tmp/file', 'C:\\file', 'd2rloader\\..\\D2R.exe'):
            with self.subTest(path=path), self.assertRaises(SystemExit):
                install.safe_relative(path)

    def test_windows_steam_arguments_and_paths_with_spaces(self):
        original = ['C:\\Steam Library\\D2R.exe', '-some-argument', 'a value']
        self.assertEqual(launch_windows.prepare(original), (original, None))
        with patch.object(Path, 'is_file', return_value=True):
            command, cwd = launch_windows.prepare(original + ['--frame-vr'])
        self.assertEqual(command, ['C:\\Steam Library\\D2RLoader.exe', '-some-argument', 'a value'])
        self.assertEqual(cwd, 'C:\\Steam Library')

    def test_windows_runtime_override_only_applies_to_explicit_vr(self):
        with tempfile.TemporaryDirectory() as temporary:
            runtime = Path(temporary) / 'steamxr_win64.json'
            runtime.write_text('{}')
            options = [str(runtime.parent / 'D2R.exe'),
                       launch_windows.RUNTIME_OPTION + str(runtime)]
            self.assertEqual(launch_windows.prepare(options), ([options[0]], None))
            self.assertIsNone(launch_windows.launch_environment(options))
            with patch.dict(os.environ, {'XR_RUNTIME_JSON': 'other-runtime', 'SteamAppId': '2536520'}):
                environment = launch_windows.launch_environment(options + ['--frame-vr'])
                self.assertEqual(environment['XR_RUNTIME_JSON'], str(runtime.resolve()))
                # Windows normalizes environment variable names to uppercase.
                self.assertEqual({k.upper(): v for k, v in environment.items()}['STEAMAPPID'], '2536520')
                self.assertEqual(os.environ['XR_RUNTIME_JSON'], 'other-runtime')

    def test_windows_missing_runtime_override_is_rejected_before_launch(self):
        with self.assertRaises(SystemExit):
            launch_windows.launch_environment(['--frame-vr',
                launch_windows.RUNTIME_OPTION + 'missing-runtime.json'])

    def test_windows_vr_skips_early_overlay_routing_but_preserves_steam_ownership(self):
        with tempfile.TemporaryDirectory() as temporary:
            runtime = Path(temporary) / 'steamxr_win64.json'
            runtime.write_text('{}')
            inherited = {'SteamAppId': '2536520', 'SteamGameId': '2536520',
                         'STEAMOVERLAYGAMEID': '2536520', 'STEAMNOOVERLAYUI': '1',
                         'XR_RUNTIME_JSON': 'previous-runtime', 'D2R_FIXTURE': 'keep'}
            with patch.dict(os.environ, inherited, clear=True), \
                    patch.object(launch_windows, 'vdxr_manifest', return_value=runtime):
                before = dict(os.environ)
                modes = [[], [launch_windows.RUNTIME_OPTION + str(runtime)],
                         [launch_windows.VDXR_OPTION]]
                for mode in modes:
                    with self.subTest(mode=mode):
                        environment = {k.lower(): v for k, v in
                                       launch_windows.launch_environment(['--frame-vr'] + mode).items()}
                        self.assertNotIn('steamoverlaygameid', environment)
                        self.assertEqual(environment['steamappid'], '2536520')
                        self.assertEqual(environment['steamgameid'], '2536520')
                        self.assertEqual(environment['steamnooverlayui'], '1')
                        self.assertEqual(environment['d2r_fixture'], 'keep')
                        self.assertEqual(environment['xr_runtime_json'],
                                         str(runtime.resolve()) if mode else 'previous-runtime')
                        self.assertEqual(dict(os.environ), before)
                self.assertIsNone(launch_windows.launch_environment(
                    ['D2R.exe', launch_windows.RUNTIME_OPTION + str(runtime)]))
                self.assertEqual(dict(os.environ), before)

    def test_explicit_vdxr_overrides_configured_steamvr_without_changing_stock_play(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            steamvr = root / 'Steam VR/steamxr_win64.json'
            vdxr = root / 'Virtual Desktop/OpenXR/virtualdesktop-openxr.json'
            for runtime in (steamvr, vdxr):
                runtime.parent.mkdir(parents=True)
                runtime.write_text('{}')
            original = ['C:\\Steam Library\\D2R.exe', '-some-argument', 'a value']
            options = original + [launch_windows.RUNTIME_OPTION + str(steamvr),
                                  launch_windows.VDXR_OPTION]
            self.assertEqual(launch_windows.prepare(options), (original, None))
            self.assertIsNone(launch_windows.launch_environment(options))
            with patch.object(launch_windows, 'vdxr_manifest', return_value=vdxr), \
                    patch.dict(os.environ, {'XR_RUNTIME_JSON': 'other-runtime', 'SteamAppId': '2536520'}), \
                    patch.object(Path, 'is_file', return_value=True):
                command, cwd = launch_windows.prepare(options + ['--frame-vr'])
                self.assertEqual(command, ['C:\\Steam Library\\D2RLoader.exe'] + original[1:])
                self.assertEqual(cwd, 'C:\\Steam Library')
                for arguments in (options + ['--frame-vr'], ['--frame-vr', launch_windows.VDXR_OPTION]):
                    environment = launch_windows.launch_environment(arguments)
                    self.assertEqual(environment['XR_RUNTIME_JSON'], str(vdxr.resolve()))
                    self.assertEqual({k.upper(): v for k, v in environment.items()}['STEAMAPPID'], '2536520')
                self.assertEqual(os.environ['XR_RUNTIME_JSON'], 'other-runtime')

    def test_missing_vdxr_does_not_fall_back_to_the_configured_steamvr(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            steamvr = root / 'steamxr_win64.json'
            steamvr.write_text('{}')
            with patch.object(launch_windows, 'vdxr_manifest', return_value=root / 'missing.json'):
                with self.assertRaisesRegex(SystemExit, 'manifest was not found'):
                    launch_windows.launch_environment(['--frame-vr', launch_windows.VDXR_OPTION,
                        launch_windows.RUNTIME_OPTION + str(steamvr)])

    def test_multiple_configured_runtime_manifests_remain_rejected(self):
        for extra in ([], [launch_windows.VDXR_OPTION]):
            with self.subTest(extra=extra), self.assertRaisesRegex(SystemExit, 'at most one'):
                launch_windows.launch_environment(['--frame-vr',
                    launch_windows.RUNTIME_OPTION + 'first.json',
                    launch_windows.RUNTIME_OPTION + 'second.json'] + extra)


class DependencyDownloadTests(unittest.TestCase):
    payload = b'fixture dependency; no executable code'
    digest = hashlib.sha256(payload).hexdigest()
    historical_payload = b'historical fixture dependency; no executable code'
    historical_digest = hashlib.sha256(historical_payload).hexdigest()
    approved_digests = (digest, historical_digest)
    url = 'https://example.invalid/fixture.zip'

    def test_verified_manual_cache_is_reused_without_network_or_replacement(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            target.write_bytes(self.payload)
            with patch.object(install.urllib.request, 'urlopen') as download, \
                    patch.object(Path, 'write_bytes') as write, \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                install.download_verified(self.url, target, self.digest)
            download.assert_not_called()
            write.assert_not_called()
            self.assertIn('Using verified cached dependency', output.getvalue())
            self.assertEqual(target.read_bytes(), self.payload)

    def test_current_download_accepts_explicit_reviewed_hash_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            with patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)) as download, \
                    contextlib.redirect_stdout(io.StringIO()):
                install.download_verified(self.url, target, self.approved_digests)
            download.assert_called_once()
            self.assertEqual(target.read_bytes(), self.payload)

    def test_historical_cache_is_reused_from_reviewed_hash_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            target.write_bytes(self.historical_payload)
            with patch.object(install.urllib.request, 'urlopen') as download, \
                    patch.object(Path, 'write_bytes') as write, \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                install.download_verified(self.url, target, self.approved_digests)
            download.assert_not_called()
            write.assert_not_called()
            self.assertIn('Using verified cached dependency', output.getvalue())
            self.assertEqual(target.read_bytes(), self.historical_payload)

    def test_unlisted_download_reports_hashes_and_preserves_existing_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            original = b'old, unverified cache'
            unlisted = b'unreviewed replacement at the same URL'
            actual = hashlib.sha256(unlisted).hexdigest()
            target.write_bytes(original)
            with patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(unlisted)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    install.download_verified(self.url, target, self.approved_digests)
            message = str(caught.exception)
            self.assertIn('Checksum mismatch', message)
            self.assertIn(self.url, message)
            for digest in (*self.approved_digests, actual):
                self.assertIn(digest, message)
            self.assertIn('No game files changed', message)
            self.assertNotIn('\n', message)
            self.assertEqual(target.read_bytes(), original)

    def test_bad_download_keeps_existing_cache_and_enforces_checksum(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            target.write_bytes(b'old, unverified cache')
            with patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(b'wrong content')), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(SystemExit, 'Checksum mismatch'):
                    install.download_verified(self.url, target, self.digest)
            self.assertEqual(target.read_bytes(), b'old, unverified cache')

    def test_valid_download_replaces_invalid_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            target.write_bytes(b'invalid cache')
            with patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                install.download_verified(self.url, target, self.digest)
            self.assertEqual(target.read_bytes(), self.payload)

    def test_unreadable_cache_reports_path_and_os_error_without_redownloading(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            target.write_bytes(self.payload)
            with patch.object(Path, 'read_bytes', side_effect=OSError(22, 'Invalid argument')), \
                    patch.object(install.urllib.request, 'urlopen') as download:
                with self.assertRaises(SystemExit) as caught:
                    install.download_verified(self.url, target, self.digest)
            message = str(caught.exception)
            self.assertIn(str(target), message)
            self.assertIn('[Errno 22]', message)
            self.assertIn('Protection history', message)
            download.assert_not_called()
            self.assertEqual(target.read_bytes(), self.payload)

    def test_file_blocked_after_download_is_reported_immediately(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            with patch.object(Path, 'read_bytes', side_effect=[FileNotFoundError(), OSError(22, 'Invalid argument')]), \
                    patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(SystemExit, 'Protection history'):
                    install.download_verified(self.url, target, self.digest)
            self.assertEqual(target.read_bytes(), self.payload)

    def test_file_removed_after_download_reports_missing_dependency(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            write_bytes = Path.write_bytes
            def removed_write(path, data):
                size = write_bytes(path, data)
                path.unlink()
                return size
            with patch.object(Path, 'write_bytes', removed_write), \
                    patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(SystemExit, 'missing, blocked/quarantined'):
                    install.download_verified(self.url, target, self.digest)
            self.assertFalse(target.exists())

    def test_blocked_write_is_explained(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            with patch.object(Path, 'write_bytes', side_effect=PermissionError(13, 'Permission denied')), \
                    patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(SystemExit, 'Could not save required dependency'):
                    install.download_verified(self.url, target, self.digest)
            self.assertFalse(target.exists())

    def test_changed_file_after_save_fails_checksum(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            write_bytes = Path.write_bytes
            def changed_write(path, data):
                return write_bytes(path, b'changed content')
            with patch.object(Path, 'write_bytes', changed_write), \
                    patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(SystemExit, 'Checksum mismatch for saved dependency'):
                    install.download_verified(self.url, target, self.digest)

    def test_saved_file_must_match_download_even_when_replaced_by_other_approved_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / 'fixture.zip'
            write_bytes = Path.write_bytes
            def swapped_write(path, data):
                self.assertEqual(data, self.payload)
                return write_bytes(path, self.historical_payload)
            with patch.object(Path, 'write_bytes', swapped_write), \
                    patch.object(install.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    install.download_verified(self.url, target, self.approved_digests)
            message = str(caught.exception)
            self.assertIn('Checksum mismatch for saved dependency', message)
            self.assertIn(str(target), message)
            self.assertIn(f'Expected SHA-256: {self.digest}', message)
            self.assertIn(f'received: {self.historical_digest}', message)
            self.assertEqual(target.read_bytes(), self.historical_payload)


if __name__ == '__main__':
    unittest.main()
