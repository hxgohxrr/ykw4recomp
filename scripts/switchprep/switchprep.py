# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations
import argparse
import configparser
import ctypes
from dataclasses import dataclass
import hashlib
import json
import locale
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import uuid
from ticket_keys import common_ticket_key

class PrepError(Exception):
    pass

def read_at(stream, offset, size):
    if offset < 0 or size < 0:
        raise PrepError('Invalid archive range')
    stream.seek(offset)
    data = stream.read(size)
    if len(data) != size:
        raise PrepError('Truncated archive')
    return data

def safe_name(name):
    if (not name or name in ('.', '..') or name[-1] in ' .' or
        any(ord(c) < 32 or c in '/\\:*?"<>|' for c in name) or
        re.fullmatch(r'(?i:CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(?:\..*)?', name)):
        raise PrepError('Unsafe archive filename')
    return name

@dataclass(frozen=True)
class Member:
    name: str
    offset: int
    size: int

def pfs_members(path):
    size = path.stat().st_size
    with path.open('rb') as f:
        magic, count, strings_size, reserved = struct.unpack('<4sIII', read_at(f, 0, 16))
        if magic != b'PFS0' or count > 100000 or strings_size > 16 * 1024 * 1024:
            raise PrepError('Unsupported or invalid PFS0/NSP')
        start = 16 + count * 24 + strings_size
        if start > size:
            raise PrepError('Truncated NSP header')
        table = read_at(f, 16, count * 24)
        strings = read_at(f, 16 + count * 24, strings_size)
    result, names, ranges = [], set(), []
    for i in range(count):
        offset, length, index, _ = struct.unpack_from('<QQII', table, i * 24)
        end = strings.find(b'\0', index)
        if index >= len(strings) or end < 0:
            raise PrepError('Invalid NSP string table')
        name = safe_name(strings[index:end].decode('utf-8'))
        if name.casefold() in names or offset + length > size - start:
            raise PrepError('Duplicate or out-of-range NSP member')
        names.add(name.casefold())
        if length:
            ranges.append((offset, offset + length))
        result.append(Member(name, start + offset, length))
    ranges.sort()
    if any(b[0] < a[1] for a, b in zip(ranges, ranges[1:])):
        raise PrepError('Overlapping NSP members')
    return result

def hfs_members(path, offset, size):
    if offset < 0 or size < 16 or offset + size > path.stat().st_size:
        raise PrepError('Invalid HFS0 partition range')
    with path.open('rb') as stream:
        magic, count, strings_size, reserved = struct.unpack('<4sIII', read_at(stream, offset, 16))
        if magic != b'HFS0' or count > 100000 or strings_size > 16 * 1024 * 1024:
            raise PrepError('Unsupported or invalid HFS0/XCI')
        start = 16 + count * 64 + strings_size
        if start > size:
            raise PrepError('Truncated HFS0 header')
        table = read_at(stream, offset + 16, count * 64)
        strings = read_at(stream, offset + 16 + count * 64, strings_size)
    result, names, ranges = [], set(), []
    for index in range(count):
        relative, length, name_index, hashed_size = struct.unpack_from('<QQII', table, index * 64)
        end = strings.find(b'\0', name_index)
        if name_index >= len(strings) or end < 0:
            raise PrepError('Invalid HFS0 string table')
        name = safe_name(strings[name_index:end].decode('utf-8'))
        if name.casefold() in names or relative + length > size - start or hashed_size > length:
            raise PrepError('Duplicate or out-of-range HFS0 member')
        names.add(name.casefold())
        if length:
            ranges.append((relative, relative + length))
        result.append(Member(name, offset + start + relative, length))
    ranges.sort()
    if any(following[0] < previous[1] for previous, following in zip(ranges, ranges[1:])):
        raise PrepError('Overlapping HFS0 members')
    return result


