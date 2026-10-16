"""Experimental branch regression checks; not hardware/performance certification.

The release manifest stays unchanged until a deliberate hardware-validated release.
Only the listed implementation adapters are permitted to differ from it here.
The panel_input adapter adds a read-only native UI-mode observer for gamepads.
"""
import hashlib
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent.parent
ADAPTERS={'steamvr/native_panels_test.cpp','vr/hud.cpp','vr/gamestate.cpp','vr/journal_ui.h','steamvr/panel_input_mode.h','cleanroom/camera/d2rcam.cpp','cleanroom/camera/d2rcam.h',
          'vr/panel_input.cpp','vr/panel_input.h','steamvr/transport.cpp','steamvr/transport.h','vr/dlss_mv.cpp','vr/dlss_mv.h'}
BLOCKS=[('steamvr/transport.cpp', 'bool View(float out[16],const float hero[3]', 'void Shutdown()', '95f62721f7751be019d4f2d5d157679eff76b8642bbd56778a067d4e890b0db6'), ('vr/vrcam.cpp', '    const bool nativeFirst =', '    const float hangA[3]', 'f087d80214b748cbdf4e9c78f3336b2ea7bc0d6bcfb0ee88c90c03fc55eb5d85')]
class NativePerspectiveRegression(unittest.TestCase):
    def test_unmodified_runtime_dependencies(self):
        manifest=json.loads((ROOT/'steamvr/vr_baseline.json').read_text())
        for name,expected in manifest['unchanged_sources'].items():
            if name in ADAPTERS: continue
            with self.subTest(source=name):
                data=(ROOT/name).read_bytes().replace(b'\r\n',b'\n')
                self.assertEqual(hashlib.sha256(data).hexdigest(),expected)
    def test_first_person_eye_bow_and_projection_math_preserved(self):
        for name,start,end,expected in BLOCKS:
            with self.subTest(source=name):
                source=(ROOT/name).read_text()
                body=source[source.index(start):source.index(end,source.index(start))]
                self.assertEqual(hashlib.sha256(body.encode()).hexdigest(),expected)
if __name__=='__main__':unittest.main()
