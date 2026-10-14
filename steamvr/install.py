#!/usr/bin/env python3
"""Local, reversible installation; does not change Steam's original executable."""
import argparse
import configparser
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
from pathlib import PurePosixPath
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from prepare_icons import prepare as prepare_icons
from prepare_potions import prepare as prepare_potions, CODES as POTION_CODES
from game_install import validate_game

ROOT = Path(__file__).resolve().parent.parent
STEAM = Path.home() / '.local/share/Steam'
GAME = STEAM / 'steamapps/common/Diablo II Resurrected'
SAVES = STEAM / 'steamapps/compatdata/2536520/pfx/drive_c/users/steamuser/Saved Games/Diablo II Resurrected'
# Explicitly reviewed 1.3.1 archives from https://d2rloader.net/download.html.
# Upstream replaced the same URL; retain the historical pin for existing caches.
# Never accept a new digest automatically from the network.
LOADER_HASHES = (
    'f7bf0287398890b09bcc68b576327e727304bd88f7be80160f9676e8bd6871e2',
    '9286c6b5bff7f1043658411dbea305faa9698b6a455e6441668fc6d02c22aad7',
)
RESHADE_HASH = 'afe4c8f13048306307983b8b3d41d5bf00a86820440b0e57dea10950e1176445'
SDK_VERSION = '10.0.26100.9169'
SDK_HASH = '475269434dcd808a67853773272f972c3229c0e10c3ddc821290e70cc0f6904d'
COMPILER_HASH = '3bd6d7acfe8a0a84acd166fba1a537ce68bd21c8edce2bd9bb863cfa034136d4'
RECORD = Path('d2rloader/pcvr-install.json')


def read_record(game):
    source = game / RECORD
    if not source.resolve().is_relative_to(game.resolve()):
        raise ValueError('Mod installation record points outside the game folder.')
    if not source.is_file():
        return {}
    record = json.loads(source.read_text(encoding='utf-8'))
    if Path(record['game']).resolve() != game.resolve():
        raise ValueError('Mod installation record belongs to a different game folder.')
    if (not isinstance(record.get('snapshots', []), list)
            or any(not isinstance(name, str) or not name for name in record.get('snapshots', []))
            or not isinstance(record.get('steam_profiles', {}), dict)
            or any(not isinstance(state, dict) for state in record.get('steam_profiles', {}).values())):
        raise ValueError('Invalid mod installation record.')
    return record


def write_record(game, record):
    target = game / RECORD
    if not target.resolve().is_relative_to(game.resolve()):
        raise ValueError('Mod installation record points outside the game folder.')
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix('.tmp')
    try:
        temporary.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)


def require_game_closed():
    if os.name == 'nt':
        import csv
        rows = csv.reader(subprocess.check_output(
            ['tasklist', '/FO', 'CSV', '/NH'], text=True, creationflags=0x08000000).splitlines())
        names = [row[0].lower() for row in rows if row]
    else:
        names = []
        for cmd in Path('/proc').glob('[0-9]*/cmdline'):
            try:
                first = cmd.read_bytes().split(b'\0', 1)[0].decode(errors='replace')
                names.append(first.replace('\\', '/').rsplit('/', 1)[-1].lower())
            except OSError:
                continue
    if any(name in ('d2r.exe', 'd2rloader.exe') for name in names):
        raise SystemExit('Save and exit D2R before installing or restoring the mod.')


def dependency_io_error(path, action, error):
    return SystemExit(
        f'Could not {action} required dependency "{path}": {error}. '
        'The file may be missing, blocked/quarantined by security software, or inaccessible. '
        'Check file permissions and your security software\'s history. On Windows, check '
        'Windows Security > Virus & threat protection > Protection history; '
        'include any detection name and affected file path in your bug report. '
        'No game files changed.')


def read_dependency(path, missing_ok=False):
    try:
        return path.read_bytes()
    except FileNotFoundError as error:
        if missing_ok:
            return None
        raise dependency_io_error(path, 'read', error) from error
    except OSError as error:
        raise dependency_io_error(path, 'read', error) from error


def dependency_checksum_error(source, expected, actual):
    # Keep this on one line: the graphical setup surfaces the last child line.
    return SystemExit(
        f'Checksum mismatch for {source}. Expected SHA-256: {", ".join(expected)}; '
        f'received: {actual}. The download may have changed upstream or been corrupted. '
        'Use the latest D2R PCVR installer; if it still fails, report these hashes. '
        'No game files changed.')


