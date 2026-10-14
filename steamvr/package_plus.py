"""Stage the tested Windows overlay with an isolated, verified embedded Python."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def build(root, dll, runtime, output):
    stage = output / 'D2R-PCVR-Plus-Windows'
    if stage.exists():
        raise ValueError('Refusing to overwrite staging directory: ' + str(stage))
    for name in ('python.exe', 'python312.dll', 'python312.zip', 'LICENSE.txt'):
        if not (runtime / name).is_file():
            raise ValueError('Incomplete embedded Python: ' + name)
    stage.mkdir(parents=True)
    shutil.copytree(runtime, stage / 'deps/python', ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    (stage / 'deps/python/python312._pth').write_text('python312.zip\n.\n../../steamvr\n', encoding='ascii')
    (stage / 'steamvr').mkdir()
    for name in ('launch_plus.py', 'launch_windows.py', 'game_install.py', 'process_guard.py', 'portable_plus.py'):
        shutil.copy2(root / 'steamvr' / name, stage / 'steamvr' / name)
    shutil.copy2(root / 'steamvr/vr_baseline.json', stage / 'vr-baseline.json')
    (stage / 'payload').mkdir()
    shutil.copy2(dll, stage / 'payload/d2rl-vrcam.dll')
    shutil.copy2(root / 'd2r_pcvr_plus_flat.ini', stage / 'payload/d2r_pcvr_plus_flat.ini')
    (stage / 'profiles').mkdir()
    shutil.copy2(root / 'd2r_vr.ini', stage / 'profiles/d2r_vr.reference.ini')
    shutil.copy2(root / 'd2r_pcvr_plus_flat.ini', stage / 'profiles/d2r_pcvr_plus_flat.ini')
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (stage / 'payload').iterdir()}
    (stage / 'payload-sha256.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    for name, action in [('Configure', 'configure'), ('Update', 'update'), ('Rollback', 'rollback'), ('Launch-Flat-Plus', 'flat'), ('Launch-VR-Plus', 'vr')]:
        (stage / (name + '.cmd')).write_text('@echo off\nsetlocal\n"%~dp0deps\\python\\python.exe" "%~dp0steamvr\\portable_plus.py" ' + action + ' %*\nset "result=%errorlevel%"\nif not "%result%"=="0" pause\nexit /b %result%\n', encoding='ascii')
    for name in ('PCVR-PLUS-PORTABLE.md', 'PCVR-PLUS.md', 'LICENSE', 'THIRD_PARTY.md'):
        shutil.copy2(root / name, stage / name)
    shutil.copytree(root / 'steamvr/licenses', stage / 'licenses')
    shutil.copy2(root / 'third_party/minhook/LICENSE.txt', stage / 'licenses/MinHook-LICENSE.txt')
    shutil.copy2(root / 'external/reshade/LICENSE.md', stage / 'licenses/ReShade-LICENSE.md')
    shutil.copy2(runtime / 'LICENSE.txt', stage / 'licenses/Python-LICENSE.txt')
    archive = output / (stage.name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                bundle.write(path, path.relative_to(output))
    (output / (archive.name + '.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n', encoding='ascii')
    print(archive)
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, default=ROOT / 'build-frame/windows-native/d2rl-vrcam.dll')
    parser.add_argument('--runtime', type=Path, default=ROOT / 'deps/python')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist-plus')
    args = parser.parse_args()
    build(ROOT, args.dll, args.runtime, args.output)
