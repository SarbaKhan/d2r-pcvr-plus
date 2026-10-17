#!/usr/bin/env python3
"""Which VR mod owns the plugin files in a game folder: D2R PCVR or D2R VR.

D2R PCVR started as a fork of D2R VR 0.152 by BodyWalkVR (MIT). Up to 0.3.16
both mods installed d2rloader/plugins/d2rl-vrcam.dll with d2r_vr.ini beside it,
so either installer replaced the other mod's files. D2R PCVR now installs
d2rl-pcvr.dll and d2r_pcvr.ini; D2R VR keeps the old names.

On an update the installer carries an earlier D2R PCVR's settings and body
calibration over from d2r_vr.ini to d2r_pcvr.ini and removes its own old files
(journaled like every other change, so a rollback puts them back). The file
name alone proves nothing: the old files count as D2R PCVR's only when this
mod's install record (d2rloader/pcvr-install.json) or one of its installation
snapshots says it installed them, and the plugin does not carry D2R VR's marks.
Files of those names that belong to D2R VR are never changed, moved or deleted;
setup says another mod is installed and stops before changing anything, since
D2RLoader would load both VR plugins into one game.
"""
from dataclasses import dataclass
import json
from pathlib import Path

MOD_ID = 'd2r-pcvr'
# Installed as the D2RLoader mod D2R-PCVR: this mod's files live under <game>\mods\D2R-PCVR, so another VR mod
# (D2R VR, the mod D2R-VR) can sit beside it and D2RLoader runs the one it is started with (-mod D2R-PCVR).
# D2RLoader loads a mod's plugins only while that mod is active. Players see DISPLAY_NAME.
MOD_NAME = 'D2R-PCVR'
DISPLAY_NAME = 'D2R VR Unleashed'
TAGLINE = 'Diablo II reimagined for physical, immersive VR'
MOD_DIR = Path('mods') / MOD_NAME
PLUGIN = MOD_DIR / 'd2rloader/plugins/d2rl-pcvr.dll'
SETTINGS = MOD_DIR / 'd2rloader/plugins/d2r_pcvr.ini'
# D2R's own mod format: <mod>\<mod>.mpq\modinfo.json; savepath "../" keeps the usual characters.
MODINFO = MOD_DIR / f'{MOD_NAME}.mpq/modinfo.json'
# The mod's own ReShade, loaded by the plugin with this folder as ReShade's base (its ini, preset, shaders,
# textures), and its OpenXR loader beside the plugin.
RESHADE_DIR = MOD_DIR / 'reshade'
RESHADE = RESHADE_DIR / 'ReShade64.dll'
OPENXR_LOADER = PLUGIN.parent / 'openxr_loader.dll'
# The new names in D2RLoader's global plugins folder (loaded with every mod), before the mod folder.
GLOBAL_PLUGIN = Path('d2rloader/plugins/d2rl-pcvr.dll')
GLOBAL_SETTINGS = Path('d2rloader/plugins/d2r_pcvr.ini')
GLOBAL_FILES = (GLOBAL_PLUGIN, GLOBAL_SETTINGS)
# What D2R PCVR installed up to 0.3.16, and what D2R VR 0.167 and earlier still installs.
LEGACY_PLUGIN = Path('d2rloader/plugins/d2rl-vrcam.dll')
LEGACY_SETTINGS = Path('d2rloader/plugins/d2r_vr.ini')
LEGACY_FILES = (LEGACY_PLUGIN, LEGACY_SETTINGS)
PLUGINS = (PLUGIN, GLOBAL_PLUGIN, LEGACY_PLUGIN)
# The same path as install.RECORD (install.py imports this module).
RECORD = Path('d2rloader/pcvr-install.json')

OTHER_MOD = 'd2r-vr'
OTHER_NAME = 'D2R VR'
# Only D2R VR's plugin carries these: its Game Link (every build since 0.157)
# and its skill icon cache folder (UTF-16).
OTHER_MARKS = (b'BodyWalk Game Link', 'd2r_vr_skill_icons'.encode('utf-16-le'))
# Only D2R PCVR's plugin carries these: its icon cache folder (UTF-16, every
# release) and, from this version on, its own plugin id.
OWN_MARKS = ('d2r_vr_icons'.encode('utf-16-le'), b'd2r-pcvr-vrcam')
# D2RLoader's own files (one D2RLoader serves every mod in the folder).
LOADER_NAMES = ('d2rloader.exe', 'd2rcore.dll', 'd2rloader.mpq')


