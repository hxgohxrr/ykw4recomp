from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from desktop_launcher import Launcher
from launcher_core import Mod


class PendingSettingsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.launcher = Launcher.__new__(Launcher)
        self.launcher.base = Path(self.temporary.name)
        self.launcher.runtime = self.launcher.base / 'runtime'
        self.launcher.root = Mock()
        self.launcher.busy = False
        self.launcher.ready = Mock(return_value=True)
        self.launcher.values = {'fps_limit': Mock(get=Mock(return_value='30')),
                                'translation': Mock(get=Mock(return_value='original'))}
        self.launcher.rpc = {'details': Mock(get=Mock(return_value='saved'))}
        self.launcher.mods = []
        self.launcher.lang = Mock(get=Mock(return_value='es'))
        self.launcher.dictionary = {}
        self.launcher.saved_settings = self.launcher.settings_snapshot()
        self.launcher.start = Mock()
        self.launcher.save = Mock(return_value=True)
        (self.launcher.base / 'YKW4.ini').write_text('[Launcher]\ntranslation=original\n[Performance]\nfps_limit=30\n')

    def edit(self):
        self.launcher.values['fps_limit'].get.return_value = '60'

    @patch('desktop_launcher.messagebox.askyesnocancel')
    def test_unchanged_and_reverted_settings_do_not_prompt_or_save(self, prompt):
        self.edit()
        self.launcher.values['fps_limit'].get.return_value = '30'
        self.launcher.play()
        prompt.assert_not_called()
        self.launcher.save.assert_not_called()
        self.launcher.start.assert_called_once()

    @patch('desktop_launcher.messagebox.askyesnocancel', return_value=True)
    def test_yes_applies_then_starts(self, prompt):
        self.edit()
        self.launcher.play()
        self.launcher.save.assert_called_once()
        self.launcher.start.assert_called_once()
        self.assertEqual(prompt.call_args.kwargs['default'], 'cancel')

    @patch('desktop_launcher.messagebox.askyesnocancel', return_value=True)
    def test_save_failure_does_not_start(self, prompt):
        self.edit()
        self.launcher.save.return_value = False
        self.launcher.play()
        self.launcher.start.assert_not_called()

    @patch('desktop_launcher.messagebox.askyesnocancel', return_value=None)
    def test_cancel_does_not_save_or_start_or_discard_edits(self, prompt):
        self.edit()
        self.launcher.play()
        self.launcher.save.assert_not_called()
        self.launcher.start.assert_not_called()
        self.assertEqual(self.launcher.values['fps_limit'].get(), '60')

    @patch('desktop_launcher.messagebox.askyesnocancel', return_value=False)
    def test_no_launches_saved_translation_without_saving(self, prompt):
        self.launcher.values['translation'].get.return_value = 'es'
        before = (self.launcher.base / 'YKW4.ini').read_bytes()
        self.launcher.play()
        self.launcher.save.assert_not_called()
        self.launcher.start.assert_called_once()
        with patch('desktop_launcher.graphics_info', return_value={'status': 'hardware'}), \
             patch('desktop_launcher.subprocess.Popen') as process, \
             patch('desktop_launcher.DiscordPresence'):
            process.return_value.__enter__.return_value.stdout = []
            process.return_value.__enter__.return_value.wait.return_value = 0
            self.launcher.start.call_args.args[0]()
            self.assertEqual(process.call_args.kwargs['env']['YKW4_LANGUAGE'], 'original')
        self.assertEqual((self.launcher.base / 'YKW4.ini').read_bytes(), before)

    @patch('desktop_launcher.messagebox.askyesnocancel', return_value=None)
    def test_discord_language_and_mod_order_are_pending(self, prompt):
        self.launcher.rpc['details'].get.return_value = 'edited'
        self.launcher.play()
        self.assertEqual(prompt.call_count, 1)
        self.launcher.saved_settings = self.launcher.settings_snapshot()
        self.launcher.lang.get.return_value = 'en'
        self.launcher.play()
        self.assertEqual(prompt.call_count, 2)
        self.launcher.mods = [Mod('first', 'First'), Mod('second', 'Second')]
        self.launcher.saved_settings = self.launcher.settings_snapshot()
        self.launcher.mods.reverse()
        self.launcher.play()
        self.assertEqual(prompt.call_count, 3)
        self.launcher.saved_settings = self.launcher.settings_snapshot()
        self.launcher.mods[0].enabled = True
        self.launcher.play()
        self.assertEqual(prompt.call_count, 4)

    @patch('desktop_launcher.messagebox.askyesnocancel')
    def test_busy_or_unprepared_never_prompts(self, prompt):
        self.edit()
        self.launcher.busy = True
        self.launcher.play()
        self.launcher.busy = False
        self.launcher.ready.return_value = False
        self.launcher.play()
        prompt.assert_not_called()
        self.launcher.start.assert_not_called()


if __name__ == '__main__':
    unittest.main()
