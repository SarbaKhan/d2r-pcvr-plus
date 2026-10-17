"""Installer integration fixtures; never use a real Steam profile or game."""
import json
import os
import shutil
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import plus_artwork as art
import plus_preferences as prefs
import setup_linux
import setup_native
import setup_native_test
import setup_windows
import steam_config as steam

ART = Path(__file__).resolve().parent.parent / 'artwork'


class IntegrationTests(unittest.TestCase):
    def test_windows_shortcuts_only_applies_preferences_art_and_preserves_vr(self):
        with tempfile.TemporaryDirectory() as tmp:
            fixture = setup_native_test.NativeSetupTests()
            app,game,client,saves,profile,_ = fixture.fixture(Path(tmp).resolve())
            shutil.copytree(ART, app/'artwork')
            vr = fixture.put(game/'mods/D2R-PCVR/d2rloader/plugins/d2r_pcvr.ini', b'calibrated VR settings')
            fixture.put(game/'mods/D2R-PCVR/d2rloader/plugins/d2r_pcvr_plus_flat.ini', b'[input]\nflat_layout=WASD\ncustom=keep\n')
            loader = fixture.put(game/'d2rloader/config/d2rloader.toml', b'text_locale=""\naudio_locale=""\ncustom=true\n')
            callback = []
            with patch.object(setup_windows,'require_closed'), patch.object(setup_windows,'run_installer') as installer:
                setup_windows.apply_setup({},'steam','fixture','Quest 2',['flat','steamvr','vdxr'],
                    client,game,profile,saves,None,2,root=app,
                    preferences={'layout':'ZQSD','text_locale':'frFR','audio_locale':'jaJP'},
                    on_steam_configured=lambda originals: callback.append(originals))
                installer.assert_not_called()
            self.assertEqual(prefs.preferences(game), dict(layout='ZQSD', text_locale='frFR', audio_locale='jaJP'))
            self.assertEqual(vr.read_bytes(), b'calibrated VR settings')
            self.assertIn(b'custom=true', loader.read_bytes())
            self.assertEqual((saves/'Character.d2s').read_bytes(), b'current character')
            self.assertEqual(len(list((profile/'config/grid').glob('*.png'))), 12)
            self.assertEqual(len(list((game/'d2rloader/pcvr-plus-icons').glob('*.png'))), 3)
            self.assertEqual(callback[0][loader], b'text_locale=""\naudio_locale=""\ncustom=true\n')

    def test_packaged_assets_and_dimensions(self):
        import struct
        expected = {'icon': (512, 512), 'cover': (600, 900), 'wide-cover': (920, 430),
                    'background': (1920, 620), 'logo': (1600, 1259)}
        for mode, files in art.assets(ART, list(art.NAMES)).items():
            for kind, (_, data) in files.items():
                self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
                self.assertEqual(struct.unpack('>II', data[16:24]), expected[kind])
                if kind in ('logo', 'icon'):
                    self.assertEqual(data[25], 6, 'Transparent assets must be RGBA PNGs')

    def test_windows_legacy_rename_preserves_id_history_and_other_game(self):
        for store in ('steam', 'battlenet'):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp).resolve()
                routing = {} if store == 'steam' else dict(store=store, python=root/'python.exe',
                    wrapper=root/'steamvr/launch_windows.py', runtime=root/'runtime.json')
                payload = steam.update_shortcuts(None, root, root/'game', ['flat','steamvr','vdxr'], **routing)
                tree = steam.decode_binary(payload)
                ids = {}
                for mode, row in zip(('flat','steamvr','vdxr'), tree['shortcuts'].values()):
                    row['appname'] = steam.LEGACY[store][mode]
                    row.pop('D2RPCVRPlusGame'); row.pop('D2RPCVRPlusStore')
                    row['LastPlayTime'] = 12345
                    row['tags'] = {'0': 'My favorites'}
                    ids[mode] = row['appid']
                tree['shortcuts']['9'] = {'appname': 'Unrelated', 'appid': 12, 'icon':'unchanged'}
                payload = steam.update_shortcuts(steam.encode_binary(tree), root, root/'game', ['flat','steamvr','vdxr'], **routing)
                payload, updates = art.steam_updates(payload, root/'profile', root/'game', ['flat','steamvr','vdxr'], ART)
                self.assertEqual(len(updates), 15)
                rows = steam.decode_binary(payload)['shortcuts']
                for mode, row in zip(('flat','steamvr','vdxr'), list(rows.values())[:3]):
                    self.assertEqual(row['appname'], art.NAMES[mode])
                    self.assertEqual(row['appid'], ids[mode])
                    self.assertEqual(row['LastPlayTime'], 12345)
                    self.assertEqual(row['tags'], {'0':'My favorites'})
                    for kind,suffix in art.GRID.items():
                        self.assertEqual(updates[root/'profile/config/grid'/f'{ids[mode]}{suffix}'],
                                         (ART/art.FOLDERS[mode]/(kind+'.png')).read_bytes())
                # Steam can discard custom fields when saving a profile. Icon
                # ownership and launch routing must still identify the installation.
                stripped = steam.decode_binary(payload)
                for row in stripped['shortcuts'].values():
                    row.pop('D2RPCVRPlusGame', None); row.pop('D2RPCVRPlusStore', None)
                again = steam.update_shortcuts(steam.encode_binary(stripped), root, root/'game', ['flat','steamvr','vdxr'], **routing)
                again, _ = art.steam_updates(again, root/'profile', root/'game', ['flat','steamvr','vdxr'], ART)
                self.assertEqual(payload, again)
                self.assertEqual(rows['9'], tree['shortcuts']['9'])

    def test_desktop_entries_xdg_wivrn_and_manual_migration(self):
        with tempfile.TemporaryDirectory(prefix='Plus é ') as tmp:
            root = Path(tmp).resolve()
            applications = root/'custom-data/applications'
            applications.mkdir(parents=True)
            manual = applications/'d2r-pcvr-plus-wivrn.desktop'
            manual.write_text('[Desktop Entry]\nType=Application\nName=D2R PCVR Plus - WiVRn\n'
                              'Exec=/usr/bin/steam -applaunch 2536520 --plus-wivrn\nIcon=steam\nCategories=Game;X-WiVRn-VR;\n')
            with patch.dict(os.environ, {'XDG_DATA_HOME':str(applications.parent)}):
                updates = art.desktop_updates('/usr/bin/steam', root/'game', ['flat','steamvr','wivrn'], ART)
                self.assertEqual(len(updates), 3)
                for path, data in updates.items():
                    self.assertEqual(path.parent, applications)
                    text=data.decode()
                    self.assertEqual('X-WiVRn-VR;' in text, path == manual)
                    self.assertNotIn('PROTON_ENABLE_WAYLAND', text)
                    self.assertIn('Icon='+art.desktop_string(ART), text)
                    path.write_bytes(data)
                self.assertEqual(updates, art.desktop_updates('/usr/bin/steam', root/'game', ['flat','steamvr','wivrn'], ART))
                manual.write_text('[Desktop Entry]\nName=Another application\nExec=unrelated\n')
                with self.assertRaisesRegex(ValueError, 'unrelated desktop'):
                    art.desktop_updates('/usr/bin/steam', root/'game', ['wivrn'], ART)

    def test_desktop_exec_escaping_and_paths(self):
        self.assertEqual(art.desktop_exec('/path with spaces/steam'), '"/path with spaces/steam"')
        self.assertEqual(art.desktop_exec('/100%/steam'), '"/100%%/steam"')
        self.assertEqual(art.desktop_exec('/a$b/steam'), '"/a\\\\$b/steam"')
        self.assertEqual(art.desktop_string('/a\\b\nc'), '/a\\\\b\\nc')

    def test_windows_artwork_transaction_rolls_back_grid_preferences_and_shortcuts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp).resolve(); profile=root/'profile'; config=profile/'config'
            config.mkdir(parents=True)
            local=config/'localconfig.vdf';local.write_text('"UserLocalConfigStore" {}')
            original=local.read_bytes(); pref=root/'flat.ini';pref.write_bytes(b'original layout')
            real=steam.os.replace
            def fail(source,target):
                if str(target).endswith('_logo.png'):
                    raise OSError('fixture disk failure')
                return real(source,target)
            with patch.object(steam.os,'replace',side_effect=fail), self.assertRaises(OSError):
                steam.configure(profile,root,root/'game','new route',['flat'],root/'backups',lambda:None,
                                artwork=ART,extra_updates={pref:b'new layout'})
            self.assertEqual(local.read_bytes(), original)
            self.assertEqual(pref.read_bytes(), b'original layout')
            self.assertFalse((config/'shortcuts.vdf').exists())
            self.assertFalse(list((config/'grid').glob('*.png')))
            self.assertFalse(list((root/'game/d2rloader/pcvr-plus-icons').glob('*.png')))

    def test_all_windows_languages_and_layouts_validate_and_preserve_vr(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp).resolve()
            fixture=setup_native_test.NativeSetupTests()
            app,game,client,saves,profile,values=fixture.fixture(root)
            values.update(connections='flat',mode='full')
            flat=fixture.put(game/'mods/D2R-PCVR/d2rloader/plugins/d2r_pcvr_plus_flat.ini',b'[input]\nflat_layout=WASD\ncustom=keep\n')
            vr=fixture.put(game/'mods/D2R-PCVR/d2rloader/plugins/d2r_pcvr.ini', b'original VR controls and panels')
            loader=fixture.put(game/'d2rloader/config/d2rloader.toml',b'text_locale="enUS"\naudio_locale="frFR"\ncustom=true\n')
            with patch.object(setup_windows,'verify_release',return_value={'sha256':{}}):
                for language in prefs.LANGUAGES:
                    for layout in ('WASD','ZQSD'):
                        selection=setup_native.validate_choices(dict(values,layout=layout,text_locale=language,audio_locale=''),app)
                        updates=prefs.preference_updates(game,selection['preferences'])
                        self.assertIn(('flat_layout='+layout).encode(),updates[flat])
                        self.assertIn(('text_locale='+json.dumps(language)).encode(),updates[loader])
                        self.assertIn(b'audio_locale=""',updates[loader])
                        self.assertNotIn(vr,updates)
                        self.assertIn(b'custom=true',updates[loader])
                with self.assertRaisesRegex(ValueError,'Invalid keyboard'):
                    setup_native.validate_choices(dict(values,layout='invalid'),app)


if __name__ == '__main__':
    unittest.main()
