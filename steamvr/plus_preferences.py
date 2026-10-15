"""Shared Flat keyboard and D2RLoader language preferences."""
import json
from pathlib import Path
import re

LANGUAGES = {'': 'Default (game language)', 'enUS': 'English', 'deDE': 'German',
             'esES': 'Spanish (Spain)', 'frFR': 'French', 'itIT': 'Italian',
             'koKR': 'Korean', 'plPL': 'Polish', 'ruRU': 'Russian',
             'zhCN': 'Chinese (Simplified)', 'zhTW': 'Chinese (Traditional)',
             'esMX': 'Spanish (Latin America)', 'jaJP': 'Japanese', 'ptBR': 'Portuguese (Brazil)'}


def preferences(game):
    result = {'layout': 'WASD', 'text_locale': '', 'audio_locale': ''}
    for rel, keys in [('d2rloader/plugins/d2r_pcvr_plus_flat.ini', ['flat_layout']),
                      ('d2rloader/config/d2rloader.toml', ['text_locale', 'audio_locale'])]:
        path = Path(game) / rel
        if not path.is_file():
            continue
        for key in keys:
            matches = re.findall(r'^\s*' + key + r'\s*=\s*([^\r\n;#]*)', path.read_text(encoding='utf-8-sig'), re.M)
            if len(matches) == 1:
                value = matches[0].strip().strip('\"\'')
                if key == 'flat_layout' and value in ('WASD', 'ZQSD'):
                    result['layout'] = value
                elif key != 'flat_layout' and value in LANGUAGES:
                    result[key] = value
    return result


def replace_value(text, key, value, section=None):
    # Preserve comments, other settings and section placement; refuse ambiguity.
    pattern = re.compile(r'^(\s*' + re.escape(key) + r'\s*=\s*)[^\r\n]*', re.M)
    matches = list(pattern.finditer(text))
    if len(matches) > 1:
        raise ValueError('Duplicate configuration key: ' + key)
    if matches:
        return pattern.sub(lambda m: m[1] + value, text, count=1)
    if section is None:
        # TOML language keys belong at root, before any table.
        return key + ' = ' + value + '\n' + text
    header = re.search(r'^\[' + re.escape(section) + r'\]\s*$', text, re.M)
    if header:
        return text[:header.end()] + '\n' + key + '=' + value + text[header.end():]
    return text.rstrip() + '\n\n[' + section + ']\n' + key + '=' + value + '\n'


def preference_updates(game, choices, previous_loader=None):
    flat = Path(game) / 'd2rloader/plugins/d2r_pcvr_plus_flat.ini'
    toml = Path(game) / 'd2rloader/config/d2rloader.toml'
    content = previous_loader if previous_loader is not None else toml.read_text(encoding='utf-8-sig')
    for key in ('text_locale', 'audio_locale'):
        content = replace_value(content, key, json.dumps(choices[key]))
    return {flat: replace_value(flat.read_text(encoding='utf-8-sig'), 'flat_layout', choices['layout'], 'input').encode(),
            toml: content.encode()}