def modinfo_bytes():
    """modinfo.json: "name" is the id D2R and D2RLoader find the mod by; the display name is in "description"."""
    return (json.dumps({'name': MOD_NAME, 'savepath': '../', 'description': f'{DISPLAY_NAME} - {TAGLINE}'},
                       indent=2) + '\n').encode('utf-8')


def loader_file(relative):
    r"""True for a file of D2RLoader itself (from its archive), not one of this mod's: the loader's exe, core and
    mpq, its d2rloader\ folder except the plugins, the runtime and this mod's record, and a mods\ folder other
    than this mod's."""
    parts = [part.lower() for part in Path(relative).parts]
    if len(parts) == 1:
        return parts[0] in LOADER_NAMES
    if parts[0] == 'd2rloader':
        return parts[1] not in ('plugins', 'runtime') and Path(relative).as_posix().lower() != RECORD.as_posix().lower()
    return parts[0] == 'mods' and parts[1] != MOD_NAME.lower()


def other_loader_users(game):
    r"""Other mods that start through this folder's D2RLoader: another mod folder with D2RLoader content (its
    d2rloader\ or modinfo), or a plugin in the global plugins folder that is not this mod's."""
    game = Path(game)
    users = []
    mods = game / 'mods'
    if mods.is_dir():
        for folder in mods.iterdir():
            if (folder.is_dir() and folder.name.lower() != MOD_NAME.lower()
                    and ((folder / 'd2rloader').is_dir() or (folder / 'modinfo.json').is_file()
                         or any(folder.glob('*.mpq/modinfo.json')))):
                users.append(folder.name)
    plugins = game / 'd2rloader/plugins'
    if plugins.is_dir():
        own = {GLOBAL_PLUGIN.name.lower()}
        for dll in plugins.glob('*.dll'):
            if dll.name.lower() not in own and plugin_owner(dll) != MOD_ID:
                users.append(dll.name)
    return users


def legacy_effect_file(relative):
    """Files formerly installed outside PCVR's mod folder; never an arbitrary game file."""
    parts = [part.lower() for part in Path(relative).parts]
    return (len(parts) == 1 and parts[0] in ('reshade64.dll', 'reshade.ini', 'd2r_frame_reshade.ini')
            or len(parts) > 1 and parts[0] in ('reshade-shaders', 'd2r_sky_ours'))


def shared_dependency(relative):
    """Dependencies an old snapshot must not restore/remove while another mod uses the game."""
    path = Path(relative)
    return (loader_file(path) or legacy_effect_file(path)
            or path.as_posix().lower() == 'openxr_loader.dll'
            or path.as_posix().lower().startswith('d2rloader/runtime/'))


def legacy_effect_files(game, roots=()):
    """Only files our earlier snapshots actually installed, bounded to the game directory."""
    game = Path(game).resolve()
    found = set()
    for manifest in _manifests(game, roots):
        for name in manifest['installed']:
            relative = Path(str(name).replace('\\', '/'))
            if relative.is_absolute() or '..' in relative.parts or not legacy_effect_file(relative):
                continue
            path = game / relative
            if not path.resolve().is_relative_to(game):
                raise ValueError(f'Legacy effect path leaves the game folder: {relative}')
            if path.is_file():
                found.add(relative)
    return sorted(found)


def plugin_owner(path):
    """MOD_ID, OTHER_MOD, 'unknown' (neither mod's marks) or None (no file)."""
    try:
        data = Path(path).read_bytes()
    except FileNotFoundError:
        return None
    except OSError:
        # There but unreadable (locked, blocked): not provably this mod's.
        return 'unknown' if Path(path).exists() else None
    if any(mark in data for mark in OTHER_MARKS):
        return OTHER_MOD
    if any(mark in data for mark in OWN_MARKS):
        return MOD_ID
    return 'unknown'


