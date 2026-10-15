"""Replay the validated Proton/container route without Steam's launch/reaper chain.

Only executable paths and a small allowlist of compatibility settings are saved.
Never persist session credentials, display sockets or the entire environment.
"""
import json
import os
from pathlib import Path
import subprocess

CONTEXT = 'direct-proton.json'
KEYS = ('STEAM_COMPAT_DATA_PATH', 'STEAM_COMPAT_CLIENT_INSTALL_PATH',
        'STEAM_COMPAT_LIBRARY_PATHS', 'STEAM_COMPAT_TOOL_PATHS', 'STEAM_COMPAT_MOUNTS',
        'STEAM_COMPAT_APP_ID', 'SteamAppId', 'SteamGameId')
MODES = {'D2R-PCVR-Plus-Flat': 'flat', 'D2R-PCVR-Plus-SteamVR': 'steamvr', 'D2R-PCVR-Plus-WiVRn': 'wivrn'}


def context(arguments, environment, game):
    from launch_linux import FLAGS
    command = [x for x in arguments if x not in FLAGS and x != '--plus-probe']
    proton = [i for i, x in enumerate(command) if Path(x).name == 'proton' and Path(x).is_file()]
    if len(proton) != 1:
        raise ValueError('Cannot identify a single Proton executable in this launch.')
    at = proton[0]
    # Strip Steam's reaper and IPC wrapper, retaining the runtime container.
    containers = [i for i, x in enumerate(command[:at]) if Path(x).name == '_v2-entry-point' and Path(x).is_file()]
    start = containers[-1] if containers else at
    command = command[start:]
    if sum(Path(x).name == 'D2R.exe' for x in command) != 1:
        raise ValueError('Expected the original D2R executable in the Proton route.')
    prefix = Path(environment.get('STEAM_COMPAT_DATA_PATH', ''))
    if not prefix.is_absolute() or not (prefix / 'pfx').is_dir():
        raise ValueError('The existing Proton prefix could not be found.')
    return {'version': 1, 'game': str(Path(game).resolve()), 'command': command,
            'environment': {k: environment[k] for k in KEYS if k in environment}}


def remember(root, arguments, environment, game):
    record = context(arguments, environment, game)
    path = Path(root) / CONTEXT
    data = json.dumps(record, indent=2) + '\n'
    if path.exists() and path.read_text() == data:
        return
    temporary = path.with_suffix('.tmp')
    temporary.write_text(data)
    os.replace(temporary, path)


def launch(root, mode):
    from launch_linux import route, runtime_manifest
    from process_guard import require_game_closed
    if mode not in ('flat', 'steamvr', 'wivrn'):
        raise ValueError('Unknown direct launch mode.')
    root = Path(root)
    path = root / CONTEXT
    if not path.is_file():
        raise ValueError('One-time setup: launch a Plus mode from Steam once with this version installed. '
                         'This records your working Proton/runtime and existing prefix for direct launches.')
    saved = json.loads(path.read_text())
    config = json.loads((root / 'linux-launch.json').read_text())
    if saved.get('version') != 1 or Path(saved['game']).resolve() != Path(config['game']).resolve():
        raise ValueError('Direct launch context belongs to another game. Launch Plus from Steam once again.')
    command = saved['command']
    prefix = Path(saved['environment']['STEAM_COMPAT_DATA_PATH'])
    if not (prefix / 'pfx').is_dir() or not command or not Path(command[0]).is_file():
        raise ValueError('Proton/runtime or prefix moved. Launch Plus from Steam once to refresh the direct launcher.')
    if any(not Path(x).is_file() for x in command if Path(x).name == 'proton'):
        raise ValueError('Proton was removed. Launch Plus from Steam once to refresh the direct launcher.')
    env = dict(os.environ)
    env.update({k: v for k, v in saved['environment'].items() if k in KEYS})
    # Direct calls do not inherit Steam's app-id injection.
    env['SteamAppId'] = env['SteamGameId'] = env['STEAM_COMPAT_APP_ID'] = '2536520'
    command, env, info = route(command + ['--plus-' + mode], config, env)
    if mode == 'wivrn' and not any(Path(x).name == '_v2-entry-point' for x in command):
        env['XR_RUNTIME_JSON'] = runtime_manifest(config['wivrn_runtime'], mode)
    require_game_closed()
    logs = Path(info['log_dir']); logs.mkdir(parents=True, exist_ok=True)
    with (logs / 'plus-direct-launch.log').open('a') as log:
        log.write('Starting direct ' + mode + '\n'); log.flush()
        return subprocess.run(command, cwd=info['game'], env=env, stdout=log, stderr=log).returncode