def download_verified(url, path, digest, max_bytes=100 * 1024 * 1024):
    expected = (digest,) if isinstance(digest, str) else tuple(digest)
    cached = read_dependency(path, missing_ok=True)
    if cached is not None and hashlib.sha256(cached).hexdigest() in expected:
        print(f'Using verified cached dependency: {path.name}')
        return
    print(f'Downloading {url}')
    request = urllib.request.Request(url, headers={'User-Agent': 'D2R-PCVR-installer/0.1'})
    with urllib.request.urlopen(request, timeout=60) as response:
        data = response.read(max_bytes + 1)
    received = hashlib.sha256(data).hexdigest()
    if received not in expected:
        raise dependency_checksum_error(url, expected, received)
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    except OSError as error:
        raise dependency_io_error(path, 'save', error) from error
    # Security software can remove/block a downloaded file even after writing
    # succeeds. Verify disk access before fetching further dependencies.
    saved = hashlib.sha256(read_dependency(path)).hexdigest()
    if saved != received:
        raise dependency_checksum_error(f'saved dependency {path}', (received,), saved)


def safe_relative(name):
    # Apply Windows path rules even when preparing/installing on Linux.
    path = PurePosixPath(name.replace('\\', '/'))
    if path.is_absolute() or '..' in path.parts or ':' in str(path) or not path.parts:
        raise SystemExit('Unsafe path in loader archive or backup manifest')
    return Path(*path.parts)


