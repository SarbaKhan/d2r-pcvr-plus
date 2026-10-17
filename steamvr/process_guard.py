"""Standard-library-only game process guard."""
import os
from pathlib import Path
import subprocess


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
