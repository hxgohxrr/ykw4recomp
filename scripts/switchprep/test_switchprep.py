# SPDX-License-Identifier: MIT
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
import switchprep as s


def pfs(entries):
    strings = b''; table = b''; payload = b''
    for name, data in entries:
        table += struct.pack('<QQII', len(payload), len(data), len(strings), 0)
        strings += name.encode() + b'\0'; payload += data
    return struct.pack('<4sIII', b'PFS0', len(entries), len(strings), 0) + table + strings + payload


def romfs(name='test.txt', payload=b'test'):
    dirs = struct.pack('<6I', 0, 0xffffffff, 0xffffffff, 0, 0xffffffff, 0)
    files = struct.pack('<IIQQII', 0, 0xffffffff, 0, len(payload), 0xffffffff, len(name.encode())) + name.encode()
    header = struct.pack('<10Q', 80, 80, 0, 80, len(dirs), 80 + len(dirs), 0,
                         80 + len(dirs), len(files), 80 + len(dirs) + len(files))
    return header + dirs + files + payload


def hfs(entries):
    strings, table, payload = b'', b'', b''
    for name, data in entries:
        table += struct.pack('<QQIIQ32s', len(payload), len(data), len(strings),
                             len(data), 0, hashlib.sha256(data).digest())
        strings += name.encode() + b'\0'; payload += data
    strings += b'\0' * (-(16 + len(table) + len(strings)) % 0x200)
    return struct.pack('<4sIII', b'HFS0', len(entries), len(strings), 0) + table + strings + payload


def xci(entries, secure=True):
    partitions = [('update', hfs([('system.nca', b'not game content')]))]
    partitions.append(('secure' if secure else 'normal', hfs(entries)))
    root = hfs(partitions)
    header = bytearray(0xf000)
    header[0x100:0x104] = b'HEAD'
    count, strings_size = struct.unpack_from('<II', root, 4)
    struct.pack_into('<QQ', header, 0x130, len(header), 16 + count * 64 + strings_size)
    return header + root


def title_entries(title_id, kind, application, version, content_kind, payload):
    digest = hashlib.sha256(payload).digest()
    ident = digest[:16]
    header = bytearray(48)
    struct.pack_into('<QIB', header, 0, title_id, version, kind)
    struct.pack_into('<HHH', header, 14, 16, 1, 0)
    struct.pack_into('<Q', header, 32, application)
    metadata = header + digest + ident + len(payload).to_bytes(6, 'little') + bytes([content_kind, 0])
    meta_ident = hashlib.sha256(metadata).hexdigest()[:32]
    return [(meta_ident + '.cnmt.nca', metadata), (ident.hex() + '.nca', payload)]


