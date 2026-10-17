"""Linux setup backend. No GUI dependency, no Wine prefix or character writes."""
import copy
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import uuid
import zlib

import steam_config as vdf
from launch_linux import runtime_manifest
from process_guard import require_game_closed

APP = 'D2R-PCVR-Plus'
NAMES = {'flat': 'D2R PCVR Plus - Flat Screen', 'steamvr': 'D2R PCVR Plus - SteamVR',
         'wivrn': 'D2R PCVR Plus - WiVRn'}
from plus_preferences import LANGUAGES, preferences, replace_value, preference_updates
import plus_artwork

def steam_profiles(steam):
    return sorted(p.parent.parent for p in (Path(steam) / 'userdata').glob('*/config/localconfig.vdf')
                  if p.parent.parent.name.isdecimal())


def discover(steam=None):
    home = Path.home()
    roots = [Path(steam)] if steam else [home / '.local/share/Steam', home / '.steam/steam', home / '.steam/root']
    root = next((p.resolve() for p in roots if (p / 'steamapps').is_dir()), roots[0])
    libraries = [root]
    source = root / 'steamapps/libraryfolders.vdf'
    if source.is_file():
        tree = vdf.text_value(vdf.parse_text(source.read_text()), 'libraryfolders') or []
        for item in tree:
            if isinstance(item.value, list):
                path = vdf.text_value(item.value, 'path')
                if path:
                    libraries.append(Path(path))
    games, runtimes = [], []
    for library in dict.fromkeys(libraries):
        manifest = library / 'steamapps/appmanifest_2536520.acf'
        folder = 'Diablo II Resurrected'
        if manifest.is_file():
            folder = vdf.text_value(vdf.parse_text(manifest.read_text()), 'AppState', 'installdir') or folder
        game = library / 'steamapps/common' / folder
        if (game / 'D2R.exe').is_file():
            games.append(str(game))
        runtime = library / 'steamapps/common/SteamVR/steamxr_linux64.json'
        if runtime.is_file():
            runtimes.append(str(runtime))
    return {'steam_root': str(root), 'steam': shutil.which('steam') or '/usr/bin/steam',
            'games': games, 'steamvr_runtime': next(iter(runtimes), ''),
            'wivrn_runtime': next((str(p) for p in [Path('/usr/share/openxr/1/openxr_wivrn.json'),
                home / '.config/openxr/1/active_runtime.json'] if p.is_file()), '')}


def default_saves(game):
    return Path(game).parent.parent / 'compatdata/2536520/pfx/drive_c/users/steamuser/Saved Games/Diablo II Resurrected'


def require_closed():
    require_game_closed()
    for path in Path('/proc').glob('[0-9]*/comm'):
        try:
            if path.read_text().strip().lower() in ('steam', 'steamwebhelper'):
                raise ValueError('Exit Steam completely (Steam > Exit), then try again. Setup never closes it for you.')
        except (FileNotFoundError, PermissionError):
            continue


def current_options(profile):
    text = (Path(profile) / 'config/localconfig.vdf').read_text(encoding='utf-8-sig')
    return vdf.text_value(vdf.parse_text(text), 'UserLocalConfigStore', 'Software', 'Valve',
                          'Steam', 'apps', vdf.APP_ID, 'LaunchOptions') or ''




