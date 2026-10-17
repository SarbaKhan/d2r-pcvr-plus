#!/usr/bin/env python3
"""Guided removal using original installation backups; never restore character saves."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import tempfile

import install
import mod_identity
import setup_windows as setup
import steam_config
from game_install import storage_config

ROOT = Path(__file__).resolve().parent.parent
PLUGIN = mod_identity.PLUGIN
# This mod's plugin under its current and its old (0.3.16 and earlier) name.
PLUGINS = mod_identity.PLUGINS
FILE_NAMES = {'d2rloader.exe', 'd2rcore.dll', 'd2rloader.mpq', 'reshade64.dll',
              'openxr_loader.dll', 'reshade.ini', 'd2r_frame_reshade.ini'}
DIRECTORIES = {'d2rloader', 'mods', 'reshade-shaders', 'd2r_sky_ours'}


def snapshots(game, record, roots):
    """Follow recorded upgrades, with a bounded lookup for older/moved backups."""
    found = {}
    for root in roots:
        if (root / 'manifest.json').is_file():
            found[root.resolve()] = root / 'manifest.json'
        elif root.is_dir():
            for source in root.glob('*/manifest.json'):
                found[source.parent.resolve()] = source
    chain = []
    for name in record.get('snapshots', []):
        path = Path(name).resolve()
        if not (path / 'manifest.json').is_file():
            matches = [candidate for candidate in found if candidate.name == path.name]
            if len(matches) != 1:
                raise ValueError(f'Installation backup is missing: {path}. Select the folder containing that backup.')
            path = matches[0]
        found[path] = path / 'manifest.json'
        chain.append(path)
    matching = {}
    for path, source in found.items():
        manifest = json.loads(source.read_text(encoding='utf-8'))
        if Path(manifest['game']).resolve() == game.resolve():
            matching[path] = manifest
        elif path in chain:
            raise ValueError(f'Backup belongs to another game: {path}')

    def original(path):
        manifest = matching[path]
        installed = {str(install.safe_relative(name)).lower() for name in manifest['installed']}
        plugins = [plugin for plugin in PLUGINS if str(plugin).lower() in installed]
        return manifest.get('previous_mod') is False or (
            bool(plugins) and not any((path / 'previous' / plugin).exists() for plugin in plugins))

    if chain:
        first = chain[0]
        if original(first):
            selected = chain
        else:
            # Older releases have no receipt; include their original snapshots.
            legacy = sorted((path for path in matching if path.name < first.name), key=lambda p: p.name)
            selected = legacy + chain
    else:
        selected = sorted(matching, key=lambda p: p.name)
    baselines = [index for index, path in enumerate(selected) if original(path)]
    if not baselines:
        raise ValueError('The original pre-mod backup was not found. Select its snapshot folder or the folder containing it.')
    # Prefer the most recent fresh installation, rather than a previously removed one.
    selected = selected[baselines[-1]:]
    return [(path, matching[path]) for path in selected]


def restore_plan(game, chain, restore_video=False):
    """The first backup of each file is its pre-install state, including later additions.

    D2R VR uses the names this mod had up to 0.3.16 (d2rl-vrcam.dll, d2r_vr.ini).
    While D2R VR's plugin is in the folder those two files are its own and are
    left exactly as they are."""
    updates = {}
    keep = set()
    if mod_identity.plugin_owner(game / mod_identity.LEGACY_PLUGIN) == mod_identity.OTHER_MOD:
        keep = {str(name).lower() for name in mod_identity.LEGACY_FILES}
    # D2RLoader is shared: while another mod starts through it (D2R VR as mods\D2R-VR, say), its files stay and only
    # this mod's own files go.
    shared = bool(mod_identity.other_loader_users(game))
    for snapshot, manifest in chain:
        for name in manifest['installed']:
            relative = install.safe_relative(name)
            parts = [part.lower() for part in relative.parts]
            if (len(parts) == 1 and parts[0] not in FILE_NAMES
                    or len(parts) > 1 and parts[0] not in DIRECTORIES):
                raise ValueError(f'Backup lists a file outside the mod: {name}')
            target = game / relative
            source = snapshot / 'previous' / relative
            if (not target.resolve().is_relative_to(game.resolve())
                    or not source.resolve().is_relative_to(snapshot.resolve())):
                raise ValueError(f'Backup path escapes its folder: {name}')
            if target.exists() and not target.is_file() or source.exists() and not source.is_file():
                raise ValueError(f'Backup path is not a file: {name}')
            if str(relative).lower() in keep or shared and mod_identity.shared_dependency(relative):
                continue
            if target not in updates:
                updates[target] = source.read_bytes() if source.is_file() else None
    receipt = game / install.RECORD
    if not receipt.resolve().is_relative_to(game.resolve()):
        raise ValueError('Mod installation record points outside the game folder.')
    updates[receipt] = None
    if restore_video:
        snapshot, manifest = chain[0]
        source = snapshot / 'saves/Settings.json'
        if not source.resolve().is_relative_to(snapshot.resolve()):
            raise ValueError('Video settings backup points outside its folder.')
        payload = source.read_bytes()
        json.loads(payload.decode('utf-8-sig'))
        updates[Path(manifest['saves']).resolve() / 'Settings.json'] = payload
    return updates


def apply_updates(updates, backups, require_closed):
    """Back up and stage all edits, checking for races and rolling back failures."""
    require_closed()
    originals = {}
    for path in updates:
        if path.exists() and not path.is_file():
            raise ValueError(f'Expected a file: {path}')
        originals[path] = path.read_bytes() if path.is_file() else None
    updates = {path: payload for path, payload in updates.items() if payload != originals[path]}
    if not updates:
        return None
    backup = backups / ('uninstall-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    recovery = []
    staged, replaced = {}, []
    try:
        for index, (path, payload) in enumerate(updates.items()):
            previous = f'files/{index}' if originals[path] is not None else None
            if previous is not None:
                destination = backup / previous
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(originals[path])
            recovery.append({'target': str(path), 'previous': previous})
            if payload is not None:
                path.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.NamedTemporaryFile(dir=path.parent, prefix='d2r-uninstall-', delete=False) as output:
                    output.write(payload)
                    staged[path] = Path(output.name)
        (backup / 'manifest.json').write_text(json.dumps({'files': recovery}, indent=2) + '\n', encoding='utf-8')
        require_closed()
        for path in updates:
            current = path.read_bytes() if path.is_file() else None
            if current != originals[path]:
                raise ValueError('A game or Steam file changed during removal. Close both and retry.')
        for path, payload in updates.items():
            if payload is None:
                path.unlink()
            else:
                os.replace(staged[path], path)
            replaced.append(path)
        for path, payload in updates.items():
            current = path.read_bytes() if path.is_file() else None
            if current != payload:
                raise OSError(f'Removal verification failed: {path}')
    except BaseException:
        for path in reversed(replaced):
            if originals[path] is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(originals[path])
        raise
    finally:
        for temporary in staged.values():
            temporary.unlink(missing_ok=True)
    return backup


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    setup.verify_release(ROOT)
    if args.check:
        print('Windows release files verified. No removal was performed.')
        return
    if os.name != 'nt':
        raise ValueError('Run Uninstall-Windows.cmd on your Windows PC.')
    print('\nD2R PCVR - Remove the Windows mod')
    print('Current characters, shared stash, original game files and the game installation are kept.')
    print('Keep the installation backups until removal is complete.')
    store = ['steam', 'battlenet'][setup.choose('Which game edition should be restored?', ['Steam', 'Battle.net'])]
    steam = setup.folder('Steam installation folder', setup.steam_location(), 'steam.exe')
    default = (setup.app_folder(setup.libraries(steam), steam_config.APP_ID) if store == 'steam'
               else setup.battlenet_location())
    game = setup.folder('D2R game folder to remove the mod from', default, 'D2R.exe').resolve()
    detected, _ = storage_config(game)
    if detected != store:
        raise ValueError(f'This is a {detected} installation, but {store} was selected.')
    record = install.read_record(game)
    roots = [ROOT / 'setup-backups/game', ROOT / 'build-frame/backups']
    found = mod_identity.survey(game, roots)
    installed = found.ours or (game / install.RECORD).is_file()
    if found.other == mod_identity.OTHER_MOD:
        print('D2R VR (another VR mod) is also installed here; its d2rl-vrcam.dll and d2r_vr.ini are kept.')
    updates = {}
    if installed:
        while True:
            try:
                chain = snapshots(game, record, roots)
                break
            except ValueError as error:
                print('\n' + str(error))
                if setup.choose('Locate installation backups?', ['Choose backup folder', 'Cancel']) == 1:
                    print('Removal cancelled. No game or Steam files were changed.')
                    return
                roots.append(setup.folder('Backup snapshot or folder containing snapshots'))
        restore_video = setup.choose('Video settings after removal?', [
            'Keep current video settings (shared by Steam and Battle.net)',
            'Restore video settings from before the first mod install']) == 1
        updates.update(restore_plan(game, chain, restore_video))
    else:
        print('No installed mod found. Setup will check for remaining VR shortcuts.')
    accounts = setup.profiles(steam)

    def steam_updates():
        result = {}
        for profile, _ in accounts:
            state = record.get('steam_profiles', {}).get(str(profile.resolve()), {})
            result.update(steam_config.uninstall_updates(profile, game, store, state))
        return result

    steam_updates()  # Preflight before the removal confirmation.
    print(f'\nRemove VR from: {game}')
    print('Matching VR shortcuts will be removed from local Steam accounts; unrelated entries are kept.')
    print('Character saves and installation backups will not be deleted.')
    if setup.choose('Continue with removal?', ['Cancel', 'Remove the VR mod']) == 0:
        print('Removal cancelled. No game or Steam files were changed.')
        return
    setup.wait_for_closed()
    # Steam flushes its profile on exit. Build edits from those final bytes.
    updates.update(steam_updates())
    backup = apply_updates(updates, ROOT / 'setup-backups', setup.require_closed)
    print('\nRemoval complete. Use the original Steam or Battle.net Play button for desktop play.')
    if backup is not None:
        print(f'Files changed during removal were backed up in: {backup}')
    else:
        print('No remaining mod files or matching Steam configuration needed changing.')
    print('The extracted mod folder, installation backups and diagnostic logs were kept.')


if __name__ == '__main__':
    try:
        main()
    except (KeyboardInterrupt, EOFError):
        print('\nRemoval cancelled. Keep setup-backups.')
        raise SystemExit(1)
    except (OSError, ValueError, KeyError) as error:
        print(f'\nRemoval could not finish: {error}')
        print('Keep setup-backups. See INSTALL-WINDOWS.md for recovery or manual removal.')
        raise SystemExit(1)