class Archives(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.ui = s.UI(quiet=True)
    def tearDown(self): self.temp.cleanup()
    def image(self, data):
        p = self.root / 'input'; p.write_bytes(data); return p
    def test_pfs_roundtrip_unicode(self):
        image = self.image(pfs([('日本語.txt', b'abc'), ('main', b'code')]))
        s.unpack_pfs(image, self.root / 'output', self.ui)
        self.assertEqual((self.root / 'output/日本語.txt').read_bytes(), b'abc')
    def test_pfs_traversal_devices_and_duplicates(self):
        for names in [('../escape',), ('C:escape',), ('CON.txt',), ('COM¹.txt',), ('a', 'A'), ('trailing.',)]:
            with self.subTest(names=names), self.assertRaises(s.PrepError):
                s.pfs_members(self.image(pfs([(n, b'data') for n in names])))
    def test_pfs_truncation_and_overlap(self):
        data = bytearray(pfs([('a', b'1234'), ('b', b'5678')]))
        struct.pack_into('<Q', data, 16 + 24, 0)
        with self.assertRaises(s.PrepError): s.pfs_members(self.image(data))
        with self.assertRaises(s.PrepError): s.pfs_members(self.image(pfs([('a', b'test')])[:-1]))
    def test_romfs_roundtrip(self):
        image = self.image(romfs('日本語.txt'))
        s.unpack_romfs(image, self.root / 'output', self.ui)
        self.assertEqual((self.root / 'output/日本語.txt').read_bytes(), b'test')
    def test_romfs_traversal_and_bounds(self):
        for data in [romfs('../escape'), romfs('NUL'), romfs()[:-1]]:
            with self.subTest(), self.assertRaises(s.PrepError): s.romfs_members(self.image(data))
    def test_romfs_cycles(self):
        data = bytearray(romfs()); struct.pack_into('<I', data, 80 + 8, 0)
        with self.assertRaises(s.PrepError): s.romfs_members(self.image(data))
    def test_wrong_hash_never_accepted(self):
        p = self.image(b'actual')
        with self.assertRaises(s.PrepError): s.copy_range(p, s.Member('x', 0, 6), self.root / 'out', self.ui, '0' * 64)
    def test_cnmt_required_content_and_optional_delta(self):
        ident = bytes.fromhex('12' * 16); raw = b'payload'; digest = hashlib.sha256(raw).digest()
        header = bytearray(48); struct.pack_into('<QIB', header, 0, 0x1000, 123, 0x81)
        struct.pack_into('<HHH', header, 14, 16, 2, 0); struct.pack_into('<Q', header, 32, 0x800)
        entry = digest + ident + len(raw).to_bytes(6, 'little') + bytes([1, 0])
        delta = b'\0' * 32 + b'\x34' * 16 + (100).to_bytes(6, 'little') + bytes([6, 0])
        members = {ident.hex() + '.nca': s.Member(ident.hex() + '.nca', 0, len(raw))}
        title = s.parse_cnmt(header + entry + delta, self.root / 'game.nsp', members)
        self.assertEqual(title.application, 0x800); self.assertEqual(len(title.contents), 1)
        with self.assertRaises(s.PrepError): s.parse_cnmt(header + entry + delta, self.root / 'game.nsp', {})
    def test_dlc_selection_duplicate_versions(self):
        class Answers(s.UI):
            def __init__(self): super().__init__('en', quiet=True); self.answers = iter(['1,2', '1'])
            def ask(self, *args): return next(self.answers)
        a = s.Title(Path('one.nsp'), 1, 1, 0x82, 0, [], {})
        b = s.Title(Path('two.nsp'), 1, 2, 0x82, 0, [], {})
        self.assertEqual(s.choose_dlcs(Answers(), [a, b]), [a])

class XciArchives(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.ui = s.UI(quiet=True)
    def tearDown(self): self.temp.cleanup()
    def image(self, data):
        package = self.root / 'game.XCI'; package.write_bytes(data); return package
    def test_secure_members_preserve_original_bytes_and_hashes(self):
        payload = bytes(range(256)) * 4
        package = self.image(xci([('program.nca', payload), ('metadata.cnmt.nca', b'metadata')]))
        original = package.read_bytes()
        members = s.package_members(package)
        self.assertEqual([member.name for member in members], ['program.nca', 'metadata.cnmt.nca'])
        target = self.root / 'copy.nca'
        s.copy_range(package, members[0], target, self.ui, hashlib.sha256(payload).hexdigest())
        self.assertEqual(target.read_bytes(), payload)
        self.assertEqual(package.read_bytes(), original)
    def test_changed_nca_still_fails_cnmt_hash(self):
        package = self.image(xci([('program.nca', b'changed')]))
        with self.assertRaisesRegex(s.PrepError, 'hash does not match CNMT'):
            s.copy_range(package, s.xci_members(package)[0], self.root / 'out', self.ui,
                         hashlib.sha256(b'original').hexdigest())
    def test_invalid_xci_header_and_missing_secure_partition(self):
        valid = xci([('program.nca', b'payload')])
        damaged = bytearray(valid); damaged[0x100:0x104] = b'FAIL'
        bad_offset = bytearray(valid); struct.pack_into('<Q', bad_offset, 0x130, len(valid))
        bad_size = bytearray(valid); struct.pack_into('<Q', bad_size, 0x138, len(valid))
        for data in [b'', valid[:0x1ff], damaged, bad_offset, bad_size,
                     xci([('program.nca', b'payload')], secure=False), valid[:-1]]:
            with self.subTest(size=len(data)), self.assertRaises(s.PrepError):
                s.xci_members(self.image(data))
    def test_hfs_unsafe_names_and_duplicates(self):
        for names in [('../escape',), ('CON.txt',), ('a', 'A'), ('trailing.',)]:
            with self.subTest(names=names), self.assertRaises(s.PrepError):
                s.xci_members(self.image(xci([(name, b'data') for name in names])))
    def test_hfs_malformed_tables_and_ranges(self):
        valid = hfs([('one.nca', b'1234'), ('two.nca', b'5678')])
        cases = [valid[:-1]]
        for offset, fmt, value in [(0, '<4s', b'PFS0'), (4, '<I', 100001),
                                   (8, '<I', 16 * 1024 * 1024 + 1),
                                   (16 + 16, '<I', 0xffffffff), (16 + 20, '<I', 5),
                                   (16, '<Q', 0xffffffffffffffff), (16 + 64, '<Q', 0)]:
            damaged = bytearray(valid); struct.pack_into(fmt, damaged, offset, value); cases.append(damaged)
        unterminated = bytearray(valid); unterminated[16 + 2 * 64:0x200] = b'x' * (0x200 - 16 - 2 * 64)
        cases.append(unterminated)
        for data in cases:
            package = self.image(data)
            with self.subTest(data=bytes(data[:40])), self.assertRaises(s.PrepError):
                s.hfs_members(package, 0, len(data))
    def test_nested_members_cannot_escape_secure_partition(self):
        package = self.image(xci([('program.nca', b'payload')]))
        data = bytearray(package.read_bytes())
        secure_entry = 0xf000 + 16 + 64
        size = struct.unpack_from('<Q', data, secure_entry + 8)[0]
        struct.pack_into('<Q', data, secure_entry + 8, size - 1)
        with self.assertRaises(s.PrepError): s.xci_members(self.image(data))
    def test_multi_title_scan_selection_and_extraction(self):
        app = 0x010086C00AF7C000
        base_entries = title_entries(app, 0x80, app, 0, 1, b'base program')
        update_entries = title_entries(app + 0x800, 0x81, app, 8, 1, b'bundled update')
        dlc_entries = title_entries(0x010086C00AF7D064, 0x82, app, 0, 2, b'upgrade data')
        unrelated = title_entries(0x2000, 0x80, 0x2000, 0, 1, b'other game')
        package = self.image(xci(base_entries + update_entries + dlc_entries + unrelated))
        external = self.root / 'update.nsp'; external.write_bytes(pfs(update_entries))
        newer = self.root / 'newer.nsp'
        newer.write_bytes(pfs(title_entries(app + 0x800, 0x81, app, 917504, 1, b'new update')))
        class Backend:
            def __init__(self): self.inputs = []
            def add_tickets(self, package, members): pass
            def run(self, source, *options):
                data = source.read_bytes(); self.inputs.append(data)
                for option in options:
                    if option.startswith('--section0='):
                        Path(option.split('=', 1)[1]).write_bytes(pfs([('metadata.cnmt', data)]))
                    if option.startswith('--exefs='):
                        Path(option.split('=', 1)[1]).write_bytes(pfs([(name, b'module') for name in ('main', 'rtld', 'sdk', 'main.npdm')]))
                    if option.startswith('--romfs='):
                        Path(option.split('=', 1)[1]).write_bytes(romfs())
        backend = Backend()
        titles, errors = s.scan(self.root, backend, self.root, self.ui)
        self.assertEqual(errors, [])
        self.assertEqual(len(titles), 6)
        request = {'base': package, 'update': None, 'dlcs': []}
        base, update, dlcs = s.requested_titles(titles, request, app)
        self.assertEqual(base.title_id, app); self.assertIsNone(update); self.assertEqual(dlcs, [])
        request.update(update=external, dlcs=[package])
        self.assertEqual(s.requested_titles(titles, request, app)[1].package, external)
        request['update'] = package
        self.assertEqual(s.requested_titles(titles, request, app)[1].version, 8)
        request['update'] = newer
        base, update, dlcs = s.requested_titles(titles, request, app)
        self.assertEqual(update.version, 917504)
        self.assertEqual([title.title_id for title in dlcs], [0x010086C00AF7D064])
        original = package.read_bytes(); backend.inputs.clear()
        final = s.extract(base, update, dlcs, self.root / 'out', self.root, backend, self.ui, images=True)
        self.assertEqual(backend.inputs, [b'base program', b'new update', b'upgrade data'])
        self.assertTrue((final / 'exefs/main').is_file())
        self.assertTrue((final / 'romfs.bin').is_file())
        self.assertTrue((final / 'dlc/010086c00af7d064.romfs').is_file())
        self.assertEqual(package.read_bytes(), original)


class NativeRequest(unittest.TestCase):
    def test_request_accepts_xci_and_deduplicates_shared_container(self):
        import configparser
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); base = root / 'game.XCI'; base.write_bytes(xci([]))
            keys = root / 'local.keys'; keys.write_text('synthetic test')
            request = root / 'selection.ini'
            config = configparser.ConfigParser(interpolation=None)
            config['Selection'] = {'base': str(base), 'update': str(base), 'keys': str(keys),
                                   'dlc_count': '1', 'dlc0': str(base)}
            with request.open('w', encoding='utf-16') as stream: config.write(stream)
            result = s.read_request(request)
            self.assertEqual(result['packages'], [base.resolve()])
            self.assertEqual(result['update'], base.resolve())
            self.assertEqual(result['dlcs'], [base.resolve()])
            for extension in ('.nsz', '.xcz', '.xc0', '.00'):
                unsupported = root / ('game' + extension); unsupported.touch()
                config['Selection']['base'] = str(unsupported)
                with request.open('w', encoding='utf-16') as stream: config.write(stream)
                with self.subTest(extension=extension), self.assertRaisesRegex(s.PrepError, 'not supported'):
                    s.read_request(request)

    def test_image_mode_publishes_mountable_images_without_unpacking(self):
        import configparser
        class Backend:
            def run(self, source, *options):
                for option in options:
                    if option.startswith('--exefs='):
                        Path(option.split('=', 1)[1]).write_bytes(pfs([(n, b'module') for n in ('main', 'rtld', 'sdk', 'main.npdm')]))
                    if option.startswith('--romfs='):
                        Path(option.split('=', 1)[1]).write_bytes(romfs())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); package = root / 'base.nsp'
            package.write_bytes(b'base')
            ident = '12' * 16
            content = s.Content(ident, 4, 1, hashlib.sha256(b'base').hexdigest())
            base = s.Title(package, 0x1000, 0, 0x80, 0x1000, [content], {ident + '.nca': s.Member(ident + '.nca', 0, 4)})
            from unittest.mock import patch
            original_rename = Path.rename
            def deny_directory_rename(path, target):
                if path.is_dir():
                    error = PermissionError('directory rename denied'); error.winerror = 5
                    raise error
                return original_rename(path, target)
            with patch.object(Path, 'rename', deny_directory_rename):
                final = s.extract(base, None, [], root / 'out', root, Backend(), s.UI(quiet=True), images=True)
            import json
            self.assertTrue(json.loads((final / 'preparation.json').read_text())['complete'])
            self.assertTrue((final / 'exefs/main').is_file())
            self.assertEqual(len(s.romfs_members(final / 'romfs.bin')), 1)
            self.assertFalse((final / 'romfs').exists())
            result = root / 'result.ini'; s.write_result(result, final, [], images=True)
            config = configparser.ConfigParser()
            with result.open(encoding='utf-16') as stream: config.read_file(stream)
            self.assertEqual(Path(config['Paths']['romfs']), final / 'romfs.bin')

    def test_unicode_request_and_required_files(self):
        import configparser
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'selection.ini').touch()
            base = root / '日本語 100%.nsp'; base.touch()
            keys = root / 'local.keys'; keys.write_text('synthetic test')
            request = root / 'selection.ini'
            config = configparser.ConfigParser(interpolation=None)
            config['Selection'] = {'base': str(base), 'keys': str(keys), 'dlc_count': '0'}
            with request.open('w', encoding='utf-16') as stream: config.write(stream)
            result = s.read_request(request)
            self.assertEqual(result['base'], base.resolve())
            self.assertEqual(result['packages'], [base.resolve()])
            base.unlink()
            with self.assertRaises(OSError): s.read_request(request)

    def test_selection_rejects_wrong_game_and_keeps_latest_dlc(self):
        app = 0x1000
        base = s.Title(Path('base.nsp'), app, 0, 0x80, app, [], {})
        update = s.Title(Path('update.nsp'), app + 0x800, 1, 0x81, 0x2000, [], {})
        a = s.Title(Path('a.nsp'), app + 1, 0, 0x82, app, [], {})
        b = s.Title(Path('b.nsp'), app + 1, 1, 0x82, app, [], {})
        request = {'base': base.package, 'update': update.package, 'dlcs': []}
        with self.assertRaises(s.PrepError): s.requested_titles([base, update], request, app)
        request.update(update=None, dlcs=[a.package, b.package])
        self.assertEqual(s.requested_titles([base, a, b], request, app)[2], [b])
        request['dlcs'].reverse()
        self.assertEqual(s.requested_titles([base, a, b], request, app)[2], [b])
        b.version = a.version
        self.assertEqual(s.requested_titles([base, a, b], request, app)[2], [b])

    def test_native_mode_never_prompts_and_cleans_private_keys_on_error(self):
        import argparse
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'selection.ini').touch()
            request = {'keys': root / 'keys', 'title_keys': None, 'packages': []}
            args = argparse.Namespace(request=root / 'selection.ini', output=root / 'out',
                result=root / 'result.ini', title_id='1000', backend=root / 'backend', scan_only=False)
            with patch.object(s, 'read_request', return_value=request), patch.object(s, 'Backend'), \
                 patch.object(s, 'scan_packages', return_value=([], [('base.nsp', 'Invalid PFS0')])), \
                 patch.object(s.UI, 'ask', side_effect=AssertionError('Unexpected console prompt')):
                with self.assertRaisesRegex(s.PrepError, 'Invalid PFS0'):
                    s.prepare_request(args, s.UI(quiet=True))
            self.assertFalse((root / 'nsp-private').exists())
            self.assertFalse(args.result.exists())


