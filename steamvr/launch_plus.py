#!/usr/bin/env python3
"""D2R PCVR Plus: pre-launch verified snapshots, separate Flat/VR profiles.

Never rewrites, moves, restores or deletes a character. Snapshot failures abort.
Launch this wrapper for BOTH modes to get backups; legacy shortcuts bypass it.
"""
import argparse
import configparser
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_tree(root):
    """Do not follow redirected folders/files within the selected save root."""
    result = []
    def failed(error):
        raise error
    for directory, folders, files in os.walk(root, followlinks=False, onerror=failed):
        for name in folders + files:
            path = Path(directory) / name
            if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
                raise ValueError(f'Redirected entry in save tree: {path}')
        result.extend(Path(directory) / name for name in files)
    return sorted(result)


def snapshot(saves, backups, mode):
    saves, backups = saves.resolve(), backups.resolve()
    if not saves.is_dir() or not (saves / 'Settings.json').is_file():
        raise ValueError('Select the actual save folder containing Settings.json; launch stock once first.')
    if backups == saves or backups.is_relative_to(saves) or saves.is_relative_to(backups):
        raise ValueError('Save and backup folders must be separate, non-nested folders.')
    files = safe_tree(saves)
    backups.mkdir(parents=True, exist_ok=True)
    token = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + mode + '-' + uuid.uuid4().hex[:12]
    temporary = backups / ('.incomplete-' + token)
    destination = backups / token
    temporary.mkdir()
    try:
        manifest = {}
        for source in files:
            relative = source.relative_to(saves)
            target = temporary / 'files' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            before = digest(source)
            shutil.copy2(source, target)
            if digest(source) != before or digest(target) != before:
                raise ValueError(f'Save changed or copy failed: {relative}; launch aborted.')
            manifest[relative.as_posix()] = before
        if safe_tree(saves) != files or any(digest(saves / name) != value for name, value in manifest.items()):
            raise ValueError('Save tree changed during backup; launch aborted.')
        (temporary / 'manifest.json').write_text(json.dumps({'mode': mode, 'sha256': manifest}, indent=2), encoding='utf-8')
        temporary.rename(destination)
    except BaseException:
        shutil.rmtree(temporary)  # Only this attempt's new snapshot, never the originals.
        raise
    return destination


@contextmanager
def launch_lock(saves):
    # A single lock shared by both modes, even if they use different backup paths.
    path = saves.parent / ('.' + saves.name + '-pcvr-plus.lock')
    try:
        stream = path.open('x', encoding='utf-8')
    except FileExistsError as error:
        raise ValueError(f'Another Plus launcher is active, or a previous launch crashed. Close the game and verify before removing {path}') from error
    try:
        with stream:
            stream.write(str(os.getpid()))
        yield
    finally:
        path.unlink()


def prepare(game, mode):
    plugin = game / 'd2rloader/plugins'
    for path in (game / 'D2R.exe', game / 'D2RLoader.exe', plugin / 'd2rl-vrcam.dll'):
        if not path.is_file():
            raise ValueError(f'Missing installation file: {path}')
    profile = plugin / ('d2r_pcvr_plus_flat.ini' if mode == 'flat' else 'd2r_vr.ini')
    config = configparser.ConfigParser(interpolation=None, strict=False, allow_no_value=True)
    if not profile.is_file():
        raise ValueError(f'Missing profile: {profile}')
    # Upstream INI permits global keys before the first section (Win32 API).
    config.read_string('[__global__]\n' + profile.read_text(encoding='utf-8-sig'))
    if mode == 'vr' and config.getint('mode', 'platform', fallback=1) != 1:
        raise ValueError('VR profile must have [mode] platform=1.')
    environment = os.environ.copy()
    environment['D2R_PCVR_PLUS_MODE'] = mode
    return [str(game / 'D2RLoader.exe')], environment


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--saves', required=True, type=Path, help='Actual save directory, including Settings.json')
    parser.add_argument('--backups', required=True, type=Path, help='Separate permanent backup directory')
    parser.add_argument('--mode', required=True, choices=('flat', 'vr'))
    parser.add_argument('--store', choices=('steam', 'battlenet'), default='steam')
    parser.add_argument('--experimental-battlenet', action='store_true')
    runtime = parser.add_mutually_exclusive_group()
    runtime.add_argument('--vdxr', action='store_true', help='VR only')
    runtime.add_argument('--openxr-runtime', type=Path, help='Explicit OpenXR manifest; VR only')
    args = parser.parse_args(arguments)
    if sys.platform != 'win32':
        parser.error('This launcher targets Windows. Proton is not validated.')
    if (args.vdxr or args.openxr_runtime) and args.mode != 'vr':
        parser.error('OpenXR runtime options are VR only')
    if args.openxr_runtime and not args.openxr_runtime.is_file():
        parser.error('OpenXR runtime manifest not found: ' + str(args.openxr_runtime))
    from game_install import validate_game
    from process_guard import require_game_closed
    from launch_windows import main as launch_vr
    game, saves = args.game.resolve(), args.saves.resolve()
    validate_game(game, args.store, args.experimental_battlenet)
    command, environment = prepare(game, args.mode)
    with launch_lock(saves):
        require_game_closed()
        saved = snapshot(saves, args.backups, args.mode)
        print(f'Verified pre-launch backup: {saved}', flush=True)
        require_game_closed()
        if args.mode == 'vr':
            # Preserve PCVR's Steam desktop handoff and runtime selection.
            arguments = ['--frame-vr']
            if args.vdxr:
                arguments.append('--frame-vdxr')
            elif args.openxr_runtime:
                arguments.append('--frame-openxr-runtime=' + str(args.openxr_runtime.resolve()))
            if args.store == 'battlenet':
                arguments.extend(['--battlenet-game=' + str(game), '--experimental-battlenet'])
            else:
                arguments.append(str(game / 'D2R.exe'))
            return launch_vr(arguments)
        if args.store == 'steam':
            environment['SteamAppId'] = environment['SteamGameId'] = '2536520'
        else:
            for key in list(environment):
                if key.lower() in ('steamappid', 'steamgameid'):
                    del environment[key]
        # Flat never calls the runtime selector or starts a VR/BodyWalk process.
        return subprocess.run(command, cwd=game, env=environment).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f'PCVR Plus: {error}') from error
