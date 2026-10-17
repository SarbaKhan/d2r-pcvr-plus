"""Read-only Windows desktop HDR detection. No display settings are changed.

Uses QueryDisplayConfig and DisplayConfigGetDeviceInfo (Windows SDK wingdi.h).
Windows 11's newer color query distinguishes HDR from SDR wide-color mode.
"""
import ctypes as c
from dataclasses import dataclass
import os

U32 = c.c_uint32
I32 = c.c_int32

class Luid(c.Structure):
    _fields_ = [('low', U32), ('high', I32)]

class Source(c.Structure):
    _fields_ = [('adapter', Luid), ('id', U32), ('mode', U32), ('flags', U32)]

class Target(c.Structure):
    _fields_ = [('adapter', Luid), ('id', U32), ('mode', U32), ('technology', U32),
                ('rotation', U32), ('scaling', U32), ('refresh_num', U32),
                ('refresh_den', U32), ('scanline', U32), ('available', I32), ('flags', U32)]

class DisplayPath(c.Structure):
    _fields_ = [('source', Source), ('target', Target), ('flags', U32)]

class Mode(c.Structure):
    # DISPLAYCONFIG_MODE_INFO's 48-byte union is not inspected. uint64 keeps
    # the alignment of its native VIDEO_SIGNAL_INFO member's pixelRate.
    _fields_ = [('type', U32), ('id', U32), ('adapter', Luid), ('data', c.c_uint64 * 6)]

class Header(c.Structure):
    _fields_ = [('type', U32), ('size', U32), ('adapter', Luid), ('id', U32)]

class ColorInfo(c.Structure):
    _fields_ = [('header', Header), ('flags', U32), ('encoding', U32), ('bits', U32)]

class ColorInfo2(c.Structure):
    _fields_ = ColorInfo._fields_ + [('active_mode', U32)]

class TargetName(c.Structure):
    _fields_ = [('header', Header), ('flags', U32), ('technology', U32),
                ('manufacturer', c.c_uint16), ('product', c.c_uint16), ('connector', U32),
                ('name', c.c_wchar * 64), ('path', c.c_wchar * 128)]

@dataclass
class Display:
    name: str
    hdr: bool | None

def request(kind, type_id, target):
    value = kind()
    value.header = Header(type_id, c.sizeof(kind), target.adapter, target.id)
    return value

def color_state(get_info, target):
    modern = request(ColorInfo2, 15, target)
    if get_info(c.byref(modern)) == 0:
        return modern.active_mode == 2  # HDR, not SDR (0) or WCG (1).
    legacy = request(ColorInfo, 9, target)
    if get_info(c.byref(legacy)) == 0:
        return bool(legacy.flags & 2) and not bool(legacy.flags & 4)
    return None

def query(api=None):
    if api is None:
        if os.name != 'nt':
            return []
        api = c.WinDLL('user32')
        api.GetDisplayConfigBufferSizes.argtypes = [U32, c.POINTER(U32), c.POINTER(U32)]
        api.GetDisplayConfigBufferSizes.restype = I32
        api.QueryDisplayConfig.argtypes = [U32, c.POINTER(U32), c.POINTER(DisplayPath),
            c.POINTER(U32), c.POINTER(Mode), c.c_void_p]
        api.QueryDisplayConfig.restype = I32
        api.DisplayConfigGetDeviceInfo.argtypes = [c.c_void_p]
        api.DisplayConfigGetDeviceInfo.restype = I32
    for _ in range(3):  # A monitor may connect/disconnect between the two calls.
        count, modes = U32(), U32()
        status = api.GetDisplayConfigBufferSizes(2, c.byref(count), c.byref(modes))
        if status:
            raise OSError(status, 'Cannot query active displays')
        if not count.value:
            return []
        if count.value > 1024 or modes.value > 4096:
            raise OSError('Unexpected display configuration size')
        paths = (DisplayPath * count.value)()
        mode_buffer = (Mode * max(1, modes.value))()
        status = api.QueryDisplayConfig(2, c.byref(count), paths, c.byref(modes), mode_buffer, None)
        if status == 122:  # ERROR_INSUFFICIENT_BUFFER: retry fresh sizes.
            continue
        if status:
            raise OSError(status, 'Cannot read active displays')
        result, seen = [], set()
        for path in paths[:count.value]:
            target = path.target
            key = (target.adapter.low, target.adapter.high, target.id)
            if not path.flags & 1 or not target.available or key in seen:
                continue
            seen.add(key)
            name = request(TargetName, 2, target)
            good_name = api.DisplayConfigGetDeviceInfo(c.byref(name)) == 0 and bool(name.name)
            label = name.name if good_name else f'Display {len(result) + 1}'
            result.append(Display(label, color_state(api.DisplayConfigGetDeviceInfo, target)))
        return result
    raise OSError('Display configuration changed repeatedly')

def check():
    try:
        displays = query()
    except (OSError, AttributeError):
        displays = []
    enabled = [d.name for d in displays if d.hdr is True]
    unknown = not displays or any(d.hdr is None for d in displays)
    status = 'on' if enabled else 'unknown' if unknown else 'off'
    if enabled:
        warning = ('Windows HDR is on: ' + ', '.join(enabled) + '. '
            'Players have reported visual problems in VR that improve with HDR off. '
            'Before playing, open Windows Settings > System > Display > HDR, select the affected display, '
            'and turn Use HDR Off. You can continue setup; it will not change your display settings.')
    elif unknown:
        warning = ('Setup could not check Windows HDR on every active display. '
            'If VR colors or brightness look wrong, check Windows Settings > System > Display > HDR '
            'and try Use HDR Off. You can continue setup.')
    else:
        warning = ''
    # Probe files and console logs use a single line per value.
    return {'hdr_status': status, 'hdr_warning': ' '.join(warning.split())}
