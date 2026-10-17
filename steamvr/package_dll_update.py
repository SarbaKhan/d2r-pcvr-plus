"""Package only the compiled plugin for an existing mod-folder installation."""
import argparse
import hashlib
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    dll = root / 'build-frame/windows-native/d2rl-pcvr.dll'
    data = dll.read_bytes()
    if not data.startswith(b'MZ'):
        raise SystemExit('Expected a compiled Windows DLL')
    destination = 'mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll'
    digest = hashlib.sha256(data).hexdigest()
    readme = f'''D2R PCVR Plus - DLL-only test update
Source commit: {args.commit}
DLL SHA-256: {digest}

For an EXISTING installation of the validated upstream-refresh Plus build.
The same Windows DLL is used on Windows and Linux through Proton.

1. Close Diablo II: Resurrected and its loader.
2. Locate your game folder, then {destination}.
3. Move the existing DLL outside the plugins directory for rollback.
4. Copy the enclosed DLL to the same path, replacing that one file only.
5. Launch using your existing shortcut.

Do not change INI files, shaders, launchers, dependencies or saves.
Do not install this alongside an older global d2rl-vrcam.dll installation.
To roll back: close the game and restore the previous DLL at the same path.

Changes:
- Candidate fix for the left-eye black background behind floating F3/F4 menus.
- English Diorama editor entry, persistent editing and exit notices.

Verify both eyes with inventory/stash/shop/waypoint menus in F3/F4.
Verify Start entry/exit, notice timeout, F1/F2/Flat and normal F3 dialogue.
Headset validation remains required; successful CI is not a visual test.
'''
    output = root / 'dist-plus/D2R-PCVR-Plus-DLL-Update.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(destination, data)
        archive.writestr('README.txt', readme)
        archive.writestr('SHA256SUMS.txt', f'{digest}  {destination}\n')
    print(output)


if __name__ == '__main__':
    main()
