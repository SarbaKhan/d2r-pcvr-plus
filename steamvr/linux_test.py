#!/usr/bin/env python3
"""Temporary CLI for Linux hardware validation; no GUI or Steam config writes."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

from launch_linux import ROOT, runtime_manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('configure', 'install', 'flat', 'steamvr', 'wivrn'))
    p.add_argument('--game', type=Path)
    p.add_argument('--steam', type=Path, help='Native Linux Steam launcher, e.g. /usr/bin/steam')
    p.add_argument('--steamvr-runtime', type=Path)
    p.add_argument('--wivrn-runtime', type=Path)
    p.add_argument('--saves', type=Path, help='Existing Settings.json folder for a custom Proton prefix')
    p.add_argument('--probe', action='store_true', help='VR only: diagnose OpenXR instead of starting D2R')
    args = p.parse_args()
    config_path = ROOT / 'linux-launch.json'
    config = json.loads(config_path.read_text()) if config_path.exists() else {}
    if args.action == 'configure':
        if not args.game or not (args.game / 'D2R.exe').is_file():
            p.error('--game must contain D2R.exe')
        steam = args.steam or Path(shutil.which('steam') or '/usr/bin/steam')
        if not steam.is_file():
            p.error('--steam must point to the Linux Steam launcher')
        config['game'] = str(args.game.resolve())
        # Preserve the wrapper path (some launchers depend on argv[0]).
        config['steam'] = str(steam.absolute())
        if args.saves:
            config['saves'] = str(args.saves.resolve())
        for mode in ('steamvr', 'wivrn'):
            path = getattr(args, mode + '_runtime')
            if path:
                config[mode + '_runtime'] = runtime_manifest(path, mode)
        config_path.write_text(json.dumps(config, indent=2) + '\n')
        print('Steam D2R launch options (keep the package at this location):')
        print('python3 ' + shlex.quote(str(ROOT / 'steamvr/launch_linux.py')) + ' %command%')
        return 0
    if not config:
        p.error('Run configure first')
    if args.action == 'install':
        command = [sys.executable, str(ROOT / 'steamvr/install.py'), '--game', config['game'],
                   '--plugin', str(ROOT / 'bin/d2rl-vrcam.dll'), '--deps', str(ROOT / 'deps'),
                   '--backup-dir', str(ROOT / 'installation-backups'), '--download-deps', '--preserve-settings']
        if config.get('saves'):
            command += ['--saves', config['saves']]
        subprocess.run(command, check=True)
        return 0
    if args.probe and args.action == 'flat':
        p.error('--probe is VR only')
    command = [config['steam'], '-applaunch', '2536520', '--plus-' + args.action]
    if args.probe:
        command.append('--plus-probe')
    return subprocess.run(command).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'PCVR Plus Linux: {error}')