def xci_members(path):
    size = path.stat().st_size
    with path.open('rb') as stream:
        header = read_at(stream, 0, 0x200)
    if header[0x100:0x104] != b'HEAD':
        raise PrepError('Unsupported or invalid XCI header')
    root_offset, header_size = struct.unpack_from('<QQ', header, 0x130)
    if root_offset < 0x200 or header_size < 16 or root_offset + header_size > size:
        raise PrepError('Invalid XCI root partition range')
    partitions = hfs_members(path, root_offset, size - root_offset)
    secure = next((member for member in partitions if member.name == 'secure'), None)
    if secure is None:
        raise PrepError('No secure partition found in XCI')
    return hfs_members(path, secure.offset, secure.size)


def package_members(path):
    if path.suffix.lower() == '.xci':
        return xci_members(path)
    return pfs_members(path)


@dataclass(frozen=True)
class Content:
    ident: str
    size: int
    kind: int
    digest: str

@dataclass
class Title:
    package: Path
    title_id: int
    version: int
    kind: int
    application: int
    contents: list[Content]
    members: dict[str, Member]

    @property
    def label(self):
        return f'{self.title_id:016X}  v{self.version}  {self.package.name}'

def parse_cnmt(data, package, members):
    if len(data) < 32:
        raise PrepError('Truncated CNMT')
    title_id, version, kind = struct.unpack_from('<QIB', data)
    ext, count, metas = struct.unpack_from('<HHH', data, 14)
    start = 32 + ext
    if kind not in (0x80, 0x81, 0x82) or count > 4096 or start + count * 56 + metas * 16 > len(data):
        raise PrepError('Unsupported or truncated content metadata')
    if kind != 0x80 and ext < 8:
        raise PrepError('Missing application metadata')
    application = title_id if kind == 0x80 else struct.unpack_from('<Q', data, 32)[0]
    contents = []
    for i in range(count):
        row = data[start + i * 56:start + (i + 1) * 56]
        content = Content(row[32:48].hex(), int.from_bytes(row[48:54], 'little'), row[54], row[:32].hex())
        member = members.get(content.ident + '.nca')
        if not member and content.kind == 6:
            continue   
        if not member or member.size != content.size:
            raise PrepError('Content referenced by CNMT is missing or has the wrong size')
        contents.append(content)
    return Title(package, title_id, version, kind, application, contents, members)

class UI:
    def __init__(self, lang='auto', quiet=False):
        if lang == 'auto':
            lang = (locale.getlocale()[0] or 'en').split('_')[0]
        self.es = lang.lower().startswith('es')
        self.quiet = quiet
        self.color = sys.stdout.isatty() and not os.environ.get('NO_COLOR')
        if self.color and os.name == 'nt':
            handle = ctypes.windll.kernel32.GetStdHandle(-11)
            mode = ctypes.c_ulong()
            if ctypes.windll.kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
                ctypes.windll.kernel32.SetConsoleMode(handle, mode.value | 4)
        self.last = 0
        self.started = time.monotonic()
        self.active = False
    def tr(self, en, es):
        return es if self.es else en
    def say(self, text, color='36'):
        if not self.quiet:
            print(f'\033[{color}m{text}\033[0m' if self.color else text, flush=True)
    def ask(self, en, es, default=''):
        prompt = self.tr(en, es)
        return input(f'{prompt}' + (f' [{default}]' if default else '') + ': ').strip().strip('"') or default
    def progress(self, label, done=None, total=None):
        now = time.monotonic()
        if self.quiet or now - self.last < .3:
            return
        self.last = now
        spinner = '|/-\\'[int(now * 4) % 4]
        suffix = f' {done / max(total, 1):6.1%}  {done / 1048576:.0f}/{total / 1048576:.0f} MiB' if done is not None else f' {spinner}  {int(now - self.started)}s'
        if sys.stdout.isatty():
            self.active = True
            print('\r' + ('\033[2K' if self.color else '') + label[:65] + suffix, end='', flush=True)
        else:
            print(label[:65] + suffix, flush=True)
    def finish(self):
        if self.active:
            print()
            self.active = False

