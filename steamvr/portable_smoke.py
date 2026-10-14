"""Windows ZIP integration check, using only its embedded interpreter and fake game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def smoke(archive):
    with tempfile.TemporaryDirectory(prefix='PCVR Plus é ') as temporary:
        work = Path(temporary).resolve()
        with zipfile.ZipFile(archive) as bundle:
            bundle.extractall(work)
        package = work / 'D2R-PCVR-Plus-Windows'
        python = package / 'deps/python/python.exe'
        script = package / 'steamvr/portable_plus.py'
        environment = {k:v for k,v in os.environ.items() if not k.upper().startswith('PYTHON')}
        environment['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
        def run(*arguments):
            return subprocess.run([str(python), *map(str, arguments)], cwd=work,
                                  env=environment, check=True, capture_output=True, text=True)
        run('-c', 'import launch_plus, launch_windows, game_install, process_guard, portable_plus; import ctypes, hashlib, json, subprocess, tempfile; print("isolated runtime OK")')
        for action in ('configure', 'update', 'rollback', 'flat', 'vr'):
            run(script, action, '--help')
        game = work / "Test game ' é"
        plugins = game / 'd2rloader/plugins'
        plugins.mkdir(parents=True)
        for path, data in [(game/'D2R.exe', b'fake - never executed'), (game/'D2RLoader.exe', b'loader'),
                           (plugins/'d2rl-vrcam.dll', b'original DLL'), (plugins/'d2r_vr.ini', b'personal VR'),
                           (game/'dxgi.dll', b'existing ReShade'), (game/'openxr_loader.dll', b'existing runtime'),
                           (game/'d2rloader/pcvr-install.json', b'existing setup state'),
                           (game/'hero.d2s', b'unchanged character')]:
            path.write_bytes(data)
        preserved = {p:p.read_bytes() for p in game.rglob('*') if p.is_file() and p.name != 'd2rl-vrcam.dll'}
        # Exercise user-visible CMD wrappers too; only fixture files are modified.
        def cmd(name):
            subprocess.run(['cmd.exe', '/d', '/c', 'call', str(package/(name+'.cmd')), '--game', str(game)],
                           cwd=work, env=environment, check=True, stdin=subprocess.DEVNULL,
                           capture_output=True, text=True, timeout=60)
        cmd('Update')
        if (plugins/'d2rl-vrcam.dll').read_bytes() != (package/'payload/d2rl-vrcam.dll').read_bytes():
            raise AssertionError('Installed DLL differs from packaged DLL')
        if not (plugins/'d2r_pcvr_plus_flat.ini').is_file():
            raise AssertionError('Flat profile missing')
        cmd('Rollback')
        if (plugins/'d2rl-vrcam.dll').read_bytes() != b'original DLL':
            raise AssertionError('Rollback did not restore original')
        for path, data in preserved.items():
            if path.read_bytes() != data:
                raise AssertionError('Existing installation changed: ' + str(path))
        print('Portable ZIP smoke passed: isolated Python, CMD update/rollback, preserved files.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    smoke(parser.parse_args().archive)
