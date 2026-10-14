"""Portable configuration, launch and reversible DLL update. No installer imports."""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import uuid
from launch_plus import main as launch
from process_guard import require_game_closed

ROOT = Path(__file__).resolve().parent.parent
DLL = 'd2rl-vrcam.dll'
FLAT = 'd2r_pcvr_plus_flat.ini'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def atomic_copy(source, target):
    temporary = target.with_name(target.name + '.' + uuid.uuid4().hex + '.tmp')
    try:
        shutil.copy2(source, temporary)
        if digest(source) != digest(temporary):
            raise ValueError('Copy verification failed')
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)


def checked(game, relative):
    path = game / relative
    if not path.resolve().is_relative_to(game):
        raise ValueError('Installation path redirects outside the game folder')
    return path


@contextmanager
def update_lock(game):
    lock = checked(game, 'd2rloader/pcvr-plus-update.lock')
    with lock.open('x') as stream:
        stream.write(str(os.getpid()))
    try:
        require_game_closed()
        yield
    finally:
        lock.unlink()


def update(game, package=ROOT):
    game = game.resolve()
    plugin = checked(game, 'd2rloader/plugins')
    for name in ('D2R.exe', 'D2RLoader.exe', 'd2rloader/plugins/' + DLL, 'd2rloader/plugins/d2r_vr.ini'):
        if not checked(game, name).is_file():
            raise ValueError('An existing automatic-setup PCVR installation is required: ' + name)
    hashes = json.loads((package / 'payload-sha256.json').read_text())
    for name in (DLL, FLAT):
        if digest(package / 'payload' / name) != hashes[name]:
            raise ValueError('Damaged package: ' + name)
    with update_lock(game):
        # Refuse a second upgrade until the previous upgrade has been rolled back.
        state = checked(game, 'd2rloader/pcvr-plus-update.json')
        if state.exists():
            raise ValueError('An update is already recorded. Roll it back before updating again.')
        token = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex
        backup = checked(game, 'd2rloader/pcvr-plus-backups/' + token)
        backup.mkdir(parents=True)
        atomic_copy(plugin / DLL, backup / DLL)
        atomic_copy(plugin / 'd2r_vr.ini', backup / 'd2r_vr.ini')
        record = {'backup': token, 'original': digest(backup / DLL), 'installed': hashes[DLL]}
        # Journal BEFORE replacement: interruption can always be recovered by rollback.
        with state.open('x', encoding='utf-8') as stream:
            json.dump(record, stream, indent=2)
        try:
            # Existing Flat settings are never overwritten. Rollback also preserves them.
            if not (plugin / FLAT).exists():
                atomic_copy(package / 'payload' / FLAT, plugin / FLAT)
            require_game_closed()
            atomic_copy(package / 'payload' / DLL, plugin / DLL)
        except BaseException:
            atomic_copy(backup / DLL, plugin / DLL)
            state.unlink()
            raise
        print('Updated DLL; original retained in ' + str(backup))


def rollback(game):
    game = game.resolve()
    with update_lock(game):
        state = checked(game, 'd2rloader/pcvr-plus-update.json')
        record = json.loads(state.read_text(encoding='utf-8'))
        token = record['backup']
        if not isinstance(token, str) or not token or any(c not in '0123456789TZ-abcdef' for c in token):
            raise ValueError('Invalid backup identifier')
        source = checked(game, 'd2rloader/pcvr-plus-backups/' + token + '/' + DLL)
        target = checked(game, 'd2rloader/plugins/' + DLL)
        if digest(source) != record['original']:
            raise ValueError('Original DLL backup was modified; rollback refused')
        if digest(target) not in (record['installed'], record['original']):
            raise ValueError('Installed DLL was changed by another tool; rollback refused')
        require_game_closed()
        atomic_copy(source, target)
        state.unlink()
        print('Original DLL restored. Profiles, saves and backup files retained.')


LAUNCHERS = {
    'steamvr': 'D2R-PCVR-PLUS-SteamVR.cmd',
    'vdxr': 'D2R-PCVR-PLUS-VDXR.cmd',
    'flat': 'D2R-PCVR-PLUS-Flat.cmd',
}


def launcher_text(action):
    return ('@echo off\nsetlocal\n'
            '"%~dp0deps\\python\\python.exe" "%~dp0steamvr\\portable_plus.py" ' + action + ' %*\n'
            'set "result=%errorlevel%"\nif not "%result%"=="0" pause\nexit /b %result%\n')


def write_launchers(settings, root):
    selected = settings['launch_modes']
    # Only replace/remove files that this configurator owns, never custom CMDs.
    for action, name in LAUNCHERS.items():
        path = root / name
        if path.exists() and path.read_text(encoding='ascii') != launcher_text(action):
            raise ValueError('Custom launcher already exists; rename it first: ' + str(path))
    for action, name in LAUNCHERS.items():
        path = root / name
        if action in selected:
            path.write_text(launcher_text(action), encoding='ascii')
        else:
            path.unlink(missing_ok=True)