def shortcuts(original, steam, game, modes):
    root = vdf.decode_binary(original) if original is not None else {'shortcuts': {}}
    if original is not None and vdf.encode_binary(root) != original:
        raise ValueError('Steam shortcuts cannot be safely round-tripped.')
    entries = root.get('shortcuts')
    if not isinstance(entries, dict) or any(not k.isdecimal() or not isinstance(v, dict) for k, v in entries.items()):
        raise ValueError('Unexpected Steam shortcut structure.')
    # Only own named entries for this game are updated; unrelated entries survive.
    for mode in modes:
        name = NAMES[mode]
        matches = [k for k, v in entries.items() if v.get('appname', v.get('AppName')) == name]
        if len(matches) > 1:
            raise ValueError('Remove duplicate Steam shortcuts named ' + name)
        if matches and not plus_artwork.belongs(entries[matches[0]], game):
            raise ValueError('A shortcut with the same name belongs to another installation: ' + name)
        key = matches[0] if matches else str(max(map(int, entries), default=-1) + 1)
        exe = '"' + str(steam) + '"'
        item = entries.setdefault(key, {})
        item.pop('AppName', None)
        item.update({'appid': zlib.crc32((exe + name).encode()) | 0x80000000, 'appname': name,
                     'Exe': exe, 'StartDir': '"' + str(Path(steam).parent) + '"',
                     'icon': str(Path(game) / 'D2R.exe'), 'D2RPCVRPlusGame': str(Path(game).resolve()), 'ShortcutPath': '',
                     'LaunchOptions': '-applaunch 2536520 --plus-' + mode,
                     'IsHidden': 0, 'AllowDesktopConfig': 0, 'AllowOverlay': 0,
                     'OpenVR': int(mode == 'steamvr'), 'Devkit': 0, 'DevkitGameID': '',
                     'DevkitOverrideAppID': 0, 'FlatpakAppID': ''})
        item.setdefault('LastPlayTime', 0)
        item.setdefault('tags', {'0': 'D2R PCVR Plus'})
    return vdf.encode_binary(root)


def validate(values):
    c = copy.deepcopy(values)
    for key in ('game', 'steam_root', 'profile', 'saves'):
        c[key] = str(Path(c[key]).expanduser().resolve())
    c['steam'] = str(Path(c['steam']).expanduser().absolute())
    if not (Path(c['game']) / 'D2R.exe').is_file():
        raise ValueError('Select the game folder containing D2R.exe.')
    if Path(c['game']).parent.name != 'common' or Path(c['game']).parent.parent.name != 'steamapps':
        raise ValueError('This release supports D2R in a native Steam library only.')
    if not Path(c['steam']).is_file() or not os.access(c['steam'], os.X_OK):
        raise ValueError('Select the native Linux Steam executable (usually /usr/bin/steam).')
    if Path(c['profile']) not in steam_profiles(c['steam_root']):
        raise ValueError('Select a Steam account from the chosen Steam data folder.')
    if not (Path(c['saves']) / 'Settings.json').is_file():
        raise ValueError('Launch the unmodded game once, or select its existing Saved Games folder (Settings.json).')
    if not c['modes'] or not set(c['modes']).issubset(NAMES):
        raise ValueError('Select at least one launch mode.')
    if c['layout'] not in ('WASD', 'ZQSD') or any(c[k] not in LANGUAGES for k in ('text_locale', 'audio_locale')):
        raise ValueError('Invalid keyboard or language selection.')
    for mode in ('steamvr', 'wivrn'):
        if mode in c['modes']:
            if not c.get(mode + '_runtime'):
                raise ValueError('Select the ' + mode + ' Linux OpenXR manifest.')
            c[mode + '_runtime'] = runtime_manifest(c[mode + '_runtime'], mode)
    destination = Path(c['game']) / APP
    if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
        raise ValueError('The D2R-PCVR-Plus installation path must be a real directory, not a symlink or file.')
    if c.get('destination') and Path(c['destination']).expanduser().absolute() != destination:
        raise ValueError('Setup installs automatically in the game folder under D2R-PCVR-Plus. Run setup again.')
    c['destination'] = str(destination)
    if destination.exists() and any(destination.iterdir()) and not (destination / 'plus-linux-managed.json').is_file():
        raise ValueError('The destination is not empty and is not a managed Plus installation.')
    return c


