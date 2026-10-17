"""Protect the upstream controls explicitly requested for this integration."""
import hashlib
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
# Exact input modules from upstream 772d337; Plus adds perspective ownership
# outside these modules and must not reintroduce its old run/walk command.
UPSTREAM_CONTROLS = {'vr/gamecmd.cpp': '4501b1256aa3fc085b86567b149b4c265eb869bfbf39e77e2b96ba07de9ac9a3', 'steamvr/controller_bindings.h': '8905f7ebd05f7fb5c9a67460f12300938f9a5a402e658ac17a5c643329db76d9', 'steamvr/hand_controls.h': 'ed9eb4d3c8af4529b64e00f0e5ba7e3d1fafda4eae6801f5987dca780a4e78c0', 'steamvr/skill_pad.h': '3e5affbd068adb015d8ab7c9ee280ceb704818175c80e0eaaca7894098617001', 'steamvr/quick_cast.h': '12af85be4e1a8f54d96892b642d02386fd07f844ff237cf9e838183f77585fdf', 'steamvr/interact_press.h': '42ec7737657fb5705427624ef9ba5f0e8e657a5650577e068e648a890d7162fd'}

class UpstreamControlTests(unittest.TestCase):
    def test_original_control_modules_are_unchanged(self):
        for path, expected in UPSTREAM_CONTROLS.items():
            with self.subTest(path=path):
                self.assertEqual(hashlib.sha256((ROOT / path).read_bytes().replace(b"\r\n", b"\n")).hexdigest(), expected)

    def test_no_plus_right_stick_run_toggle(self):
        source = (ROOT / 'steamvr/transport.cpp').read_text(encoding='utf-8')
        self.assertNotIn('RequestRunToggle', source)
        self.assertNotIn('runButton', source)

if __name__ == '__main__':
    unittest.main()
