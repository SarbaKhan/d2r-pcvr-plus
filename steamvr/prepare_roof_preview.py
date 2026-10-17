"""Prepare a local Kurast roof-retention experiment in the isolated pcvr-dev mod.

Reads the owner's game archives; generated presets must never be distributed.
This is a manual developer experiment, not part of setup or normal launching.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

from game_assets import open_assets
from game_install import validate_game
from install import require_game_closed

PRESETS = 'data/hd/env/preset/act3/kurast/'
MODELS = 'data/hd/env/model/act3/kurast/'
MOD_INFO = b'{"name":"pcvr-dev","savepath":"pcvr-dev/"}\n'
RECEIPT = 'pcvr-roof-preview.json'
MAX_PRESET = 16 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def preset_path(name):
    # Archive and receipt paths must be flat Kurast presets, never arbitrary paths.
    return isinstance(name, str) and re.fullmatch(
        re.escape(PRESETS) + r'[a-z0-9_]+\.json', name) is not None


def roof_model(name):
    if not isinstance(name, str):
        return False
    name = name.replace('\\', '/').lower()
    return re.fullmatch(re.escape(MODELS) +
        r'(act3_kurast_roof/roof_[a-z0-9_]+|act3_kurast_slums/slums_(hut_)?roof[a-z0-9_]*)\.model', name) is not None


def retain_roofs(preset):
    """Detach only entities whose entire model selection consists of roof pieces.

    Keep all transforms, models, physics, walls, non-roof fades and dependencies.
    Name matching on the entity itself would also select non-roof decorations.
    """
    if not isinstance(preset, dict) or preset.get('type') != 'Preset':
        raise ValueError('Expected an HD Preset object.')
    result = copy.deepcopy(preset)
    changed = 0
    for entity in result.get('entities', []):
        components = entity.get('components', [])
        models = []
        for component in components:
            if component.get('type') == 'ModelDefinitionComponent':
                models.append(component.get('filename'))
            elif component.get('type') == 'ModelVariationDefinitionComponent':
                variations = component.get('variations', [])
                models.extend(v.get('filename') for v in variations)
                if not variations:
                    models.append(None)
        if models and all(roof_model(model) for model in models):
            keep = [c for c in components if c.get('type') != 'FadeTargetComponent']
            if len(keep) != len(components):
                entity['components'] = keep
                changed += 1
    return result, changed


def checked_root(game):
    game = Path(game).resolve()
    root = game / 'mods/pcvr-dev/pcvr-dev.mpq'
    checked_path(root, 'modinfo.json')
    if (root / 'modinfo.json').read_bytes() != MOD_INFO:
        raise ValueError('Requires the existing isolated pcvr-dev mod and save path.')
    return root


def checked_path(root, name):
    path = root / name
    if path.resolve() != path or root.resolve() != root:
        raise ValueError('Preview paths must not contain symlinks or junctions.')
    return path


def generate(assets):
    generated = {}
    for name in sorted(assets.files):
        if not preset_path(name):
            continue
        source = assets.read(name)
        if len(source) > MAX_PRESET:
            raise ValueError(f'Unexpectedly large preset: {name}')
        result, count = retain_roofs(json.loads(source))
        if count:
            data = (json.dumps(result, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
            generated[name] = (data, {'source_sha256': digest(source),
                                     'sha256': digest(data), 'roof_entities': count})
    if not generated:
        raise ValueError('No Kurast roof fade links found; no files installed.')
    return generated


def install_preview(root, generated):
    receipt = checked_path(root, RECEIPT)
    if receipt.exists():
        raise ValueError('A preview receipt already exists; remove that preview first.')
    for name, (data, info) in generated.items():
        if not preset_path(name) or digest(data) != info['sha256']:
            raise ValueError('Invalid generated preset.')
        if checked_path(root, name).exists():
            raise ValueError(f'Existing override left untouched: {name}')
    # Record ownership before installing files; a later interrupted copy leaves
    # only complete targets, which --remove can verify and remove.
    record = {'format': 1, 'files': {name: info for name, (_, info) in generated.items()}}
    with receipt.open('x', encoding='utf-8') as output:
        json.dump(record, output, indent=2)
        output.write('\n')
    for name, (data, _) in generated.items():
        path = checked_path(root, name)
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.pcvr-roof-', delete=False) as output:
                temporary = Path(output.name)
                output.write(data)
            # On Windows rename refuses an existing destination, unlike replace.
            # The script is a Windows/Steam developer helper.
            if path.exists():
                raise ValueError(f'Existing override left untouched: {name}')
            temporary.rename(path)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        if digest(path.read_bytes()) != generated[name][1]['sha256']:
            raise OSError(f'Preview copy verification failed: {name}')
    return record


def remove_preview(root):
    receipt = checked_path(root, RECEIPT)
    if not receipt.exists():
        return 0
    record = json.loads(receipt.read_text(encoding='utf-8'))
    if record.get('format') != 1 or not isinstance(record.get('files'), dict):
        raise ValueError('Unrecognized roof-preview receipt.')
    paths = []
    for name, info in record['files'].items():
        if not preset_path(name):
            raise ValueError('Invalid roof-preview receipt path.')
        path = checked_path(root, name)
        if path.exists():
            if digest(path.read_bytes()) != info.get('sha256'):
                raise ValueError(f'Preview was modified; left untouched: {name}')
            paths.append(path)
    # No recursive deletion. Keep unrelated files, directories and mod metadata.
    for path in paths:
        path.unlink()
    receipt.unlink()
    return len(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--remove', action='store_true')
    args = parser.parse_args()
    try:
        require_game_closed()
        root = checked_root(args.game)
        if args.remove:
            print(f'Removed {remove_preview(root)} roof-preview overrides.')
            return
        # First experiment is Steam-only; its existing dev launch path is verified.
        validate_game(args.game, 'steam')
        with open_assets(args.game) as assets:
            generated = generate(assets)
        require_game_closed()
        root = checked_root(args.game)
        record = install_preview(root, generated)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'{error}\n')
    roofs = sum(info['roof_entities'] for info in record['files'].values())
    print(f'Installed {len(record["files"])} local presets retaining {roofs} roof pieces.')
    print('Use the isolated developer shortcut (-mod pcvr-dev), then DEV > Scenic Locations > Kurast Bazaar.')
    print('This preview requires a game restart. Roof visibility in the headset is not yet verified.')


if __name__ == '__main__':
    main()