def default_saves(game):
    if os.name == 'nt':
        return Path.home() / 'Saved Games/Diablo II Resurrected'
    # Steam creates the Proton prefix in the library containing the game.
    return game.parent.parent / 'compatdata/2536520/pfx/drive_c/users/steamuser/Saved Games/Diablo II Resurrected'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', type=Path, default=GAME)
    ap.add_argument('--deps', type=Path, default=Path.home() / 'vrdeps')
    ap.add_argument('--saves', type=Path, help='Saved Games folder (for custom prefixes/redirected Windows folders)')
    ap.add_argument('--plugin', type=Path, default=ROOT / 'build-frame/d2rl-vrcam.dll')
    ap.add_argument('--openxr-loader', type=Path, help='Windows x64 openxr_loader.dll; required on native Windows')
    ap.add_argument('--backup-dir', type=Path, default=ROOT / 'build-frame/backups')
    ap.add_argument('--download-deps', action='store_true', help='Fetch pinned D2RLoader and ReShade from their official sites')
    ap.add_argument('--store', choices=['auto', 'steam', 'battlenet'], default='auto')
    ap.add_argument('--experimental-battlenet', action='store_true', help='Enable Battle.net 3.3.93847 for local offline compatibility testing')
    ap.add_argument('--casclib', type=Path, default=ROOT / 'bin/CascLib.dll', help='Pinned Unicode x64 CascLib 3.0 for Battle.net asset reading')
    ap.add_argument('--preserve-settings', action='store_true', help='Keep existing calibration, mod/ReShade configuration and video settings when upgrading')
    ap.add_argument('--restore', type=Path, help='Restore the installation snapshot made by this script')
    args = ap.parse_args()
    game = args.game.resolve()
    require_game_closed()
    saves = (args.saves or default_saves(game)).resolve()
    if args.restore:
        snapshot = args.restore.resolve()
        m = json.loads((snapshot / 'manifest.json').read_text())
        if str(game) != m['game']:
            raise SystemExit('Snapshot belongs to a different game folder')
        for rel in m['installed']:
            rel = safe_relative(rel)
            target = game / rel
            old = snapshot / 'previous' / rel
            if old.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(old, target)
            elif target.is_file():
                target.unlink()
        saves = Path(m.get('saves', str(saves)))
        saves.mkdir(parents=True, exist_ok=True)
        shutil.copy2(snapshot / 'saves/Settings.json', saves / 'Settings.json')
        print('Mod files and video settings restored. Character progress was kept.')
        return
    try:
        store, version = validate_game(game, args.store, args.experimental_battlenet)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    if store == 'battlenet':
        if os.name != 'nt':
            raise SystemExit('Experimental Battle.net support currently requires native Windows.')
        if not args.casclib.is_file():
            raise SystemExit('Battle.net installation requires the packaged Unicode x64 CascLib.dll.')
        print('Experimental Battle.net build: use a local OFFLINE character for compatibility testing.')
    if not args.plugin.is_file():
        raise SystemExit(f'Mod DLL not found: {args.plugin}. Extract the complete release ZIP and follow INSTALL-WINDOWS.md.')
    if os.name == 'nt' and not args.openxr_loader:
        raise SystemExit('Native Windows requires --openxr-loader pointing to the release bin/openxr_loader.dll')
    if args.openxr_loader and not args.openxr_loader.is_file():
        raise SystemExit('OpenXR loader DLL was not found')
    if not (saves / 'Settings.json').is_file():
        raise SystemExit(f'Launch stock D2R once first, or pass --saves; Settings.json not found in {saves}')
    record = read_record(game)
    # Read and validate all inputs before changing any game files.
    settings = json.loads((saves / 'Settings.json').read_text(encoding='utf-8-sig'))
    archive = args.deps / 'D2RLoader-1.3.1-beta.zip'
    reshade = args.deps / 'reshade-6.8/ReShade64.dll'
    compiler = args.deps / 'd3dcompiler47/modern/d3dcompiler_47.dll'
    if args.download_deps:
        download_verified('https://d2rloader.net/downloads/D2RLoader-1.3.1-beta.zip', archive, LOADER_HASHES)
        setup = args.deps / 'ReShade_Setup_6.8.0_Addon.exe'
        download_verified('https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe', setup, RESHADE_HASH)
        # Official setup executable contains an appended ZIP. Extract the DLL
        # locally; do not run setup or create an automatically loaded DXGI proxy.
        with zipfile.ZipFile(setup) as z:
            reshade.parent.mkdir(parents=True, exist_ok=True)
            reshade.write_bytes(z.read('ReShade64.dll'))
        if os.name != 'nt':
            # Wine's compiler stayed busy throughout multi-minute tests of the
            # effect. Use Microsoft's current SDK redist privately under the
            # mod; the Windows 8.1 Winetricks version lacks shader model 5.1.
            sdk = args.deps / f'd3dcompiler47/microsoft.windows.sdk.cpp.{SDK_VERSION}.nupkg'
            download_verified(f'https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.cpp/{SDK_VERSION}/{sdk.name}',
                              sdk, SDK_HASH, max_bytes=200 * 1024 * 1024)
            with zipfile.ZipFile(sdk) as z:
                compiler.parent.mkdir(parents=True, exist_ok=True)
                compiler.write_bytes(z.read('c/Redist/D3D/x64/d3dcompiler_47.dll'))
    if os.name != 'nt' and not compiler.is_file():
        raise SystemExit('Proton needs the SDK shader compiler; run with --download-deps.')
    if compiler.is_file() and hashlib.sha256(read_dependency(compiler)).hexdigest() != COMPILER_HASH:
        raise SystemExit('SDK shader compiler checksum mismatch; run with --download-deps.')
    loader_digest = hashlib.sha256(read_dependency(archive)).hexdigest()
    if loader_digest not in LOADER_HASHES:
        raise dependency_checksum_error(f'D2RLoader archive {archive}', LOADER_HASHES, loader_digest)
    payload = {}
    try:
        with zipfile.ZipFile(archive) as z:
            for name in z.namelist():
                if not name.endswith('/'):
                    rel = safe_relative(name)
                    if rel.parts[0].lower() not in ('mods', 'd2rloader', 'd2rloader.exe', 'd2rcore.dll', 'd2rloader.mpq'):
                        raise SystemExit(f'Unexpected file in loader archive: {name}')
                    payload[rel] = z.read(name)
    except OSError as error:
        raise dependency_io_error(archive, 'read', error) from error
    payload[Path('ReShade64.dll')] = read_dependency(reshade)
    payload[Path('d2rloader/plugins/d2rl-vrcam.dll')] = args.plugin.read_bytes()
    if compiler.is_file():
        payload[Path('d2rloader/runtime/d3dcompiler_47.dll')] = compiler.read_bytes()
    if args.openxr_loader:
        payload[Path('openxr_loader.dll')] = args.openxr_loader.read_bytes()
    # Native artwork is read locally, never downloaded/bundled with the mod.
    # Prepare before mutating the install and journal it with all other files.
    with tempfile.TemporaryDirectory(prefix='d2r-vr-icons-') as temporary:
        try:
            result = prepare_icons(game, temporary, casclib=args.casclib)
        except (OSError, ValueError, KeyError, IndexError) as error:
            print(f'Native icon cache unavailable; wheel labels still work: {error}')
        else:
            for name in result['images'] + ['manifest.tsv', 'cache.json']:
                payload[Path('d2rloader/plugins/d2r_vr_icons') / safe_relative(name)] = (Path(temporary) / name).read_bytes()
            print(f"Prepared {result['icons']} native icon bindings from your installed game.")
    with tempfile.TemporaryDirectory(prefix='d2r-vr-potions-') as temporary:
        try:
            result = prepare_potions(game, temporary, casclib=args.casclib)
        except (OSError, ValueError, KeyError, IndexError) as error:
            print(f'Native potion models unavailable; retaining icon fallback: {error}')
        else:
            for name in [code + '.pvr' for code in POTION_CODES] + ['cache.json']:
                payload[Path('d2rloader/plugins/d2r_vr_potions') / safe_relative(name)] = (Path(temporary) / name).read_bytes()
            print(f"Prepared {result['models']} original 3D potion models from your installed game.")
    require_game_closed()
    snapshot = args.backup_dir / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    snapshot.mkdir(parents=True)
    # Only video settings are needed for rollback. Never copy characters/stash.
    (snapshot / 'saves').mkdir()
    shutil.copy2(saves / 'Settings.json', snapshot / 'saves/Settings.json')
    installed = []
    manifest = {'game': str(game), 'store': store, 'game_version': version,
                'previous_mod': (game / 'd2rloader/plugins/d2rl-vrcam.dll').is_file(),
                'saves': str(saves), 'installed': installed}
    (snapshot / 'manifest.json').write_text(json.dumps(manifest, indent=2))

    def put(rel, content):
        dest = game / rel
        if dest.exists():
            old = snapshot / 'previous' / rel
            old.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dest, old)
        dest.parent.mkdir(parents=True, exist_ok=True)
        installed.append(str(rel))
        # Journal before replacing so an interrupted install can be restored.
        (snapshot / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        dest.write_bytes(content)

    record.update({'game': str(game), 'store': store, 'saves': str(saves),
                   'snapshots': record.get('snapshots', []) + [str(snapshot.resolve())]})
    # Track the chain before other writes, including interrupted installs.
    put(RECORD, (json.dumps(record, indent=2) + '\n').encode('utf-8'))
    for rel, content in payload.items():
        put(rel, content)
    cfg = configparser.ConfigParser(interpolation=None, strict=False, allow_no_value=True)
    # The introductory comment also mentions [camera]. Splitting that comment
    # produced a malformed first camera section which Win32 profile reads used
    # instead of the real one, silently restoring the large default near plane.
    lines = (ROOT / 'd2r_vr.ini').read_text(encoding='utf-8').splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == '[camera]')
    cfg.read_string('\n'.join(lines[start:]))
    changes = {
        'camera': {'fov_from_flatvr': '0', 'neck_model': '0', 'near': '0.2'},
        'stereo': {'afr': '1', 'pair_per_tick': '1', 'true_scale': '0', 'convergence': '0', 'swap': '0'},
        'turn': {'snap': '30'},
        'mode': {'platform': '1', 'vr_mode': '4', 'vr_mode_order': '2'},
        'hud': {'hide': '0', 'map': '0', 'stereo_labels': '1', 'monster_draw_order': '1'},
        'update': {'check': '0'},
        'gameplay': {'motion_controls': '1', 'arrow_speed': '2', 'arrow_direct_release': '1', 'contact_hits': '1'},
        'render': {'game_height_fog': '1', 'fixed_foveation': '1', 'foveation_inner': '0.55',
                   'symmetric_projection': '1', 'defer_gpu_wait': '0',
                   'eye_tracked_foveation': '0', 'gaze_foveation_inner': '0.35'},
    }
    for section, values in changes.items():
        if not cfg.has_section(section):
            cfg.add_section(section)
        for key, value in values.items():
            cfg.set(section, key, value)
    import io
    out = io.StringIO(); cfg.write(out)
    def put_config(rel, content):
        if not args.preserve_settings or not (game / rel).is_file():
            put(rel, content)

    put_config(Path('d2rloader/plugins/d2r_vr.ini'), out.getvalue().encode())
    flat_profile = Path('d2rloader/plugins/d2r_pcvr_plus_flat.ini')
    # Flat preferences survive repair and full VR calibration resets.
    if not (game / flat_profile).is_file():
        put(flat_profile, (ROOT / 'd2r_pcvr_plus_flat.ini').read_bytes())
    put_config(Path('ReShade.ini'), b'[GENERAL]\nEffectSearchPaths=.\\reshade-shaders\\Shaders\nTextureSearchPaths=.\\reshade-shaders\\Textures\nPresetPath=.\\D2R_Frame_ReShade.ini\n[INPUT]\nKeyOverlay=36,0,0,0\n')
    put_config(Path('D2R_Frame_ReShade.ini'), b'Techniques=D2R_DepthFog@D2R_DepthFog.fx\n')
    put(Path('reshade-shaders/Shaders/D2R_DepthFog.fx'), (ROOT / 'reshade/D2R_DepthFog.fx').read_bytes())
    for texture in (ROOT / 'reshade/sky').glob('*.png'):
        put(Path('D2R_Sky_ours') / texture.name, texture.read_bytes())
        put(Path('reshade-shaders/Textures/D2R_Sky_ours') / texture.name, texture.read_bytes())
    settings.update({'VSync': 0, 'NVIDIA DLSS': 0, 'Anti Aliasing': 1, 'Framerate Cap': 0,
                     'Dynamic Resolution Scaling': 0, 'Camera Shake': 0,
                     'Resolution Scale': 100, 'Ambient Occlusion Quality': 2,
                     'Atmospherics Quality': 2, 'Shadow Quality': 2})
    if not args.preserve_settings:
        (saves / 'Settings.json').write_text(json.dumps(settings, indent=4))
    print(f'Installed offline PCVR Plus mod. Installation and video-settings backup: {snapshot}')


if __name__ == '__main__':
    main()
