#!/usr/bin/env python3
"""Build a replaceable, onedir Qt/Python bundle from the validated Linux kit."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent.parent
NAME = 'D2R-PCVR-Plus-Setup'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', type=Path, required=True)
    args = parser.parse_args()
    build = ROOT / 'build-frame/linux-setup'
    build.mkdir(parents=True, exist_ok=True)
    payload = build / 'payload'
    if payload.exists():
        shutil.rmtree(payload)
    payload.mkdir()
    with tarfile.open(args.kit) as archive:
        for member in archive.getmembers():
            path = Path(member.name)
            if (not member.isfile() or path.is_absolute() or '..' in path.parts
                    or path.parts[0] != 'D2R-PCVR-Plus-Linux-Test'):
                raise ValueError('Unexpected Linux kit archive member: ' + member.name)
            target = payload.joinpath(*path.parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.extractfile(member).read())
    manifest = json.loads((payload / 'manifest.json').read_text())
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if manifest['source_commit'] != commit:
        raise ValueError('DLL kit and setup source must have the same commit.')
    for name, digest in manifest['sha256'].items():
        if hashlib.sha256((payload / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Linux kit integrity check failed: ' + name)
    # Include editable setup sources and notices alongside the bundled modules.
    for name in ('setup_linux', 'setup_linux_gui', 'linux_setup_entry', 'linux_tls', 'steam_config', 'plus_artwork', 'plus_preferences'):
        shutil.copy2(ROOT / 'steamvr' / (name + '.py'), payload / 'steamvr')
    shutil.copy2(ROOT / 'LINUX-SETUP.md', payload)
    licenses = payload / 'runtime-licenses'
    licenses.mkdir(exist_ok=True)
    for package in ('PySide6', 'PySide6_Essentials', 'PySide6_Addons', 'shiboken6', 'pyinstaller'):
        dist = importlib.metadata.distribution(package)
        for file in dist.files or []:
            if 'license' in str(file).lower() or 'copying' in str(file).lower():
                source = Path(dist.locate_file(file))
                if source.is_file():
                    target = licenses / package / str(file)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, target)
    for file in (ROOT / 'installer/licenses').glob('*.txt'):
        shutil.copy2(file, licenses / file.name)
    import builtins
    builtins.license._Printer__setup()
    (licenses / 'PYTHON-LICENSE.txt').write_text('\n'.join(builtins.license._Printer__lines) + '\n')
    # PyInstaller analyzes the entry point's imports, including worker-only paths.
    subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onedir',
                    '--name', NAME, '--distpath', str(build / 'dist'), '--workpath', str(build / 'work'),
                    '--specpath', str(build), '--paths', str(ROOT / 'steamvr'),
                    '--add-data', str(payload) + ':payload',
                    str(ROOT / 'steamvr/linux_setup_entry.py')], check=True)
    bundle = build / 'dist' / NAME
    executable = bundle / NAME
    subprocess.run([str(executable), '--self-test'], check=True)
    subprocess.run([sys.executable, str(ROOT / 'installer/linux_tls_smoke.py'), str(executable)], check=True)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    subprocess.run([str(executable), '--gui-smoke'], env=env, check=True)
    # A stock route must execute its child and propagate status without game config.
    result = subprocess.run([str(executable), '--route', '/bin/sh', '-c', 'exit 17'])
    if result.returncode != 17:
        raise ValueError('Frozen stock launcher smoke test failed')
    shutil.copy2(ROOT / 'LINUX-SETUP.md', bundle / 'README.md')
    (bundle / 'build.json').write_text(json.dumps({'source_commit': commit, 'version': '0.2.3-preview',
        'python': sys.version, 'packages': {p: importlib.metadata.version(p) for p in ('PySide6', 'PyInstaller')},
        'hardware_validation': 'Earlier CLI routes validated on CachyOS/Quest 2; Qt installation requires acceptance testing.'}, indent=2))
    output = ROOT / 'dist-plus/D2R-PCVR-PLUS-Setup-0.2.3-Linux-x86_64.tar.gz'
    output.parent.mkdir(exist_ok=True)
    with tarfile.open(output, 'w:gz') as archive:
        archive.add(bundle, arcname=NAME)
    output.with_suffix(output.suffix + '.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
    print(output)


if __name__ == '__main__':
    main()
