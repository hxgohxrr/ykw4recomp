import configparser
import io
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from launcher_core import LinuxGamepad, Preparation, allowed_download, atomic_write, checked_tree, gamebanana_json, import_mod, installation_directory, remember_installation, mod_key, publish_files, read_ini, save_mods, scan_mods, update_ini, validate_settings
from discord_linux import DiscordPresence


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.base = self.root / 'installation with spaces'
        self.base.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def test_installation_selection_persists(self):
        runtime = self.root / 'runtime/YKW4Recomp'
        runtime.parent.mkdir()
        with patch.dict(os.environ, {'XDG_CONFIG_HOME': str(self.root / 'config')}):
            remember_installation(self.base)
            self.assertEqual(installation_directory(runtime), self.base.resolve())
            self.assertEqual(installation_directory(runtime, self.root), self.root.resolve())
            (runtime.parent / 'YKW4_code.so').touch()
            self.assertEqual(installation_directory(runtime), runtime.parent.resolve())

    def test_read_only_runtime_uses_user_data(self):
        with patch.dict(os.environ, {'XDG_CONFIG_HOME': str(self.root / 'config'), 'XDG_DATA_HOME': str(self.root / 'data')}), patch('launcher_core.os.access', return_value=False):
            self.assertEqual(installation_directory(self.root / 'bin/YKW4Recomp'), self.root / 'data/ykw4recomp/game')

    def test_ini_preserves_comments_unknown_sections_and_encoding(self):
        for encoding in ('utf-8', 'utf-16'):
            path = self.base / (encoding + '.ini')
            path.write_text('; keep me\n[Graphics]\nresolution=1\ncustom=42\n[Unknown]\nthing=yes\n', encoding=encoding)
            update_ini(path, {'Graphics': {'resolution': '2'}, 'Launcher': {'translation': 'es'}})
            self.assertIn('; keep me', path.read_text(encoding=encoding))
            parser = read_ini(path)
            self.assertEqual(parser['Graphics']['custom'], '42')
            self.assertEqual(parser['Graphics']['resolution'], '2')
            self.assertEqual(parser['Unknown']['thing'], 'yes')
            self.assertEqual(parser['Launcher']['translation'], 'es')

    def test_ini_rejects_injected_sections_without_writes(self):
        path = self.base / 'settings.ini'
        with self.assertRaises(ValueError):
            update_ini(path, {'Launcher': {'translation': 'es\n[Other]'}})
        self.assertFalse(path.exists())

    def test_settings_validation(self):
        values = dict(fps='60', deadzone='15', rumble='100', resolution='1.5', fullscreen=True, vsync=False, shadows=True, swap=False, translation='original')
        self.assertEqual(validate_settings(values)['Graphics']['resolution'], '1.5')
        for name, value in (('fps', '-1'), ('fps', '1001'), ('deadzone', '91'), ('rumble', '101'), ('resolution', '3')):
            with self.assertRaises(ValueError):
                validate_settings(dict(values, **{name: value}))

    def source_mod(self):
        folder = self.root / 'external mod'
        (folder / 'romfs/text').mkdir(parents=True)
        (folder / 'romfs/text/data').write_text('fixture')
        return folder

    def test_mod_import_order_and_active_list(self):
        folder = self.source_mod()
        first = import_mod(self.base, folder)
        second = import_mod(self.base, folder, translation=True)
        mods = scan_mods(self.base)
        self.assertEqual(len(mods), 2)
        mods.sort(key=lambda mod: mod.identifier == first)
        mods[0].enabled = True
        save_mods(self.base, mods)
        self.assertEqual(scan_mods(self.base)[0].identifier, second)
        self.assertEqual((self.base / 'mods/active.txt').read_text(), second + '\n')
        self.assertTrue((folder / 'romfs/text/data').is_file())

    def test_mod_hash_matches_windows_utf16(self):
        self.assertEqual(mod_key('test'), '975819681a395537')
        self.assertEqual(len(mod_key('日本語😀')), 16)

    def test_zip_import(self):
        archive = self.root / 'mod.zip'
        with zipfile.ZipFile(archive, 'w') as stream:
            stream.writestr('contents/romfs/folder/hello', 'fixture')
        identifier = import_mod(self.base, archive)
        self.assertEqual((self.base / 'mods' / identifier / 'romfs/folder/hello').read_text(), 'fixture')

    def test_zip_rejects_escaping_paths_and_duplicates(self):
        for entries in (('../escape',), ('/absolute',), ('C:/windows',), ('romfs/a', 'romfs/A')):
            archive = self.root / 'bad.zip'
            with zipfile.ZipFile(archive, 'w') as stream:
                for name in entries:
                    stream.writestr(name, 'unsafe')
            with self.assertRaises(ValueError):
                import_mod(self.base, archive)
        self.assertFalse((self.root / 'escape').exists())

    def test_zip_rejects_symlinks(self):
        archive = self.root / 'symlink.zip'
        with zipfile.ZipFile(archive, 'w') as stream:
            entry = zipfile.ZipInfo('romfs/link')
            entry.create_system = 3
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            stream.writestr(entry, '../outside')
        with self.assertRaises(ValueError):
            import_mod(self.base, archive)

    @unittest.skipIf(os.name == 'nt', 'POSIX filesystem')
    def test_tree_rejects_symlinks(self):
        folder = self.source_mod()
        (folder / 'romfs/link').symlink_to(self.root)
        with self.assertRaises(ValueError):
            checked_tree(folder)

    def test_ambiguous_romfs_rejected(self):
        folder = self.source_mod()
        (folder / 'other/romfs').mkdir(parents=True)
        with self.assertRaises(ValueError):
            import_mod(self.base, folder)

    def test_publication_rollback_preserves_saves(self):
        (self.base / 'YKW4_data/save').mkdir(parents=True)
        (self.base / 'YKW4_data/save/player').write_text('save')
        (self.base / 'YKW4_code.so').write_text('old')
        staging = self.root / 'staging.so'
        staging.write_text('new')
        with self.assertRaises(FileNotFoundError):
            publish_files(self.base, [(staging, 'YKW4_code.so'), (self.root / 'missing', 'YKW4_data/romfs.bin')], self.root / 'backup')
        self.assertEqual((self.base / 'YKW4_code.so').read_text(), 'old')
        self.assertEqual((self.base / 'YKW4_data/save/player').read_text(), 'save')
        self.assertEqual(staging.read_text(), 'new')

    def test_consent_before_any_preparation_writes(self):
        base = self.root / 'not-created'
        with self.assertRaises(ValueError):
            Preparation(self.root / 'runtime', base).prepare({})
        self.assertFalse(base.exists())

    def test_precancelled_process_does_not_start(self):
        preparation = Preparation(self.root / 'runtime', self.base)
        preparation.cancel()
        with patch('launcher_core.subprocess.Popen') as process:
            with self.assertRaises(RuntimeError):
                preparation.run(['unreachable'])
            process.assert_not_called()

    def test_discord_frame_and_activity(self):
        class Connection:
            def sendall(self, data):
                self.data = data
        connection = Connection()
        DiscordPresence.send(connection, 1, {'hello': '日本語'})
        opcode, length = struct.unpack('<II', connection.data[:8])
        self.assertEqual((opcode, length), (1, len(connection.data) - 8))
        presence = DiscordPresence({'show_time': '1', 'details': 'fixture', 'dlc_image': 'dlc'}, 123, True)
        self.assertEqual(presence.activity()['assets']['large_image'], 'dlc')
        self.assertIn('timestamps', presence.activity())

    def test_catalog_allowlist(self):
        self.assertTrue(allowed_download('https://files.gamebanana.com/mod.zip'))
        for url in ('http://gamebanana.com/mod.zip', 'https://gamebanana.com.evil.invalid/mod.zip', 'file:///tmp/mod.zip', 'https://user@gamebanana.com/mod.zip', 'https://gamebanana.com:444/mod.zip'):
            self.assertFalse(allowed_download(url))

    def test_gamepad_mapping_and_initial_state(self):
        self.assertEqual(LinuxGamepad.action(struct.pack('<IhBB', 0, 1, 1, 0)), 'accept')
        self.assertEqual(LinuxGamepad.action(struct.pack('<IhBB', 0, 1, 1, 1)), 'cancel')
        self.assertEqual(LinuxGamepad.action(struct.pack('<IhBB', 0, -32767, 2, 7)), 'previous')
        self.assertEqual(LinuxGamepad.action(struct.pack('<IhBB', 0, 32767, 2, 1)), 'next')
        self.assertIsNone(LinuxGamepad.action(struct.pack('<IhBB', 0, 1, 0x81, 0)))
        self.assertIsNone(LinuxGamepad.action(struct.pack('<IhBB', 0, 0, 1, 0)))

    def test_catalog_filters_other_games(self):
        with patch('launcher_core.urllib.request.build_opener') as opener:
            opener.return_value.open.return_value.__enter__.return_value.read.return_value = b'{"_aRecords":[{"_idRow":1,"_aGame":{"_idRow":16848}},{"_idRow":2,"_aGame":{"_idRow":1}}]}'
            self.assertEqual([entry['_idRow'] for entry in gamebanana_json()], [1])
            opener.return_value.open.return_value.__enter__.return_value.read.return_value = b'{"_aGame":{"_idRow":1}}'
            with self.assertRaises(ValueError):
                gamebanana_json(123)

    def test_discord_fragmented_frames_and_limits(self):
        payload = b'{"evt":"READY"}'
        buffer = bytearray(struct.pack('<II', 1, len(payload)) + payload)
        class FragmentedConnection:
            def recv(self, size):
                block = buffer[:min(size, 2)]
                del buffer[:len(block)]
                return block
        self.assertEqual(DiscordPresence.receive(FragmentedConnection()), (1, {'evt': 'READY'}))
        buffer.extend(struct.pack('<II', 1, 65537))
        with self.assertRaises(ValueError):
            DiscordPresence.receive(FragmentedConnection())

    @unittest.skipIf(os.name == 'nt', 'Linux preparation backend')
    def test_preparation_seals_only_new_install_and_preserves_saves(self):
        folder = self.source_mod()
        exefs = self.root / 'exefs'
        exefs.mkdir()
        for name in ('main', 'rtld', 'sdk', 'main.npdm'):
            (exefs / name).write_bytes(b'fixture')
        for policy in ('bound-or-new', 'legacy'):
            preparation = Preparation(self.root / 'runtime', self.base, lambda line: None)
            commands = []
            def fake_run(command):
                commands.append([str(part) for part in command])
                if '-r' in command:
                    staged = Path(command[command.index('-r') + 1])
                    self.assertNotEqual(staged, exefs)
                    self.assertEqual(staged.parent.name, 'input')
                    self.assertEqual((staged / 'main').read_bytes(), b'fixture')
                    self.assertEqual(list(staged.parent.iterdir()), [staged])
                    output = Path(command[command.index('--outdir') + 1]) / 'YKW4'
                    output.mkdir(parents=True)
                    (output / 'YKW4_code.so').write_bytes(b'fixture')
                if '--launcher-pack-romfs' in command:
                    Path(command[-1]).write_bytes(b'romfs fixture')
            preparation.run = fake_run
            with patch('launcher_core.subprocess.run') as process:
                process.return_value.stdout = policy
                preparation.prepare({'exefs': str(exefs), 'romfs': str(folder / 'romfs')}, accepted=True)
            self.assertEqual(any('--binding-seal' in command for command in commands), policy == 'bound-or-new')
            self.assertTrue((self.base / 'YKW4_data/romfs.bin').is_file())


if __name__ == '__main__':
    unittest.main()
