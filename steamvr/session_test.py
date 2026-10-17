"""Real subprocess checks for cleanup ownership and normal exit behavior."""
import subprocess
import sys
import unittest
from unittest import mock
import session
from session import run


@unittest.skipUnless(sys.platform.startswith("linux"), "Proton process supervision uses Linux /proc")
class SessionTest(unittest.TestCase):
    def test_normal_exit(self):
        self.assertEqual(run([sys.executable, '-c', 'raise SystemExit(7)'], poll=.02), 7)

    def test_game_exit_cleans_lingering_wrapper(self):
        # argv[0] mimics Wine's Windows game executable without launching D2R.
        code = ('import subprocess,sys,time; '
                'p=subprocess.Popen([r"S:\\game\\D2RLoader.exe", "-c", "import time;time.sleep(.3)"], '
                'executable=sys.executable); p.wait(); time.sleep(30)')
        unrelated = subprocess.Popen([sys.executable, '-c', 'import time;time.sleep(30)'])
        try:
            self.assertEqual(run([sys.executable, '-c', code], poll=.02, grace=.1), 0)
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate()
            unrelated.wait(timeout=3)

    def test_no_game_does_not_force_cleanup(self):
        self.assertEqual(run([sys.executable, '-c', 'import time;time.sleep(.2);raise SystemExit(9)'], poll=.02, grace=.02), 9)

    def test_steady_game_does_not_scan_every_process_each_poll(self):
        code = ('import subprocess,sys,time; '
                'p=subprocess.Popen([r"S:\\game\\D2R.exe", "-c", "import time;time.sleep(.5)"], '
                'executable=sys.executable); p.wait(); time.sleep(30)')
        with mock.patch('session.processes', wraps=session.processes) as scan:
            self.assertEqual(run([sys.executable, '-c', code], poll=.01, grace=.03), 0)
            self.assertLess(scan.call_count, 15)  # >50 polls; startup/exit still scanned

    def test_pid_reuse_and_exec_are_not_treated_as_the_game(self):
        old = {123: (1, '100', 'S', 'd2r.exe')}
        for current in ((1, '101', 'S', 'd2r.exe'), (1, '100', 'S', 'helper'),
                        (1, '100', 'Z', 'd2r.exe'), None):
            with mock.patch('session.process_info', return_value=current):
                self.assertEqual(session.observed_games(old), {})


if __name__ == '__main__':
    unittest.main()
