#!/usr/bin/env python3
"""D2R PCVR Plus: separate Flat/VR profiles, native game save location.

The launcher does not inspect, copy, relocate or restore character saves.
"""
import argparse
import configparser
import os
from pathlib import Path
import subprocess
import sys


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
    game = args.game.resolve()
    validate_game(game, args.store, args.experimental_battlenet)
    command, environment = prepare(game, args.mode)
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