def configure():
    source = ROOT / 'portable-settings.json'
    previous = json.loads(source.read_text(encoding='utf-8')) if source.exists() else {}
    settings = {}
    default_game = previous.get('game', '')
    selected_game = input('Existing D2R game folder [' + default_game + ']: ').strip().strip('"')
    settings['game'] = str(Path(selected_game or default_game).resolve())
    if not (Path(settings['game']) / 'D2RLoader.exe').is_file():
        raise ValueError('D2RLoader.exe not found')
    settings['store'] = input('Store (steam/battlenet) [steam]: ').strip() or 'steam'
    if settings['store'] not in ('steam', 'battlenet'):
        raise ValueError('Invalid store')
    print('1. Steam Link / SteamVR\n2. Virtual Desktop / VDXR\n3. Flat Screen\n4. All (create three CMD launchers)')
    choice = input('Launch modes (1/2/3/4 or steamvr/vdxr/flat/all) [all]: ').strip().lower() or 'all'
    choice = {'1':'steamvr', '2':'vdxr', '3':'flat', '4':'all'}.get(choice, choice)
    if choice not in ('steamvr', 'vdxr', 'flat', 'all'):
        raise ValueError('Invalid launch mode')
    settings['launch_modes'] = list(LAUNCHERS) if choice == 'all' else [choice]
    # Legacy generic VR launcher defaults to SteamVR when All is selected.
    settings['vr_runtime'] = 'steamvr' if choice == 'all' else choice
    settings['vdxr'] = choice == 'vdxr'
    if 'steamvr' in settings['launch_modes']:
        default = Path(previous['openxr_runtime']) if previous.get('openxr_runtime') else Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Steam/steamapps/common/SteamVR/steamxr_win64.json'
        selected = input('SteamVR manifest [' + str(default) + ']: ').strip().strip('"')
        manifest = (Path(selected) if selected else default).resolve()
        if not manifest.is_file():
            raise ValueError('SteamVR manifest not found; select the actual SteamVR installation: ' + str(manifest))
        settings['openxr_runtime'] = str(manifest)
    write_launchers(settings, ROOT)
    (ROOT / 'portable-settings.json').write_text(json.dumps(settings, indent=2), encoding='utf-8')
    print('Created: ' + ', '.join(LAUNCHERS[mode] for mode in settings['launch_modes']))
    print('Configuration saved. Battle.net requires explicit --experimental-battlenet at launch.')


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('configure', 'update', 'rollback', 'flat', 'vr', 'steamvr', 'vdxr'))
    parser.add_argument('--game', type=Path)
    runtime = parser.add_mutually_exclusive_group()
    runtime.add_argument('--openxr-runtime', type=Path)
    runtime.add_argument('--vdxr', action='store_true')
    runtime.add_argument('--system-openxr', action='store_true', help='Use the system/environment runtime')
    args, extra = parser.parse_known_args(arguments)
    override = args.openxr_runtime is not None or args.vdxr or args.system_openxr
    if override and args.action != 'vr':
        parser.error('OpenXR runtime options are VR only')
    if args.action == 'configure':
        configure()
        return 0
    settings_file = ROOT / 'portable-settings.json'
    settings = json.loads(settings_file.read_text(encoding='utf-8')) if settings_file.exists() else {}
    game = args.game or (Path(settings['game']) if settings.get('game') else None)
    if game is None:
        parser.error('Run Configure.cmd first, or supply --game PATH')
    if args.action in ('update', 'rollback'):
        if extra:
            parser.error('Unrecognized arguments: ' + ' '.join(extra))
        (update if args.action == 'update' else rollback)(game)
        return 0
    if args.action in LAUNCHERS and settings.get('launch_modes') and args.action not in settings['launch_modes']:
        parser.error('This mode is not configured. Run Configure.cmd and select it or All.')
    mode = 'vr' if args.action in ('steamvr', 'vdxr') else args.action
    arguments = ['--mode', mode, '--game', str(game)]
    for key in ('store',):
        if key in settings:
            arguments += ['--' + key, settings[key]]
    if mode == 'vr':
        if args.action == 'steamvr':
            if not settings.get('openxr_runtime'):
                raise ValueError('SteamVR manifest missing. Run Configure.cmd and choose SteamVR or All.')
            arguments += ['--openxr-runtime', settings['openxr_runtime']]
        elif args.action == 'vdxr':
            arguments.append('--vdxr')
        elif args.openxr_runtime is not None:
            arguments += ['--openxr-runtime', str(args.openxr_runtime)]
        elif args.vdxr:
            arguments.append('--vdxr')
        elif not args.system_openxr:
            if settings.get('vdxr') or settings.get('vr_runtime') == 'vdxr':
                arguments.append('--vdxr')
            elif settings.get('openxr_runtime'):
                arguments += ['--openxr-runtime', settings['openxr_runtime']]
            elif settings.get('vr_runtime') != 'system':
                raise ValueError('VR runtime is not explicitly configured. Run Configure.cmd and choose SteamVR, VDXR or All; or pass --openxr-runtime PATH, --vdxr or --system-openxr.')
        print('VR runtime: ' + ('VDXR' if '--vdxr' in arguments else
              arguments[arguments.index('--openxr-runtime') + 1] if '--openxr-runtime' in arguments else
              'system/environment default'), flush=True)
    return launch(arguments + extra)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        sys.exit('PCVR Plus: ' + str(error))