def _manifests(game, roots):
    """This mod's snapshot manifests for this game, oldest first: those named by
    the install record, and any found in the given backup folders."""
    folders = []
    record = game / RECORD
    if record.is_file():
        try:
            names = json.loads(record.read_text(encoding='utf-8')).get('snapshots', [])
        except (OSError, ValueError, AttributeError):
            names = []
        folders += [Path(name) for name in names if isinstance(name, str) and name]
    for root in roots:
        if root is None:
            continue
        root = Path(root)
        folders += [root] if (root / 'manifest.json').is_file() else (
            [source.parent for source in root.glob('*/manifest.json')] if root.is_dir() else [])
    found = {}
    for folder in folders:
        try:
            manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
            if Path(manifest['game']).resolve() != game.resolve() or not isinstance(manifest.get('installed'), list):
                continue
        except (OSError, ValueError, KeyError, TypeError):
            continue
        found[folder.resolve()] = manifest
    return [found[path] for path in sorted(found, key=lambda path: path.name)]


def _listed(manifest, relative):
    names = {str(name).replace('\\', '/').lower() for name in manifest['installed']}
    return relative.as_posix().lower() in names


def installed_legacy_files(game, roots=()):
    """True when this mod's own records say it put d2rl-vrcam.dll / d2r_vr.ini here.

    From this version on D2R PCVR never writes those names, so once its own
    d2rl-pcvr.dll is in the folder the old names are not its files. Otherwise the
    newest snapshot that installed a plugin decides; a record whose snapshots
    cannot be read was written by an earlier release (0.3.16 or older), all of
    which used the old names."""
    if (game / PLUGIN).is_file() or (game / GLOBAL_PLUGIN).is_file():
        return False
    manifests = [m for m in _manifests(game, roots) if any(_listed(m, p) for p in PLUGINS)]
    if manifests:
        return _listed(manifests[-1], LEGACY_PLUGIN)
    return (game / RECORD).is_file()


@dataclass(frozen=True)
class Survey:
    ours: bool       # D2R PCVR is installed here (new names, or old names that are its own)
    legacy: bool     # its old d2rl-vrcam.dll / d2r_vr.ini are here, to carry over and remove
    other: 'str | None'   # OTHER_MOD or 'unknown': a d2rl-vrcam.dll that is not D2R PCVR's
    message: str     # what to tell the player about `other` ('' when None)
    moved: bool = False   # its d2rl-pcvr.dll / d2r_pcvr.ini in the global plugins folder, to carry into the mod


def other_mod_message(owner):
    if owner == OTHER_MOD:
        return ('Another VR mod, D2R VR, is installed in this game folder '
                '(d2rloader\\plugins\\d2rl-vrcam.dll and d2r_vr.ini are its files). '
                'D2R PCVR leaves its files alone and does not install beside it: D2RLoader would start both mods in one game. '
                'Remove D2R VR with its own uninstaller first, then run this setup again.')
    return ('This game folder has a d2rloader\\plugins\\d2rl-vrcam.dll that D2R PCVR setup did not install '
            '(no D2R PCVR installation record lists it), so it may belong to D2R VR or to a copy installed by hand. '
            'Setup leaves it alone and does not install beside it: D2RLoader would start both. '
            'Remove it with its own uninstaller (or move it out of the plugins folder if you copied it there yourself), '
            'or select the backup of your earlier D2R PCVR installation, then run this setup again.')


def survey(game, roots=()):
    """What is in this game folder, read only."""
    game = Path(game)
    moved = any((game / name).is_file() for name in GLOBAL_FILES)
    new = (game / PLUGIN).is_file() or moved
    owner = plugin_owner(game / LEGACY_PLUGIN)
    ours_legacy = owner != OTHER_MOD and installed_legacy_files(game, roots)
    legacy = ours_legacy and any((game / name).is_file() for name in LEGACY_FILES)
    other = None
    if owner is not None and not ours_legacy:
        other = OTHER_MOD if owner == OTHER_MOD else 'unknown'
    return Survey(ours=new or legacy, legacy=legacy, moved=moved, other=other,
                  message=other_mod_message(other) if other else '')
