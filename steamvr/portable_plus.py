"""Portable configuration, launch and reversible DLL update. No installer imports."""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import sys
import uuid
from launch_plus import digest, main as launch
from process_guard import require_game_closed

ROOT = Path(__file__).resolve().parent.parent
DLL = 'd2rl-vrcam.dll'
FLAT = 'd2r_pcvr_plus_flat.ini'


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


def configure():
    settings = {}
    for key, prompt in [('game', 'Existing D2R game folder'), ('saves', 'Actual saves folder containing Settings.json'), ('backups', 'Separate permanent save-backup folder')]:
        settings[key] = str(Path(input(prompt + ': ').strip().strip('"')).resolve())
    if not (Path(settings['game']) / 'D2RLoader.exe').is_file():
        raise ValueError('D2RLoader.exe not found')
    if not (Path(settings['saves']) / 'Settings.json').is_file():
        raise ValueError('Settings.json not found')
    saves, backups = Path(settings['saves']), Path(settings['backups'])
    if saves.is_relative_to(backups) or backups.is_relative_to(saves):
        raise ValueError('Save and backup folders must not overlap')
    settings['store'] = input('Store (steam/battlenet) [steam]: ').strip() or 'steam'
    if settings['store'] not in ('steam', 'battlenet'):
        raise ValueError('Invalid store')
    settings['vdxr'] = input('VR uses VDXR? (y/N): ').lower() == 'y'
    (ROOT / 'portable-settings.json').write_text(json.dumps(settings, indent=2), encoding='utf-8')
    print('Configuration saved. Battle.net requires explicit --experimental-battlenet at launch.')


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('configure', 'update', 'rollback', 'flat', 'vr'))
    parser.add_argument('--game', type=Path)
    args, extra = parser.parse_known_args(arguments)
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
    arguments = ['--mode', args.action, '--game', str(game)]
    for key in ('saves', 'backups', 'store'):
        if key in settings:
            arguments += ['--' + key, settings[key]]
    if args.action == 'vr' and settings.get('vdxr'):
        arguments.append('--vdxr')
    return launch(arguments + extra)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        sys.exit('PCVR Plus: ' + str(error))
