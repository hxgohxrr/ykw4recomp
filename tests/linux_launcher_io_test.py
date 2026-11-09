import copy
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import launcher_core as core
from desktop_launcher import Launcher


class LinuxLauncherIOTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)

    def test_unchanged_ini_preserves_bytes_and_does_not_sync(self):
        for encoding in ('utf-8', 'utf-8-sig', 'utf-16'):
            with self.subTest(encoding=encoding):
                path = self.base / (encoding + '.ini')
                original = '; keep\r\n[Graphics]\r\nresolution=0.5\r\nextra=unchanged\r\n'.encode(encoding)
                path.write_bytes(original)
                with patch.object(core, 'atomic_write', wraps=core.atomic_write) as write:
                    core.update_ini(path, {'Graphics': {'resolution': '0.5'}})
                    write.assert_not_called()
                self.assertEqual(path.read_bytes(), original)

    def test_changed_ini_is_still_atomic_and_durable(self):
        path = self.base / 'YKW4.ini'
        core.update_ini(path, {'Graphics': {'resolution': '1'}})
        with patch.object(core.os, 'fsync', wraps=core.os.fsync) as sync, patch.object(core.os, 'replace', wraps=core.os.replace) as replace:
            core.update_ini(path, {'Graphics': {'resolution': '0.5'}})
            sync.assert_called_once()
            replace.assert_called_once()
        self.assertEqual(core.read_ini(path)['Graphics']['resolution'], '0.5')

    def test_absent_empty_ini_is_created(self):
        path = self.base / 'new.ini'
        core.update_ini(path, {})
        self.assertEqual(path.read_bytes(), b'')

    def test_settings_and_mods_share_one_ini_write(self):
        mods = [core.Mod('first', 'First', enabled=True)]
        sections = {'Graphics': {'resolution': '0.5'}, 'Mods': {'unknown': 'keep'}}
        original = copy.deepcopy(sections)
        with patch.object(core, 'atomic_write', wraps=core.atomic_write) as write:
            core.save_mods(self.base, mods, sections=sections)
        self.assertEqual([Path(call.args[0]).name for call in write.call_args_list], ['YKW4.ini', 'active.txt'])
        self.assertEqual(core.read_ini(self.base / 'YKW4.ini')['Mods']['unknown'], 'keep')
        self.assertEqual(sections, original)
        self.assertEqual((self.base / 'mods/active.txt').read_text(), 'first\n')

    def test_repeated_mod_save_has_no_syncs_but_changes_persist(self):
        mods = [core.Mod('first', 'First', enabled=True)]
        core.save_mods(self.base, mods)
        with patch.object(core.os, 'fsync', wraps=core.os.fsync) as sync:
            core.save_mods(self.base, mods)
            sync.assert_not_called()
        mods[0].enabled = False
        with patch.object(core.os, 'fsync', wraps=core.os.fsync) as sync:
            core.save_mods(self.base, mods)
            self.assertEqual(sync.call_count, 2)
        self.assertEqual((self.base / 'mods/active.txt').read_bytes(), b'')

    @unittest.skipIf(os.name == 'nt', 'POSIX symlink fixture')
    def test_matching_config_links_are_replaced_not_retained(self):
        source = self.base / 'external.ini'
        source.write_text('[Graphics]\nresolution=1\n')
        target = self.base / 'YKW4.ini'
        target.symlink_to(source)
        core.update_ini(target, {'Graphics': {'resolution': '1'}})
        self.assertFalse(target.is_symlink())
        (self.base / 'mods').mkdir()
        active = self.base / 'mods/active.txt'
        empty = self.base / 'external.txt'
        empty.touch()
        active.symlink_to(empty)
        core.save_mods(self.base, [])
        self.assertFalse(active.is_symlink())
        self.assertEqual(source.read_text(), '[Graphics]\nresolution=1\n')

    def test_only_visible_mod_metadata_is_loaded_in_priority_order(self):
        identifiers = [f'mod-{index:03}' for index in range(260)]
        for identifier in identifiers:
            folder = self.base / 'mods' / identifier
            (folder / 'romfs').mkdir(parents=True)
            (folder / 'mod.ini').write_text('[Mod]\nname=' + identifier + '\ntranslation=1\n')
        priority = list(reversed(identifiers))
        core.update_ini(self.base / 'YKW4.ini', {'Mods': {'order': ','.join(core.mod_key(identifier) for identifier in priority), core.mod_key(priority[0]): '1'}})
        with patch.object(core, 'read_ini', wraps=core.read_ini) as read:
            mods = core.scan_mods(self.base)
        self.assertEqual([mod.identifier for mod in mods], priority[:128])
        self.assertEqual(sum(Path(call.args[0]).name == 'mod.ini' for call in read.call_args_list), 128)
        self.assertTrue(mods[0].enabled and mods[0].translation)

    def test_duplicate_priority_and_unordered_mods_keep_previous_order(self):
        for identifier in ('a', 'b', 'c', 'd'):
            (self.base / 'mods' / identifier / 'romfs').mkdir(parents=True)
        priority = ','.join(core.mod_key(identifier) for identifier in ('c', 'b', 'c'))
        core.update_ini(self.base / 'YKW4.ini', {'Mods': {'order': priority, 'b': '1'}})
        mods = core.scan_mods(self.base)
        self.assertEqual([mod.identifier for mod in mods], ['c', 'b', 'a', 'd'])
        self.assertTrue(mods[1].enabled)

    @unittest.skipIf(os.name == 'nt', 'POSIX symlink fixture')
    def test_mod_links_are_still_excluded(self):
        external = self.base / 'external'
        (external / 'romfs').mkdir(parents=True)
        root = self.base / 'mods'
        root.mkdir()
        (root / 'linked').symlink_to(external, target_is_directory=True)
        (root / 'payload').mkdir()
        (root / 'payload/romfs').symlink_to(external / 'romfs', target_is_directory=True)
        self.assertEqual(core.scan_mods(self.base), [])

    def test_launcher_save_uses_grouped_settings(self):
        launcher = Launcher.__new__(Launcher)
        launcher.base = self.base
        launcher.busy = False
        launcher.dictionary = {}
        launcher.lang = Mock(get=Mock(return_value='en'))
        values = dict(fps='60', deadzone='15', rumble='100', resolution='1', fullscreen='0', vsync='1', shadows='1', swap='0', translation='original')
        launcher.values = {key: Mock(get=Mock(return_value=value)) for key, value in values.items()}
        launcher.rpc = {'application_id': Mock(get=Mock(return_value='123'))}
        launcher.mods = []
        launcher.status = Mock()
        with patch('desktop_launcher.save_mods', wraps=core.save_mods) as save, patch('desktop_launcher.update_ini', wraps=core.update_ini) as update:
            self.assertTrue(launcher.save())
            self.assertIn('sections', save.call_args.kwargs)
            self.assertEqual(len(update.call_args_list), 1)
            self.assertEqual(Path(update.call_args.args[0]).name, 'YKW4.discord.ini')


if __name__ == '__main__':
    unittest.main()