def atomic_write(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as f:
        f.write(payload)
        temporary = Path(f.name)
    try:
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def commit_files(updates, backups):
    """Optimistic concurrency and rollback for preferences and Steam configuration."""
    originals = {p: p.read_bytes() if p.exists() else None for p in updates}
    backups.mkdir(parents=True, exist_ok=False)
    manifest = []
    for i, (p, data) in enumerate(originals.items()):
        manifest.append({'path': str(p), 'backup': str(i) if data is not None else None})
        if data is not None:
            (backups / str(i)).write_bytes(data)
    (backups / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    done = []
    try:
        require_closed()
        for p, data in updates.items():
            if (p.read_bytes() if p.exists() else None) != originals[p]:
                raise ValueError('Configuration changed during setup: ' + str(p))
            atomic_write(p, data)
            done.append(p)
    except BaseException:
        for p in reversed(done):
            if originals[p] is None:
                p.unlink(missing_ok=True)
            else:
                atomic_write(p, originals[p])
        raise


def install_choices(choices, source_bundle, payload, runner):
    c = validate(choices)
    require_closed()
    profile = Path(c['profile'])
    if current_options(profile) != c['reviewed_options']:
        raise ValueError('D2R launch options changed since review. Review them again.')
    destination = Path(c['destination'])
    source_bundle = Path(source_bundle).resolve()
    if destination == source_bundle or source_bundle.is_relative_to(destination) or destination.is_relative_to(source_bundle):
        raise ValueError('Extract the download outside the game folder and run that copy of setup. The permanent installation is managed automatically.')
    # Stage and promote the complete Python/Qt bundle. Keep the old installation
    # available if a later dependency download or game installation fails.
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.plus-stage-', dir=destination.parent))
    old = destination.with_name(destination.name + '.previous-' + uuid.uuid4().hex[:8])
    promoted = False
    try:
        shutil.copytree(source_bundle, stage, dirs_exist_ok=True)
        # Keep historical snapshot paths valid across updates.
        if destination.exists():
            for folder in ('installation-backups', 'configuration-backups', 'deps'):
                if (destination / folder).is_dir():
                    shutil.copytree(destination / folder, stage / folder, dirs_exist_ok=True)
        (stage / 'plus-linux-managed.json').write_text(json.dumps({'version': '0.2.3', 'game': c['game']}))
        if destination.exists():
            destination.rename(old)
        stage.rename(destination)
        promoted = True
        installed_payload = destination / Path(payload).relative_to(source_bundle)
        executable = destination / Path(runner).relative_to(source_bundle)
        config = {k: c[k] for k in ('game', 'steam', 'saves', 'steamvr_runtime', 'wivrn_runtime') if c.get(k)}
        (installed_payload / 'linux-launch.json').write_text(json.dumps(config, indent=2) + '\n')
        loader_config = Path(c['game']) / 'd2rloader/config/d2rloader.toml'
        previous_loader = loader_config.read_text(encoding='utf-8-sig') if loader_config.is_file() else None
        print('Installing verified mod dependencies and profiles...', flush=True)
        subprocess.run([str(executable), '--install-game', '--game', c['game'], '--plugin',
                        str(installed_payload / 'bin/d2rl-pcvr.dll'), '--deps', str(destination / 'deps'),
                        '--backup-dir', str(destination / 'installation-backups'), '--saves', c['saves'],
                        '--download-deps', '--preserve-settings', '--store', 'steam'], check=True)
        updates = preference_updates(c['game'], c, previous_loader)
        local = profile / 'config/localconfig.vdf'
        shortcut_file = profile / 'config/shortcuts.vdf'
        options = shlex.quote(str(executable)) + ' --route %command%'
        updates[local] = vdf.set_launch_options(local.read_text(), options).encode()
        updates[shortcut_file] = shortcuts(shortcut_file.read_bytes() if shortcut_file.exists() else None,
                                           c['steam'], c['game'], c['modes'])
        updates[shortcut_file], art_updates = plus_artwork.steam_updates(
            updates[shortcut_file], profile, c['game'], c['modes'], installed_payload / 'artwork')
        updates.update(art_updates)
        updates.update(plus_artwork.desktop_updates(c['steam'], c['game'], c['modes'], installed_payload / 'artwork'))
        if current_options(profile) != c['reviewed_options']:
            raise ValueError('D2R launch options changed during installation. Run setup again.')
        commit_files(updates, destination / ('configuration-backups/' + uuid.uuid4().hex))
        print('Setup complete. Start Steam and use the selected Plus shortcuts in Steam or the application menu. The original D2R entry remains unmodded.', flush=True)
        print('Characters and the existing Proton prefix remain in their original locations.', flush=True)
        if old.exists():
            print('Previous setup files retained at: ' + str(old), flush=True)
    except BaseException:
        # Preserve failed installation files for diagnostics; restore old launcher
        # at its exact original path so existing Steam options remain usable.
        if promoted and old.exists():
            failed = destination.with_name(destination.name + '.failed-' + uuid.uuid4().hex[:8])
            destination.rename(failed)
            old.rename(destination)
        raise
    finally:
        if stage.exists():
            shutil.rmtree(stage)