def copy_range(source, member, destination, ui, digest=None):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(destination.parent).free < member.size + 64 * 1024 * 1024:
        raise PrepError('Not enough free disk space')
    hashed = hashlib.sha256()
    with source.open('rb') as src, destination.open('xb') as dst:
        src.seek(member.offset)
        done = 0
        while done < member.size:
            chunk = src.read(min(4 * 1024 * 1024, member.size - done))
            if not chunk:
                raise PrepError('Truncated content')
            dst.write(chunk)
            hashed.update(chunk)
            done += len(chunk)
            ui.progress('Copy / SHA-256', done, member.size)
    ui.finish()
    if digest and hashed.hexdigest() != digest:
        raise PrepError('NCA content hash does not match CNMT; extraction stopped')

def nca_keys(source, destination):

    entries = {}
    sources = {'aes_kek_generation_source': 32, 'aes_key_generation_source': 32,
               'key_area_key_application_source': 32, 'key_area_key_ocean_source': 32,
               'key_area_key_system_source': 32, 'titlekek_source': 32,
               'header_kek_source': 32, 'header_key_source': 64}
    for line in Path(source).read_text(encoding='utf-8-sig').splitlines():
        if '=' not in line: continue
        name, value = (part.strip().lower() for part in line.split('=', 1))
        expected = 64 if name == 'header_key' else sources.get(name)
        if re.fullmatch(r'(master_key|titlekek|key_area_key_(application|ocean|system))_[0-9a-f]{2}', name):
            expected = 32
        if expected is None: continue
        if not re.fullmatch('[0-9a-f]{' + str(expected) + '}', value):
            raise PrepError('Invalid ' + name + ': expected ' + str(expected) + ' hexadecimal digits; key values are never logged')
        if name in entries and entries[name] != value:
            raise PrepError('Conflicting entries for ' + name)
        entries[name] = value
    if 'header_key' not in entries and not {'master_key_00', 'header_kek_source', 'header_key_source'} <= entries.keys():
        raise PrepError('Missing NCA header_key in prod.keys')
    with destination.open('x', encoding='ascii') as stream:
        for name, value in sorted(entries.items()): stream.write(name + ' = ' + value + '\n')


