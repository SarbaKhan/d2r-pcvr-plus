#!/usr/bin/env python3
"""Experimental Steam/Proton routing. Never starts Wine or creates a prefix itself."""
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

from launch_plus import prepare

ROOT = Path(__file__).resolve().parent.parent
FLAGS = {'--plus-flat': 'flat', '--plus-steamvr': 'steamvr', '--plus-wivrn': 'wivrn'}


def runtime_manifest(path, mode):
    path = Path(path).expanduser().resolve()
    data = json.loads(path.read_text())
    library = data['runtime']['library_path']
    target = Path(library)
    if not target.is_absolute():
        target = path.parent / target
    if not target.is_file():
        raise ValueError(f'Runtime library missing: {target}')
    identity = (str(path) + ' ' + str(target)).lower()
    markers = ('steamxr', 'steamvr') if mode == 'steamvr' else ('wivrn', 'monado')
    if not any(marker in identity for marker in markers):
        raise ValueError(f'Manifest does not identify the selected {mode} runtime: {path}')
    return str(path)


def route(arguments, config, environment):
    command = list(arguments)
    modes = [FLAGS[x] for x in command if x in FLAGS]
    probe = '--plus-probe' in command
    if not modes:
        if probe:
            raise ValueError('Probe requires --plus-steamvr or --plus-wivrn')
        return command, dict(environment), None
    if len(modes) != 1:
        raise ValueError('Select exactly one Plus mode')
    mode = modes[0]
    if probe and mode == 'flat':
        raise ValueError('Flat does not use an OpenXR probe')
    command = [x for x in command if x not in FLAGS and x != '--plus-probe']
    matches = [i for i, x in enumerate(command) if Path(x).name == 'D2R.exe']
    if len(matches) != 1:
        raise ValueError('Expected exactly one Steam D2R.exe argument')
    index = matches[0]
    game = Path(command[index]).resolve().parent
    if game != Path(config['game']).resolve():
        raise ValueError('Steam launched a different game directory than configured')
    prefix = environment.get('STEAM_COMPAT_DATA_PATH')
    if not prefix:
        raise ValueError('Steam Proton context missing; launch the purchased game through Steam')
    prepare(game, 'flat' if mode == 'flat' else 'vr')
    env = dict(environment)
    env['D2R_PCVR_PLUS_MODE'] = 'flat' if mode == 'flat' else 'vr'
    env['STEAM_COMPAT_INSTALL_PATH'] = str(game)
    library = game.parent.parent
    if library.name != 'steamapps':
        raise ValueError('Game must be inside a Steam library steamapps/common directory')
    env['STEAM_COMPAT_LIBRARY_PATHS'] = str(library) + (':' + env['STEAM_COMPAT_LIBRARY_PATHS'] if env.get('STEAM_COMPAT_LIBRARY_PATHS') else '')
    log_dir = game / 'd2rloader/logs'
    env['PROTON_LOG'] = '1'
    env['PROTON_LOG_DIR'] = str(log_dir)
    env['WINEDEBUG'] = '-all,+timestamp,+pid,+tid,err+all'
    # Use native Wayland for Flat mouse capture only; VR uses XWayland.
    if mode == 'flat' and environment.get('XDG_SESSION_TYPE', '').lower() == 'wayland':
        env['PROTON_ENABLE_WAYLAND'] = '1'
    else:
        env.pop('PROTON_ENABLE_WAYLAND', None)
    if mode == 'flat':
        for key in ('XR_RUNTIME_JSON', 'PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES'):
            env.pop(key, None)
    else:
        selected = config.get(mode + '_runtime')
        if not selected:
            raise ValueError(f'Configure --{mode}-runtime with the Linux OpenXR JSON first')
        manifest = runtime_manifest(selected, mode)
        if mode == 'wivrn':
            # WiVRn selects the system runtime when the headset connects.
            # A host /usr manifest override breaks Steam's container import.
            env.pop('XR_RUNTIME_JSON', None)
        else:
            env['XR_RUNTIME_JSON'] = manifest
        env['PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES'] = '1'
    command[index] = str(game / 'D2RLoader.exe')
    if probe:
        executable = ROOT / 'bin/openxr-probe.exe'
        if not executable.is_file():
            raise ValueError(f'Probe missing: {executable}')
        command[index] = str(executable)
        output = str(log_dir / f'plus-{mode}-probe.txt')
        command.insert(index + 1, 'Z:' + output.replace('/', '\\'))
    return command, env, {'mode': mode, 'probe': probe, 'game': str(game), 'prefix': prefix,
                          'runtime': env.get('XR_RUNTIME_JSON'), 'log_dir': str(log_dir)}


def main(arguments=None):
    arguments = sys.argv[1:] if arguments is None else arguments
    config_path = ROOT / 'linux-launch.json'
    # Stock launch must work even before configuration or after mod removal.
    config = json.loads(config_path.read_text()) if any(x in FLAGS for x in arguments) else {}
    command, env, info = route(arguments, config, os.environ)
    if info is None:
        os.execvpe(command[0], command, env)
    # Capture only a real Steam-provided route. Direct launches call route()
    # directly, so cannot overwrite this template with already rewritten paths.
    try:
        from direct_linux import remember
        remember(ROOT, arguments, os.environ, info['game'])
    except (OSError, ValueError) as error:
        print('Direct launcher context was not saved: ' + str(error), file=sys.stderr)
    logs = Path(info['log_dir'])
    logs.mkdir(parents=True, exist_ok=True)
    with (logs / 'plus-linux-launch.jsonl').open('a') as log:
        log.write(json.dumps({'time': datetime.now(timezone.utc).isoformat(), **info}) + '\n')
        log.flush()
        # Do not kill Proton helpers during this validation stage. Observe normal exit.
        result = subprocess.run(command, env=env, cwd=info['game']).returncode
        log.write(json.dumps({'exit_code': result}) + '\n')
    return result


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f'PCVR Plus Linux: {error}', file=sys.stderr)
        # Also leave startup failures next to the test package, before game logs exist.
        with (ROOT / 'linux-launch-error.txt').open('a') as log:
            log.write(f'{datetime.now(timezone.utc).isoformat()} {error}\n')
        raise SystemExit(1)