class Tickets(unittest.TestCase):
    def test_nca_keys_ignore_unrelated_records_and_preserve_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / 'prod.keys'; output = root / 'filtered.keys'
            original = 'header_key = ' + '00' * 32 + '\nmariko_master_kek_source_05 = ' + '11' * 17 + '\n'
            source.write_text(original)
            s.nca_keys(source, output)
            self.assertEqual(source.read_text(), original)
            self.assertNotIn('mariko_', output.read_text())
            output.unlink(); source.write_text('header_key = ' + '22' * 31)
            with self.assertRaisesRegex(s.PrepError, 'expected 64'):
                s.nca_keys(source, output)
            self.assertFalse(output.exists())

    def test_common_and_personalized_ticket(self):
        data = bytearray(0x2b0); struct.pack_into('<I', data, 0, 0x10004)
        data[0x180:0x190] = bytes(range(16)); data[0x2a0:0x2b0] = bytes(range(16, 32))
        self.assertEqual(s.common_ticket_key(data), (bytes(range(16, 32)).hex(), bytes(range(16)).hex()))
        data[0x281] = 1
        self.assertIsNone(s.common_ticket_key(data))
        self.assertIsNone(s.common_ticket_key(b'short'))
    def test_ticket_merge_is_private_and_unique(self):
        import sys
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); prod = root / 'input.keys'; original = 'header_key = ' + '00' * 32; prod.write_text(original)
            data = bytearray(0x2b0); struct.pack_into('<I', data, 0, 0x10004)
            data[0x180:0x190] = bytes(range(16)); data[0x2a0:0x2b0] = bytes(range(16, 32))
            package = root / 'test.nsp'; package.write_bytes(pfs([('ticket.tik', data)]))
            backend = s.Backend(sys.executable, prod, None, root / 'session', s.UI(quiet=True))
            members = {m.name: m for m in s.pfs_members(package)}
            backend.add_tickets(package, members); backend.add_tickets(package, members)
            self.assertEqual(len(backend.title_map), 1)
            self.assertEqual(len((backend.home / '.switch/title.keys').read_text().splitlines()), 1)
            self.assertEqual(prod.read_text(), original)

if __name__ == '__main__': unittest.main()
