"""Desktop HDR queries using fake Win32 responses; never change display state."""
import ctypes as c
import os
import unittest
from unittest.mock import patch
import hdr_check as hdr

class FakeDisplays:
    def __init__(self, entries, retries=0):
        self.entries = entries
        self.retries = retries
        self.calls = 0

    def GetDisplayConfigBufferSizes(self, flags, count, modes):
        assert flags == 2  # Active desktop paths; no headset/hidden paths.
        count._obj.value = len(self.entries)
        modes._obj.value = 1
        return 0

    def QueryDisplayConfig(self, flags, count, paths, modes, mode_buffer, topology):
        self.calls += 1
        if self.calls <= self.retries:
            return 122
        for i, item in enumerate(self.entries):
            paths[i].flags = item.get('active', 1)
            paths[i].target.available = item.get('available', 1)
            paths[i].target.id = item.get('id', i)
        return 0

    def DisplayConfigGetDeviceInfo(self, pointer):
        info = pointer._obj
        item = next(entry for i, entry in enumerate(self.entries) if entry.get('id', i) == info.header.id)
        if info.header.type == 2:
            info.name = item.get('name', 'Fixture display')
            return 0
        if info.header.type == 15 and 'modern' in item:
            info.active_mode = item['modern']
            return 0
        if info.header.type == 9 and 'legacy' in item:
            info.flags = item['legacy']
            return 0
        return 87

class HdrTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows wchar size')
    def test_windows_sdk_buffer_sizes(self):
        self.assertEqual(c.sizeof(hdr.DisplayPath), 72)
        self.assertEqual(c.sizeof(hdr.Mode), 64)
        self.assertEqual(c.sizeof(hdr.TargetName), 420)
        self.assertEqual(c.sizeof(hdr.ColorInfo), 32)
        self.assertEqual(c.sizeof(hdr.ColorInfo2), 36)

    def test_mixed_displays_distinguish_hdr_from_sdr_and_wide_color(self):
        api = FakeDisplays([{'name': 'SDR', 'modern': 0}, {'name': 'Wide', 'modern': 1},
                            {'name': 'HDR', 'modern': 2}])
        displays = hdr.query(api)
        self.assertEqual([d.hdr for d in displays], [False, False, True])
        with patch.object(hdr, 'query', return_value=displays):
            result = hdr.check()
        self.assertEqual(result['hdr_status'], 'on')
        self.assertIn('Windows HDR is on: HDR.', result['hdr_warning'])

    def test_legacy_supported_only_is_not_enabled_and_wcg_is_not_hdr(self):
        displays = hdr.query(FakeDisplays([{'legacy': flag} for flag in (0, 1, 3, 7)]))
        self.assertEqual([d.hdr for d in displays], [False, False, True, False])

    def test_inactive_unavailable_and_duplicate_targets_are_skipped(self):
        api = FakeDisplays([{'id': 1, 'modern': 2}, {'id': 1, 'modern': 2},
            {'id': 2, 'active': 0, 'modern': 2}, {'id': 3, 'available': 0, 'modern': 2}])
        self.assertEqual(len(hdr.query(api)), 1)

    def test_topology_change_is_retried_and_bounded(self):
        api = FakeDisplays([{'modern': 2}], retries=1)
        self.assertTrue(hdr.query(api)[0].hdr)
        self.assertEqual(api.calls, 2)
        with self.assertRaises(OSError):
            hdr.query(FakeDisplays([{'modern': 2}], retries=3))

    def test_unknown_queries_do_not_claim_hdr_is_off_or_block_setup(self):
        displays = hdr.query(FakeDisplays([{'name': 'Unavailable query'}]))
        self.assertIsNone(displays[0].hdr)
        for value in (displays, []):
            with patch.object(hdr, 'query', return_value=value):
                result = hdr.check()
            self.assertEqual(result['hdr_status'], 'unknown')
            self.assertIn('continue setup', result['hdr_warning'])
        with patch.object(hdr, 'query', side_effect=OSError('not supported')):
            self.assertEqual(hdr.check()['hdr_status'], 'unknown')

    def test_sdr_only_is_quiet(self):
        with patch.object(hdr, 'query', return_value=[hdr.Display('SDR monitor', False)]):
            self.assertEqual(hdr.check(), {'hdr_status': 'off', 'hdr_warning': ''})

if __name__ == '__main__':
    unittest.main()
