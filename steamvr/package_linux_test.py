#!/usr/bin/env python3
"""Build a source-traceable hardware-test kit; no game data or XR runtime bundled."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import io

ROOT = Path(__file__).resolve().parent.parent


def main():
    files = ['LICENSE', 'docs/upstream-refresh-plus.md', 'docs/diorama-editor.md', 'THIRD_PARTY.md', 'CONTROLS.md', 'LINUX-TEST.md', 'LINUX-SETUP.md', 'd2r_pcvr.ini',
             'd2r_pcvr_plus_flat.ini', 'reshade/D2R_DepthFog.fx']
    modules = ['launch_linux', 'linux_test', 'launch_plus', 'install', 'game_install', 'mod_identity',
               'process_guard', 'prepare_icons', 'steam_assets', 'game_assets', 'casc_assets',
               'prepare_potions', 'potion_assets', 'vendor/bitknit']
    files += ['steamvr/' + name + '.py' for name in modules]
    files += [ 'external/reshade/LICENSE.md', 'third_party/minhook/LICENSE.txt']
    for folder, pattern in [('steamvr/licenses', '*.txt'), ('reshade/sky', '*.png')]:
        files += [p.relative_to(ROOT).as_posix() for p in sorted((ROOT / folder).glob(pattern))]
    files += ['artwork/manifest.json', 'artwork/README.md']
    files += ['artwork/' + mode + '/' + kind + '.png' for mode in ('flat', 'steamvr', 'wivrn', 'virtual-desktop')
              for kind in ('icon', 'cover', 'wide-cover', 'background', 'logo')]
    payload = {name: (ROOT / name).read_bytes() for name in files}
    for name in ('d2rl-pcvr.dll', 'openxr-probe.exe'):
        payload['bin/' + name] = (ROOT / 'build-frame/windows-native' / name).read_bytes()
    manifest = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'status': 'CLI routes validated on CachyOS/Quest 2; new setup requires acceptance testing',
                'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in payload.items()}}
    payload['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    output = ROOT / 'dist-plus/D2R-PCVR-Plus-Linux-Test.tar.gz'
    output.parent.mkdir(exist_ok=True)
    with tarfile.open(output, 'w:gz') as archive:
        for name, data in payload.items():
            info = tarfile.TarInfo('D2R-PCVR-Plus-Linux-Test/' + name)
            info.size = len(data)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(data))
    # Read back every entry; catch missing/corrupt payload before publication.
    with tarfile.open(output) as archive:
        for name, digest in manifest['sha256'].items():
            assert hashlib.sha256(archive.extractfile('D2R-PCVR-Plus-Linux-Test/' + name).read()).hexdigest() == digest
    output.with_suffix(output.suffix + '.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
    print(output)


if __name__ == '__main__':
    main()
