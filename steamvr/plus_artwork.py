"""Plan artwork and desktop entries; callers commit all files transactionally."""
import hashlib
import json
import os
from pathlib import Path

NAMES = {'flat': 'D2R PCVR Plus - Flat Screen', 'steamvr': 'D2R PCVR Plus - SteamVR',
         'wivrn': 'D2R PCVR Plus - WiVRn', 'vdxr': 'D2R PCVR Plus - Virtual Desktop'}
FOLDERS = {'flat': 'flat', 'steamvr': 'steamvr', 'wivrn': 'wivrn', 'vdxr': 'virtual-desktop'}
GRID = {'cover': 'p.png', 'wide-cover': '.png', 'background': '_hero.png', 'logo': '_logo.png'}


def belongs(entry, game):
    marker = entry.get('D2RPCVRPlusGame')
    if marker is not None:
        return Path(marker).resolve() == Path(game).resolve()
    icon = Path(entry.get('icon', '')).resolve()
    return icon == (Path(game) / 'D2R.exe').resolve() or icon in {
        (Path(game) / 'd2rloader/pcvr-plus-icons' / (mode + '.png')).resolve() for mode in NAMES}


def assets(directory, modes):
    directory = Path(directory)
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    hashes = {a['path']: a['sha256'] for a in manifest['assets']}
    result = {}
    for mode in modes:
        result[mode] = {}
        for kind in ('icon', *GRID):
            rel = FOLDERS[mode] + '/' + kind + '.png'
            path = directory / rel
            data = path.read_bytes()
            if hashlib.sha256(data).hexdigest() != hashes.get(rel):
                raise ValueError('Artwork integrity check failed: ' + rel)
            result[mode][kind] = (path, data)
    return result


def steam_updates(payload, profile, game, modes, directory):
    import steam_config as vdf
    available = assets(directory, modes)
    tree = vdf.decode_binary(payload)
    updates = {}
    for mode in modes:
        matches = [e for e in tree['shortcuts'].values()
                   if e.get('appname') == NAMES[mode] and belongs(e, game)]
        if len(matches) != 1:
            raise ValueError('Cannot identify the artwork target: ' + NAMES[mode])
        entry = matches[0]
        icon = Path(game) / 'd2rloader/pcvr-plus-icons' / (mode + '.png')
        updates[icon] = available[mode]['icon'][1]
        entry['icon'] = str(icon)
        entry['D2RPCVRPlusGame'] = str(Path(game).resolve())
        appid = entry['appid'] & 0xffffffff
        for kind, suffix in GRID.items():
            updates[Path(profile) / 'config/grid' / (str(appid) + suffix)] = available[mode][kind][1]
    return vdf.encode_binary(tree), updates


def desktop_string(value):
    return str(value).replace('\\', '\\\\').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')


def desktop_exec(value):
    # Exec quoting is applied before the desktop-entry string escape layer.
    text = str(value).replace('%', '%%')
    for char in ('\\', '"', '`', '$'):
        text = text.replace(char, '\\' + char)
    return desktop_string('"' + text + '"')


def desktop_updates(steam, game, modes, directory, applications=None, direct_runner=None):
    available = assets(directory, modes)
    if applications is None:
        home = os.environ.get('XDG_DATA_HOME', '')
        base = Path(home) if home and Path(home).is_absolute() else Path.home() / '.local/share'
        applications = base / 'applications'
    updates = {}
    for mode in modes:
        path = Path(applications) / ('d2r-pcvr-plus-' + mode + '.desktop')
        command = desktop_exec(steam) + ' -applaunch 2536520 --plus-' + mode
        if path.exists():
            text = path.read_text(encoding='utf-8')
            fields = dict(line.split('=', 1) for line in text.splitlines() if '=' in line and not line.startswith('#'))
            owned = fields.get('X-D2RPCVRPlus-Game') == desktop_string(Path(game).resolve())
            # Adopt the documented manually created WiVRn shortcut too.
            manual = (mode == 'wivrn' and fields.get('Name') == NAMES[mode]
                      and fields.get('Exec') in (command, str(steam) + ' -applaunch 2536520 --plus-wivrn')
                      and fields.get('Icon') == 'steam' and fields.get('Categories') == 'Game;X-WiVRn-VR;')
            if not owned and not manual:
                raise ValueError('An unrelated desktop entry already exists: ' + str(path))
        if direct_runner is not None:
            command = desktop_exec(direct_runner) + ' --play ' + mode
        text = '\n'.join(['[Desktop Entry]', 'Type=Application', 'Version=1.0',
            'Name=' + NAMES[mode], 'Exec=' + command,
            'Icon=' + desktop_string(available[mode]['icon'][0]), 'Terminal=false',
            'Categories=Game;' + ('X-WiVRn-VR;' if mode == 'wivrn' else ''),
            'X-D2RPCVRPlus-Game=' + desktop_string(Path(game).resolve()), ''])
        updates[path] = text.encode('utf-8')
    return updates