class Backend:
    def __init__(self, executable, keys, title_keys, workspace, ui):
        self.exe = Path(executable).resolve(strict=True)
        if not self.exe.is_file():
            raise PrepError('Backend must be an executable file')
        self.ui = ui
        self.home = workspace / 'backend-home'
        keydir = self.home / '.switch'
        keydir.mkdir(parents=True)
        nca_keys(keys, keydir / 'prod.keys')
        if title_keys:
            shutil.copyfile(title_keys, keydir / 'title.keys')
        self.title_map = {}
        if title_keys:
            for line in Path(title_keys).read_text(encoding='utf-8-sig').splitlines():
                if '=' not in line:
                    continue
                name, value = (part.strip().lower().removeprefix('0x') for part in line.split('=', 1))
                if re.fullmatch('[0-9a-f]{32}', name) and re.fullmatch('[0-9a-f]{32}', value):
                    if name in self.title_map and self.title_map[name] != value:
                        raise PrepError('Conflicting entries in supplied title.keys')
                    self.title_map[name] = value
        self.write_titles()
        self.keys = keydir / 'prod.keys'
        self.env = dict(os.environ, HOME=str(self.home), USERPROFILE=str(self.home), XDG_CONFIG_HOME=str(self.home))
    def write_titles(self):
        with (self.home / '.switch/title.keys').open('w', encoding='ascii') as f:
            for name, value in sorted(self.title_map.items()):
                f.write(name + ' = ' + value + '\n')
    def add_tickets(self, package, members):
        mappings = []
        with package.open('rb') as f:
            for member in members.values():
                if member.name.lower().endswith('.tik') and member.size <= 16384:
                    mapping = common_ticket_key(read_at(f, member.offset, member.size))
                    if mapping:
                        mappings.append(mapping)
        if mappings:
            for rights_id, encrypted_key in mappings:
                if rights_id in self.title_map and self.title_map[rights_id] != encrypted_key:
                    raise PrepError('Conflicting common-ticket title key mapping')
                self.title_map[rights_id] = encrypted_key
            self.write_titles()
    def run(self, source, *options):
        args = [str(self.exe), '-k', str(self.keys), '--suppresskeys', '--disablekeywarns', *options, str(source)]
        outputs = [Path(option.split('=', 1)[1]) for option in options if option.startswith(('--romfs=', '--exefs=', '--section0='))]
        with tempfile.TemporaryFile() as log:
            process = subprocess.Popen(args, env=self.env, stdout=log, stderr=log,
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            try:
                while process.poll() is None:
                    written = sum(path.stat().st_size for path in outputs if path.is_file())
                    label = self.ui.tr('Preparing data', 'Preparando datos')
                    if written: label += f' ({written / 1048576:.0f} MiB)'
                    self.ui.progress(label)
                    time.sleep(.15)
            except BaseException:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
                raise
            finally:
                self.ui.finish()
            if process.returncode:
                raise PrepError('Backend failed. Check your local keys and backend compatibility; raw output is not displayed to protect keys.')


def scan(folder, backend, workspace, ui):
    packages = []
    for directory, dirs, files in os.walk(folder, followlinks=False):
        dirs[:] = [d for d in dirs if not (Path(directory) / d).is_symlink() and not (Path(directory) / d).is_junction()]
        packages.extend(Path(directory) / f for f in files if f.lower().endswith(('.nsp', '.xci')) and not (Path(directory) / f).is_symlink())
    return scan_packages(packages, backend, workspace, ui)


def scan_packages(packages, backend, workspace, ui):
    titles, errors, seen = [], [], set()
    for index, package in enumerate(sorted(packages), 1):
        ui.say(f'[{index}/{len(packages)}] {package.name}')
        try:
            members = {m.name.lower(): m for m in package_members(package)}
            backend.add_tickets(package, members)
            metas = [m for m in members.values() if m.name.lower().endswith('.cnmt.nca')]
            if not metas:
                raise PrepError('No CNMT metadata found')
            for meta in metas:
                with tempfile.TemporaryDirectory(dir=workspace) as temporary:
                    temp = Path(temporary)
                    nca = temp / 'metadata.nca'
                    copy_range(package, meta, nca, ui)
                    image = temp / 'metadata.pfs0'
                    backend.run(nca, '-x', '--section0=' + str(image))
                    found = False
                    for entry in pfs_members(image):
                        if entry.name.lower().endswith('.cnmt'):
                            if entry.size > 1024 * 1024:
                                raise PrepError('CNMT metadata is too large')
                            with image.open('rb') as f:
                                title = parse_cnmt(read_at(f, entry.offset, entry.size), package, members)
                            signature = (package, title.title_id, title.version, tuple(c.ident for c in title.contents))
                            if signature not in seen:
                                seen.add(signature); titles.append(title)
                            found = True
                    if not found:
                        raise PrepError('No readable CNMT found')
        except (PrepError, OSError, ValueError, struct.error, UnicodeError) as error:
            errors.append((package.name, str(error)))
            ui.say(ui.tr('Skipped: ', 'Omitido: ') + str(error), '33')
    return titles, errors


def read_request(path):
    config = configparser.ConfigParser(interpolation=None)
    with path.open(encoding='utf-16') as stream:
        config.read_file(stream)
    section = config['Selection']
    def local(key, required=False):
        value = section.get(key, '').strip()
        if not value:
            if required: raise PrepError('Missing selection: ' + key)
            return None
        result = Path(value).resolve(strict=True)
        if not result.is_file(): raise PrepError('Selection must be a file: ' + key)
        return result
    request = {key: local(key, key in ('base', 'keys')) for key in ('base', 'update', 'keys', 'title_keys')}
    count = section.getint('dlc_count', 0)
    if not 0 <= count <= 64: raise PrepError('Invalid DLC count')
    request['dlcs'] = [local('dlc' + str(i)) for i in range(count)]
    if len(request['dlcs']) > 64 or any(p is None for p in request['dlcs']):
        raise PrepError('Invalid DLC selection')
    request['packages'] = list(dict.fromkeys([request['base']] + ([request['update']] if request['update'] else []) + request['dlcs']))
    if any(p.suffix.lower() not in ('.nsp', '.xci') for p in request['packages']):
        raise PrepError('Select NSP or XCI files; compressed NSZ/XCZ and split packages are not supported')
    return request


def requested_titles(titles, request, application):
    def one(package, kind):
        matches = [t for t in titles if t.package == package and t.kind == kind and t.application == application]
        if not matches: raise PrepError('Selected package has no compatible content: ' + package.name)
        return max(matches, key=lambda t: t.version)
    base = one(request['base'], 0x80)
    update = one(request['update'], 0x81) if request['update'] else None
    dlcs = []
    for package in request['dlcs']:
        matches = [t for t in titles if t.package == package and t.kind == 0x82 and t.application == application]
        if not matches: raise PrepError('Selected package has no compatible DLC: ' + package.name)
        dlcs.extend(matches)
    selected = {}
    for title in dlcs:
        previous = selected.get(title.title_id)
        if previous is None or title.version > previous.version:
            selected[title.title_id] = title
    return base, update, list(selected.values())


def write_result(result, final, dlcs, images=False):
    config = configparser.ConfigParser(interpolation=None)
    config['Paths'] = {'exefs': str(final / 'exefs'), 'romfs': str(final / ('romfs.bin' if images else 'romfs')), 'dlc': str(final / 'dlc') if dlcs else ''}
    with result.open('x', encoding='utf-16') as stream:
        config.write(stream)


def prepare_request(args, ui):
    if not args.output or not args.result or not args.title_id:
        raise PrepError('Launcher request requires output, result and title ID')
    args.request = args.request.resolve(strict=True)
    args.output = args.output.resolve()
    args.result = args.result.resolve()
    request = read_request(args.request)
    app = Path(sys.executable).parent if getattr(sys, 'frozen', False) else Path(__file__).parent
    executable = args.backend or app / 'backend/hactool.exe'
    args.output.mkdir(parents=True, exist_ok=True)

    private = args.request.parent / 'nsp-private'
    private.mkdir()
    try:
        backend = Backend(executable, request['keys'], request['title_keys'], private, ui)
        titles, errors = scan_packages(request['packages'], backend, private, ui)
        if errors: raise PrepError('Could not read selected NSP/XCI: ' + '; '.join(name + ': ' + error for name, error in errors))
        base, update, dlcs = requested_titles(titles, request, int(args.title_id, 16))
        ui.say(f'Base: {base.package.name}; update: {update.version if update else "none"}; DLC: {len(dlcs)}')
        if args.scan_only: return 0
        final = extract(base, update, dlcs, args.output, private, backend, ui, images=True)
        write_result(args.result, final, dlcs, images=True)
        ui.say(ui.tr('Ready: ', 'Preparado: ') + str(final), '32')
        return 0
    finally:
        shutil.rmtree(private)


def unpack_pfs(image, output, ui):
    output.mkdir(parents=True, exist_ok=True)
    for member in pfs_members(image):
        target = output / member.name
        if target.exists():
            target.unlink()
        copy_range(image, member, target, ui)


def romfs_members(image):
    size = image.stat().st_size
    with image.open('rb') as f:
        h = struct.unpack('<10Q', read_at(f, 0, 80))
        if h[0] != 80 or any(h[i] + h[i + 1] > size for i in (1, 3, 5, 7)) or h[9] > size or h[4] > 128 * 1024 * 1024 or h[8] > 256 * 1024 * 1024:
            raise PrepError('Invalid RomFS header')
        dirs, files = read_at(f, h[3], h[4]), read_at(f, h[7], h[8])
    result, seen_dirs, seen_files, paths = [], set(), set(), set()
    pending = [(0, Path(), 0)]
    while pending:
        index, parent, depth = pending.pop()
        if depth > 96 or index in seen_dirs or len(seen_dirs) > 100000:
            raise PrepError('Cyclic or excessive RomFS directories')
        seen_dirs.add(index)
        if index + 24 > len(dirs):
            raise PrepError('Invalid RomFS directory')
        _, sibling, child, file_index, _, length = struct.unpack_from('<6I', dirs, index)
        if index + 24 + length > len(dirs):
            raise PrepError('Truncated RomFS directory name')
        name = dirs[index + 24:index + 24 + length].decode('utf-8')
        path = parent / safe_name(name) if index else parent
        if index:
            key = path.as_posix().casefold()
            if key in paths: raise PrepError('Duplicate RomFS path')
            paths.add(key)
        if sibling != 0xffffffff:
            pending.append((sibling, parent, depth))
        if child != 0xffffffff:
            pending.append((child, path, depth + 1))
        while file_index != 0xffffffff:
            if file_index in seen_files or file_index + 32 > len(files) or len(seen_files) > 1000000:
                raise PrepError('Cyclic or invalid RomFS file')
            seen_files.add(file_index)
            _, next_file, offset, length, _, name_size = struct.unpack_from('<IIQQII', files, file_index)
            end = file_index + 32 + name_size
            if end > len(files) or h[9] + offset + length > size:
                raise PrepError('Out-of-range RomFS file')
            name = safe_name(files[file_index + 32:end].decode('utf-8'))
            target = path / name
            key = target.as_posix().casefold()
            if key in paths: raise PrepError('Duplicate RomFS path')
            paths.add(key)
            result.append(Member(target.as_posix(), h[9] + offset, length))
            file_index = next_file
    return result


def unpack_romfs(image, output, ui):
    members = romfs_members(image)
    output.mkdir(parents=True, exist_ok=True)
    if sum(m.size for m in members) + 64 * 1024 * 1024 > shutil.disk_usage(output).free:
        raise PrepError('Not enough disk space for extracted RomFS')
    if not members: raise PrepError('RomFS contains no files')
    for index, member in enumerate(members, 1):
        ui.progress(f'RomFS [{index}/{len(members)}]')
        copy_range(image, member, output / member.name, ui)


def materialize(title, content, directory, ui):
    path = directory / (content.ident + '.nca')
    if not path.exists():
        copy_range(title.package, title.members[content.ident + '.nca'], path, ui, content.digest)
    return path


def one_content(title, kind):
    contents = [c for c in title.contents if c.kind == kind]
    if len(contents) != 1:
        raise PrepError('Expected one program/data NCA; multi-program titles are not supported')
    return contents[0]


def extract(base, update, dlcs, output, workspace, backend, ui, images=False):
    run_id = time.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8]
    output.mkdir(parents=True, exist_ok=True)


    staging = output / ('Prepared-' + run_id)
    staging.mkdir()
    final = staging
    complete = False
    try:
        with tempfile.TemporaryDirectory(prefix='.switchprep-work-', dir=output) as temporary:
            temp = Path(temporary)
            base_nca = materialize(base, one_content(base, 1), temp, ui)
            chosen = materialize(update, one_content(update, 1), temp, ui) if update else base_nca
            exefs = temp / 'base-exefs.pfs0'
            backend.run(base_nca, '-x', '--exefs=' + str(exefs))
            unpack_pfs(exefs, staging / 'exefs', ui)
            exefs.unlink()
            image = temp / 'game.romfs'
            options = ['-x', '--romfs=' + str(image)]
            if update:
                patched_exefs = temp / 'update-exefs.pfs0'
                options += ['--basenca=' + str(base_nca), '--exefs=' + str(patched_exefs)]
            backend.run(chosen, *options)
            if update:
                unpack_pfs(patched_exefs, staging / 'exefs', ui)
                patched_exefs.unlink()
            if images:
                romfs_members(image)   
                image.rename(staging / 'romfs.bin')
            else:
                unpack_romfs(image, staging / 'romfs', ui)
                image.unlink()
            for path in list(temp.glob('*.nca')): path.unlink()
            for title in dlcs:
                ui.say('DLC ' + title.label)
                data = [c for c in title.contents if c.kind in (2, 5)]
                if len(data) != 1: raise PrepError('Expected one data NCA in DLC')
                nca = materialize(title, data[0], temp, ui)
                image = temp / 'dlc.romfs'
                backend.run(nca, '-x', '--romfs=' + str(image))
                if images:
                    romfs_members(image)
                    (staging / 'dlc').mkdir(exist_ok=True)
                    image.rename(staging / 'dlc' / f'{title.title_id:016x}.romfs')
                else:
                    unpack_romfs(image, staging / 'dlc' / f'{title.title_id:016x}' / 'romfs', ui)
                    image.unlink()
                nca.unlink()
        if not all((staging / 'exefs' / name).is_file() for name in ('main', 'rtld', 'sdk', 'main.npdm')):
            raise PrepError('Incomplete ExeFS; missing required launcher files')
        manifest = {'complete': True, 'application': f'{base.title_id:016X}',
                    'romfs_format': 'image' if images else 'directory',
                    'base_version': base.version, 'update_version': update.version if update else None,
                    'dlc': [{'id': f'{d.title_id:016X}', 'version': d.version} for d in dlcs],
                    'note': 'Private extracted game data. Do not include with the redistributable launcher.'}
        (staging / 'preparation.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        complete = True
        ui.say('Completed extraction: ' + str(final))
        return final
    except BaseException:
        if not complete:

            try:
                shutil.rmtree(staging)
            except OSError as cleanup_error:
                ui.say(f'Could not clean incomplete extraction at {staging}: {cleanup_error}')
        raise


def choose(ui, title, entries, optional=False):
    ui.say('\n' + title, '35')
    if optional: ui.say('  0. ' + ui.tr('None', 'Ninguna'))
    for i, entry in enumerate(entries, 1): ui.say(f'  {i}. {entry.label}')
    while True:
        text = ui.ask('Choose number', 'Elige un número', '0' if optional else '1')
        if text.isdecimal() and 0 <= int(text) <= len(entries) and (optional or int(text)):
            return entries[int(text) - 1] if int(text) else None


def choose_dlcs(ui, entries):
    if not entries: return []
    ui.say('\nDLC', '35')
    for i, entry in enumerate(entries, 1): ui.say(f'  {i}. {entry.label}')
    while True:
        text = ui.ask('DLC: all, none, or numbers (1,3-5)', 'DLC: all, none o números (1,3-5)', 'none').lower()
        if text in ('all', 'todos'): return entries
        if text in ('none', 'ninguno', '0'): return []
        try:
            numbers = set()
            for part in text.split(','):
                match = re.fullmatch(r'\s*(\d+)(?:-(\d+))?\s*', part)
                if not match: raise ValueError()
                lo = int(match[1]); hi = int(match[2] or lo)
                if not 1 <= lo <= hi <= len(entries): raise ValueError()
                numbers.update(range(lo, hi + 1))
            chosen = [entries[i - 1] for i in sorted(numbers)]
            if len({t.title_id for t in chosen}) != len(chosen):
                raise ValueError()
            return chosen
        except ValueError:
            ui.say(ui.tr('Invalid selection or multiple versions of the same DLC.', 'Selección inválida o varias versiones del mismo DLC.'), '33')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description='SwitchPrep: local NSP/XCI preparation for extracted-file launchers')
    parser.add_argument('--lang', default='auto')
    parser.add_argument('--folder', type=Path)
    parser.add_argument('--backend', type=Path)
    parser.add_argument('--keys', type=Path)
    parser.add_argument('--title-keys', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--result', type=Path)
    parser.add_argument('--title-id', help='Restrict application ID (hex)')
    parser.add_argument('--scan-only', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--request', type=Path, help='UTF-16 selection from the native launcher; no console prompts')
    args = parser.parse_args()
    ui = UI(args.lang)
    if args.request:
        return prepare_request(args, ui)
    ui.say('SwitchPrep  |  Local package assistant', '36')
    ui.say(ui.tr('Use your own dumps and locally supplied keys. No downloads or telemetry. Ctrl+C cancels.',
                 'Usa tus volcados y claves locales. Sin descargas ni telemetría. Ctrl+C cancela.'))
    folder = (args.folder or Path(ui.ask('Folder to scan', 'Carpeta que analizar', str(Path.cwd())))).resolve(strict=True)
    if not folder.is_dir(): raise PrepError('Input must be a folder')
    app = Path(sys.executable).parent if getattr(sys, 'frozen', False) else Path(__file__).parent
    candidates = [app / 'backend/hactool.exe', app / 'hactool.exe']
    default_backend = next((str(p) for p in candidates if p.is_file()), shutil.which('hactool') or '')
    backend_path = args.backend or Path(ui.ask('Path to hactool executable', 'Ruta del ejecutable hactool', default_backend))
    keys = args.keys or Path(ui.ask('Path to your prod.keys', 'Ruta de tu prod.keys', str(Path.home() / '.switch/prod.keys')))
    keys = keys.resolve(strict=True)
    title_keys = args.title_keys
    if title_keys is None:
        candidate = keys.parent / 'title.keys'
        if candidate.is_file(): title_keys = candidate
    if title_keys is None and not args.scan_only:
        value = ui.ask('Optional title.keys path (Enter to skip)', 'Ruta opcional de title.keys (Enter para omitir)')
        title_keys = Path(value).resolve(strict=True) if value else None
    with tempfile.TemporaryDirectory(prefix='switchprep-') as temporary:
        workspace = Path(temporary)
        backend = Backend(backend_path, keys, title_keys, workspace, ui)
        titles, errors = scan(folder, backend, workspace, ui)
        if args.title_id:
            titles = [t for t in titles if t.application == int(args.title_id, 16)]
        ui.say(ui.tr(f'Found {len(titles)} unique titles; {len(errors)} skipped packages.', f'Detectados {len(titles)} contenidos únicos; {len(errors)} paquetes omitidos.'))
        if args.report:
            args.report.write_text(json.dumps({'titles': [{'id': f'{t.title_id:016X}', 'application': f'{t.application:016X}', 'kind': t.kind, 'version': t.version, 'package': t.package.name, 'contents': len(t.contents)} for t in titles], 'errors': errors}, ensure_ascii=False, indent=2), encoding='utf-8')
        if args.scan_only: return 0 if titles else 1
        bases = sorted([t for t in titles if t.kind == 0x80], key=lambda t: (t.title_id, -t.version))
        if not bases: raise PrepError('No readable base game found')
        base = choose(ui, ui.tr('Base game', 'Juego base'), bases)
        updates = sorted([t for t in titles if t.kind == 0x81 and t.application == base.title_id], key=lambda t: -t.version)
        update = choose(ui, ui.tr('Apply update over base game', 'Aplicar actualización sobre el juego base'), updates, True) if updates else None
        dlcs = choose_dlcs(ui, sorted([t for t in titles if t.kind == 0x82 and t.application == base.title_id], key=lambda t: (t.title_id, -t.version)))
        output = args.output or Path(ui.ask('Output parent folder', 'Carpeta de destino', str(folder.parent / 'SwitchPrep-output')))
        output = output.resolve()
        if output == folder or folder in output.parents:
            raise PrepError('Choose an output folder outside the scanned source tree')
        ui.say('\n' + base.label + ('\nUpdate: ' + update.label if update else '') + f'\nDLC: {len(dlcs)}\nOutput: {output}', '35')
        answer = ui.ask('Extract this selection? (yes/no)', '¿Extraer esta selección? (sí/no)', 'no').lower()
        if answer not in ('yes', 'y', 'si', 'sí', 's'): return 2
        final = extract(base, update, dlcs, output, workspace, backend, ui)
        if args.result:
            write_result(args.result, final, dlcs)
        ui.say(ui.tr('Ready: ', 'Preparado: ') + str(final), '32')
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print('\nCancelled / Cancelado.'); sys.exit(130)
    except (PrepError, OSError, ValueError, struct.error, UnicodeError) as error:
        print('\nError: ' + str(error)); sys.exit(1)
