"""Package the F4 skybox preview for an already configured Plus installation."""
import argparse
import hashlib
from pathlib import Path
import struct
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    mod = 'mods/D2R-PCVR/'
    shader_root = mod + 'reshade/reshade-shaders/'
    files = {
        mod + 'd2rloader/plugins/d2rl-pcvr.dll': root / 'build-frame/windows-native/d2rl-pcvr.dll',
        shader_root + 'Shaders/D2R_DepthFog.fx': root / 'reshade/D2R_DepthFog.fx',
        shader_root + 'Textures/D2R_Sky_ours/diorama-night.png': root / 'reshade/sky/diorama-night.png',
    }
    data = {name: path.read_bytes() for name, path in files.items()}
    if not next(iter(data.values())).startswith(b'MZ'):
        raise SystemExit('Expected a compiled Windows DLL')
    png = data[shader_root + 'Textures/D2R_Sky_ours/diorama-night.png']
    if png[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', png[16:24]) != (1774, 887):
        raise SystemExit('Expected the approved 1774 x 887 panorama')
    hashes = ''.join(f'{hashlib.sha256(blob).hexdigest()}  {name}\n' for name, blob in data.items())
    readme = f'''D2R PCVR Plus - F4 skybox preview
Source commit: {args.commit}

For the existing validated Plus mod-folder installation (merged PR #11).
Windows and Linux/Proton use exactly the same files.

1. Close the game and loader.
2. Keep copies of your existing DLL and D2R_DepthFog.fx outside the game/mod
   folders, so you can roll back. No save files are involved.
3. Extract the enclosed mods folder INTO your Diablo II: Resurrected game
   directory, merging folders and replacing just the DLL and shader.
   This also adds the panorama under reshade-shaders/Textures/D2R_Sky_ours.
4. Launch with your existing shortcut and select F4.

Passthrough is DISABLED for this preview on all runtimes, including WiVRn,
regardless of your saved passthrough setting. No INI edits are required.
F4 should show the night panorama behind the miniature, with soft edges.
F1/F2/F3 and Flat are unchanged. No floor or table is included yet.

Headset checks: turn/look up/down, walk around the miniature, edit its position,
height, yaw and width; sky should remain fixed in the tracking environment.
Verify both eyes, floating menus and transitions F1/F2/F3/F4.
The image still has an imperfect seam and is a first 1774 x 887 visual trial.

To roll back: close the game, restore BOTH the previous DLL and shader, and
remove the added diorama-night.png if desired. Existing launchers, dependencies,
configuration and characters are not changed by this package.

To replace the panorama later: use the same image filename and a 2:1 PNG;
ReShade resizes it to the dimensions declared in D2R_DepthFog.fx (1774 x 887).
Restart the game to reload it. A larger source will need a matching shader
texture size change to retain its full resolution.
'''
    output = root / 'dist-plus/D2R-PCVR-Plus-Skybox-Update.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, blob in data.items():
            archive.writestr(name, blob)
        archive.writestr('README.txt', readme)
        archive.writestr('SHA256SUMS.txt', hashes)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        for name, blob in data.items():
            assert archive.read(name) == blob
    print(output)
    print(hashes)


if __name__ == '__main__':
    main()
