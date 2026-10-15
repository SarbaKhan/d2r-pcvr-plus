"""Desktop executable entry point; no Steam shortcut or library handoff."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
MODES = ('flat', 'steamvr', 'vdxr')


def arguments(config, mode):
    if mode not in MODES or mode not in config['modes']:
        raise ValueError('This mode was not installed. Run Setup and select it first.')
    game = Path(config['game'])
    args = ['--game', str(game), '--store', config['store'], '--mode', 'flat' if mode == 'flat' else 'vr']
    if config['store'] == 'battlenet':
        args.append('--experimental-battlenet')
    if mode == 'steamvr':
        args += ['--openxr-runtime', config['steamvr_runtime']]
    elif mode == 'vdxr':
        args += ['--vdxr']
    return args


def main(mode):
    from launch_plus import main as launch
    config = json.loads((ROOT / 'direct-launch.json').read_text(encoding='utf-8'))
    return launch(arguments(config, mode))


if __name__ == '__main__':
    try:
        raise SystemExit(main(sys.argv[1]))
    except (OSError, ValueError, KeyError, IndexError) as error:
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, str(error), 'D2R PCVR Plus launcher', 0x10)
        raise SystemExit(1)
