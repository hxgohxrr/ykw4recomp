import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from launcher_core import LauncherEvents, graphics_info, graphics_summary
from desktop_launcher import Launcher


class LinuxPerformanceTests(unittest.TestCase):
    def device(self, name, kind, minor=3):
        return {'name': name, 'device_type': kind, 'api_version': 1 << 22 | minor << 12}

    def test_cpu_renderer_requires_software_status(self):
        result = graphics_summary([self.device('llvmpipe', 4)])
        self.assertEqual(result['status'], 'software')

    def test_hardware_not_hidden_by_cpu_adapter(self):
        for kind in (1, 2, 3):
            with self.subTest(kind=kind):
                result = graphics_summary([self.device('CPU', 4), self.device('GPU', kind)])
                self.assertEqual(result['status'], 'hardware')

    def test_old_hardware_does_not_hide_software_fallback(self):
        result = graphics_summary([self.device('old GPU', 2, 2), self.device('CPU', 4)])
        self.assertEqual(result['status'], 'software')
        self.assertEqual(graphics_summary([self.device('old GPU', 2, 2)])['status'], 'unavailable')
        self.assertEqual(graphics_summary([])['status'], 'unavailable')

    def test_graphics_probe_is_timed_and_isolated(self):
        with patch('launcher_core.subprocess.run') as run:
            run.return_value.stdout = json.dumps({'status': 'software', 'devices': []})
            self.assertEqual(graphics_info()['status'], 'software')
            self.assertEqual(run.call_args.args[0][-1], '--graphics-info')
            self.assertEqual(run.call_args.kwargs['timeout'], 5)
            self.assertNotIn('--game', run.call_args.args[0])

    def test_graphics_probe_failure_is_not_reported_as_hardware(self):
        for error in (OSError('no Vulkan'), subprocess.TimeoutExpired('probe', 5), subprocess.CalledProcessError(1, 'probe')):
            with patch('launcher_core.subprocess.run', side_effect=error):
                self.assertEqual(graphics_info()['status'], 'unavailable')
        for output in ('not JSON', '[]', '{"status":"invalid"}'):
            with patch('launcher_core.subprocess.run') as run:
                run.return_value.stdout = output
                self.assertEqual(graphics_info()['status'], 'unavailable')

    def test_log_overflow_preserves_control_and_recent_lines(self):
        events = LauncherEvents(log_limit=2, line_limit=8)
        for line in ('old', 'middle', 'latest message'):
            events.put(('log', line))
        callback = Mock()
        events.put(('callback', callback))
        events.put(('done', None))
        self.assertEqual(events.get_nowait(), ('callback', callback))
        self.assertEqual(events.get_nowait(), ('done', None))
        self.assertIn('1 mensajes', events.get_nowait()[1])
        self.assertEqual(events.get_nowait(), ('log', 'middle'))
        self.assertEqual(events.get_nowait(), ('log', 'latest m'))
        with self.assertRaises(queue.Empty):
            events.get_nowait()

    def test_log_buffer_is_bounded_with_threaded_producers(self):
        events = LauncherEvents(log_limit=10)
        def produce():
            for number in range(1000):
                events.put(('log', str(number)))
        workers = [threading.Thread(target=produce) for unused in range(4)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join()
        self.assertEqual(len(events.logs), 10)
        self.assertEqual(events.dropped, 3990)
        events.put(('error', 'must remain visible'))
        self.assertEqual(events.get_nowait(), ('error', 'must remain visible'))

    def launcher_fixture(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        launcher = Launcher.__new__(Launcher)
        launcher.dictionary = {}
        launcher.root = Mock()
        launcher.base = Path(folder.name)
        (launcher.base / 'YKW4.discord.ini').write_text('[Discord]\nenabled=0\n')
        launcher.runtime = launcher.base / 'runtime'
        launcher.ready = Mock(return_value=True)
        launcher.save = Mock(return_value=True)
        launcher.values = {'translation': Mock(get=Mock(return_value='original'))}
        launcher.events = LauncherEvents()
        launcher.destroyed = False
        self.addCleanup(setattr, launcher, 'destroyed', True)
        launcher.mods = []
        launcher.actions = []
        launcher.start = launcher.actions.append
        return launcher

    def test_declining_software_renderer_does_not_start_game(self):
        launcher = self.launcher_fixture()
        with patch('desktop_launcher.graphics_info', return_value={'status': 'software'}), patch('desktop_launcher.messagebox.askyesno', return_value=False) as ask, patch('desktop_launcher.subprocess.Popen') as launch:
            launcher.play()
            worker = threading.Thread(target=launcher.actions.pop(), daemon=True)
            worker.start()
            kind, callback = launcher.events.control.get(timeout=2)
            self.assertEqual(kind, 'callback')
            callback()
            worker.join(timeout=2)
            self.assertFalse(worker.is_alive())
            self.assertEqual(ask.call_args.kwargs['default'], 'no')
            launch.assert_not_called()

    def test_accepting_software_renderer_starts_game_once(self):
        launcher = self.launcher_fixture()
        with patch('desktop_launcher.graphics_info', return_value={'status': 'software'}), patch('desktop_launcher.messagebox.askyesno', return_value=True), patch('desktop_launcher.subprocess.Popen') as launch, patch('desktop_launcher.DiscordPresence'):
            launch.return_value.__enter__.return_value.stdout = iter(())
            launch.return_value.__enter__.return_value.wait.return_value = 0
            launcher.play()
            worker = threading.Thread(target=launcher.actions.pop(), daemon=True)
            worker.start()
            kind, callback = launcher.events.control.get(timeout=2)
            callback()
            worker.join(timeout=2)
            self.assertFalse(worker.is_alive())
            launch.assert_called_once()

    def test_hardware_path_does_not_prompt(self):
        launcher = self.launcher_fixture()
        with patch('desktop_launcher.graphics_info', return_value={'status': 'hardware'}), patch('desktop_launcher.messagebox.askyesno') as ask, patch('desktop_launcher.subprocess.Popen') as launch, patch('desktop_launcher.DiscordPresence'):
            launch.return_value.__enter__.return_value.stdout = iter(())
            launch.return_value.__enter__.return_value.wait.return_value = 0
            launcher.play()
            launcher.actions.pop()()
            ask.assert_not_called()
            launch.assert_called_once()

    def test_destroyed_launcher_does_not_start_after_pending_confirmation(self):
        launcher = self.launcher_fixture()
        with patch('desktop_launcher.graphics_info', return_value={'status': 'software'}), patch('desktop_launcher.subprocess.Popen') as launch:
            launcher.play()
            worker = threading.Thread(target=launcher.actions.pop(), daemon=True)
            worker.start()
            kind, callback = launcher.events.control.get(timeout=2)
            launcher.destroyed = True
            worker.join(timeout=2)
            self.assertFalse(worker.is_alive())
            launch.assert_not_called()

    def test_invalid_log_limits_are_rejected(self):
        for log_limit, line_limit in ((0, 10), (10, 0), (-1, 10)):
            with self.subTest(log_limit=log_limit, line_limit=line_limit):
                with self.assertRaises(ValueError):
                    LauncherEvents(log_limit=log_limit, line_limit=line_limit)

    def test_log_updates_are_batched_without_a_window(self):
        launcher = self.launcher_fixture()
        launcher.root.focus_get.return_value = None
        launcher.gamepad = Mock(poll=Mock(return_value=[]))
        launcher.activity_window = Mock(state=Mock(return_value='withdrawn'))
        launcher.log = Mock()
        launcher.log.index.return_value = '2500.0'
        for number in range(100):
            launcher.events.put(('log', str(number)))
        launcher.pump()
        launcher.log.insert.assert_called_once_with('end', '\n'.join(str(number) for number in range(100)) + '\n')
        launcher.log.delete.assert_called_once_with('1.0', '501.0')
        launcher.log.see.assert_not_called()
        launcher.root.after.assert_called_once()


if __name__ == '__main__':
    unittest.main()
