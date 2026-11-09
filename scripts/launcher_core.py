import configparser
from collections import deque
from dataclasses import dataclass
import json
import os
from pathlib import Path, PurePosixPath
import queue
import re
import shutil
import signal
import stat
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import zipfile
import urllib.parse
import urllib.request


class LauncherEvents:
    def __init__(self, log_limit=500, line_limit=4096):
        if log_limit < 1 or line_limit < 1:
            raise ValueError('Los limites del registro deben ser positivos.')
        self.control = queue.SimpleQueue()
        self.logs = deque(maxlen=log_limit)
        self.line_limit = line_limit
        self.lock = threading.Lock()
        self.dropped = 0

    def put(self, event):
        kind, value = event
        if kind != 'log':
            self.control.put(event)
            return
        with self.lock:
            if len(self.logs) == self.logs.maxlen:
                self.dropped += 1
            self.logs.append(str(value)[:self.line_limit])

    def get_nowait(self):
        try:
            return self.control.get_nowait()
        except queue.Empty:
            pass
        with self.lock:
            if self.dropped:
                dropped, self.dropped = self.dropped, 0
                return 'log', f'Registro saturado: {dropped} mensajes antiguos omitidos en el launcher. El registro del runtime se conserva.'
            if not self.logs:
                raise queue.Empty
            return 'log', self.logs.popleft()


def graphics_summary(devices):
    eligible = [device for device in devices if device['api_version'] >= (1 << 22 | 3 << 12)]
    hardware = [device for device in eligible if device['device_type'] != 4]
    return {'status': 'hardware' if hardware else 'software' if eligible else 'unavailable', 'devices': devices}


def vulkan_devices():
    import ctypes
    class Application(ctypes.Structure):
        _fields_ = [('sType', ctypes.c_uint32), ('pNext', ctypes.c_void_p), ('pApplicationName', ctypes.c_char_p), ('applicationVersion', ctypes.c_uint32), ('pEngineName', ctypes.c_char_p), ('engineVersion', ctypes.c_uint32), ('apiVersion', ctypes.c_uint32)]
    class InstanceCreate(ctypes.Structure):
        _fields_ = [('sType', ctypes.c_uint32), ('pNext', ctypes.c_void_p), ('flags', ctypes.c_uint32), ('pApplicationInfo', ctypes.POINTER(Application)), ('enabledLayerCount', ctypes.c_uint32), ('ppEnabledLayerNames', ctypes.c_void_p), ('enabledExtensionCount', ctypes.c_uint32), ('ppEnabledExtensionNames', ctypes.c_void_p)]
    library = ctypes.CDLL('libvulkan.so.1')
    library.vkCreateInstance.argtypes = [ctypes.POINTER(InstanceCreate), ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    library.vkCreateInstance.restype = ctypes.c_int32
    library.vkEnumeratePhysicalDevices.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
    library.vkEnumeratePhysicalDevices.restype = ctypes.c_int32
    library.vkGetPhysicalDeviceProperties.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    library.vkGetPhysicalDeviceProperties.restype = None
    library.vkDestroyInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    library.vkDestroyInstance.restype = None
    application = Application(sType=0, pApplicationName=b'YKW4 graphics check', apiVersion=(1 << 22 | 3 << 12))
    create = InstanceCreate(sType=1, pApplicationInfo=ctypes.pointer(application))
    instance = ctypes.c_void_p()
    status = library.vkCreateInstance(ctypes.byref(create), None, ctypes.byref(instance))
    if status:
        raise RuntimeError('Vulkan no pudo crear la instancia: ' + str(status))
    try:
        count = ctypes.c_uint32()
        if library.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), None) or count.value > 64:
            raise RuntimeError('Vulkan no pudo enumerar los adaptadores.')
        if not count.value:
            return []
        handles = (ctypes.c_void_p * count.value)()
        if library.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), handles):
            raise RuntimeError('La lista de adaptadores Vulkan ha cambiado.')
        devices = []
        for handle in handles[:count.value]:
            properties = (ctypes.c_uint64 * 512)()
            library.vkGetPhysicalDeviceProperties(handle, properties)
            raw = bytes(properties)
            fields = (ctypes.c_uint32 * 5).from_buffer_copy(raw[:20])
            devices.append({'api_version': fields[0], 'device_type': fields[4], 'name': raw[20:276].split(b'\0', 1)[0].decode('utf-8', errors='replace')})
        return devices
    finally:
        library.vkDestroyInstance(instance, None)


def graphics_info(timeout=5):
    try:
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--graphics-info'], capture_output=True, text=True, check=True, timeout=timeout)
        information = json.loads(result.stdout)
        if not isinstance(information, dict) or information.get('status') not in ('hardware', 'software', 'unavailable'):
            raise ValueError('Respuesta de diagnostico no valida.')
        return information
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        return {'status': 'unavailable', 'devices': [], 'reason': str(error)[:200]}


class LinuxGamepad:
    def __init__(self):
        self.descriptors = {}
        self.scan_at = 0
        self.moved_at = 0

    @staticmethod
    def action(event):
        unused, value, kind, number = struct.unpack('<IhBB', event)
        if kind & 0x80:
            return None
        if kind == 1 and value == 1:
            return {0: 'accept', 1: 'cancel'}.get(number)
        if kind == 2 and number in (1, 7) and abs(value) > 20000:
            return 'previous' if value < 0 else 'next'
        return None

    def poll(self):
        now = time.monotonic()
        if now >= self.scan_at:
            self.scan_at = now + 2
            for path in Path('/dev/input').glob('js[0-3]'):
                if path in self.descriptors:
                    continue
                try:
                    descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC | os.O_NOFOLLOW)
                    if not stat.S_ISCHR(os.fstat(descriptor).st_mode):
                        os.close(descriptor)
                        continue
                    self.descriptors[path] = descriptor
                except OSError:
                    pass
        actions = []
        for path, descriptor in list(self.descriptors.items()):
            try:
                data = os.read(descriptor, 8 * 64)
                if not data:
                    raise OSError('Mando desconectado')
                for offset in range(0, len(data) - 7, 8):
                    action = self.action(data[offset:offset + 8])
                    if action in ('next', 'previous'):
                        if now - self.moved_at < 0.15:
                            continue
                        self.moved_at = now
                    if action:
                        actions.append(action)
            except BlockingIOError:
                pass
            except OSError:
                os.close(descriptor)
                del self.descriptors[path]
        return actions

    def close(self):
        for descriptor in self.descriptors.values():
            os.close(descriptor)
        self.descriptors.clear()


def installation_config():
    return Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config'))) / 'ykw4recomp/launcher.json'


def installation_directory(runtime, explicit=None):
    if explicit is not None:
        return Path(explicit).resolve()
    portable = Path(runtime).resolve().parent
    if (portable / 'YKW4_code.so').is_file():
        return portable
    try:
        saved = json.loads(installation_config().read_text(encoding='utf-8'))['installation']
        if isinstance(saved, str) and Path(saved).is_absolute():
            return Path(saved).resolve()
    except (OSError, ValueError, KeyError, TypeError):
        pass
    if os.access(portable, os.W_OK):
        return portable
    return Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'ykw4recomp/game'


def remember_installation(base):
    atomic_write(installation_config(), json.dumps({'installation': str(Path(base).resolve())}, ensure_ascii=False) + '\n')


def read_ini(path):
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    path = Path(path)
    if path.is_file():
        raw = path.read_bytes()
        parser.read_string(raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'))
    return parser


def atomic_write(path, text, encoding='utf-8'):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix='.' + path.name, dir=path.parent)
    try:
        with os.fdopen(descriptor, 'w', encoding=encoding, newline='') as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def update_ini(path, sections):
    path = Path(path)
    raw = path.read_bytes() if path.exists() else b''
    encoding = 'utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig' if raw.startswith(b'\xef\xbb\xbf') else 'utf-8'
    text = raw.decode(encoding)
    original = text
    for section, values in sections.items():
        match = re.search(r'^\[' + re.escape(section) + r'\][ \t]*\r?$', text, re.M | re.I)
        if not match:
            text = text.rstrip() + '\n\n[' + section + ']\n'
            match = re.search(r'^\[' + re.escape(section) + r'\][ \t]*$', text, re.M | re.I)
        following = re.search(r'^\[', text[match.end():], re.M)
        end = match.end() + following.start() if following else len(text)
        block = text[match.end():end]
        for key, value in values.items():
            if any(character in str(value) for character in '\r\n\x00'):
                raise ValueError('Un ajuste contiene caracteres no permitidos.')
            pattern = r'^([ \t]*' + re.escape(key) + r'[ \t]*=)[^\r\n]*'
            if re.search(pattern, block, re.M | re.I):
                block = re.sub(pattern, lambda found: found[1] + str(value), block, flags=re.M | re.I)
            else:
                block = block.rstrip() + '\n' + key + '=' + str(value) + '\n'
        text = text[:match.end()] + block + text[end:]
    if text != original or not path.exists() or path.is_symlink():
        atomic_write(path, text, encoding)


def validate_settings(values):
    fps = str(values['fps'])
    if fps != 'game' and (not fps.isdecimal() or (int(fps) != 0 and not 10 <= int(fps) <= 1000)):
        raise ValueError('FPS: usa game, 0 o un número entre 10 y 1000.')
    for name, maximum in (('deadzone', 90), ('rumble', 100)):
        if not str(values[name]).isdecimal() or not 0 <= int(values[name]) <= maximum:
            raise ValueError('Zona muerta: 0–90. Vibración: 0–100.')
    if str(values['resolution']) not in ('0.5', '0.75', '1', '1.5', '2'):
        raise ValueError('Selecciona una resolución válida.')
    return {'Performance': {'fps_limit': fps}, 'Graphics': {'resolution': values['resolution'], 'vsync': int(values['vsync']), 'shadows': int(values['shadows'])}, 'Display': {'fullscreen': int(values['fullscreen'])}, 'Controllers': {'gamepad_deadzone': values['deadzone'], 'gamepad_rumble': values['rumble'], 'gamepad_swap_ab': int(values['swap'])}, 'Launcher': {'translation': values['translation']}}


def mod_key(identifier):
    digest = 1469598103934665603
    encoded = identifier.encode('utf-16-le')
    for index in range(0, len(encoded), 2):
        digest = ((digest ^ int.from_bytes(encoded[index:index + 2], 'little')) * 1099511628211) & 0xffffffffffffffff
    return f'{digest:016x}'


@dataclass
class Mod:
    identifier: str
    name: str
    enabled: bool = False
    translation: bool = False
    spanish: bool = False


def valid_mod_id(identifier):
    return bool(identifier) and len(identifier) < 128 and not identifier.startswith('.') and not any(character in identifier for character in '/\\:*?"<>|\r\n\t')


def scan_mods(base):
    base = Path(base)
    settings = read_ini(base / 'YKW4.ini')
    section = settings['Mods'] if settings.has_section('Mods') else {}
    root = base / 'mods'
    if root.is_symlink():
        raise ValueError('La carpeta mods no puede ser un enlace.')
    order = section.get('order', '').split(',')
    positions = {}
    for index, key in enumerate(order):
        positions.setdefault(key, index)
    candidates = []
    for folder in sorted(root.iterdir()) if root.is_dir() else []:
        if not valid_mod_id(folder.name) or folder.is_symlink() or not folder.is_dir() or not (folder / 'romfs').is_dir() or (folder / 'romfs').is_symlink():
            continue
        candidates.append((folder, mod_key(folder.name)))
    candidates.sort(key=lambda item: positions.get(item[1], len(order)))
    mods = []
    for folder, key in candidates[:128]:
        meta = read_ini(folder / 'mod.ini')
        values = meta['Mod'] if meta.has_section('Mod') else {}
        mods.append(Mod(folder.name, values.get('name', folder.name), section.get(key, section.get(folder.name, '0')) == '1', values.get('translation', '0') == '1', values.get('rey3ds_spanish', '0') == '1'))
    return mods


def save_mods(base, mods, sections=None):
    values = {mod_key(mod.identifier): int(mod.enabled) for mod in mods}
    values['order'] = ','.join(mod_key(mod.identifier) for mod in mods)
    updates = dict(sections or {})
    updates['Mods'] = {**updates.get('Mods', {}), **values}
    update_ini(Path(base) / 'YKW4.ini', updates)
    active = Path(base) / 'mods/active.txt'
    text = ''.join(mod.identifier + '\n' for mod in mods if mod.enabled)
    if not active.is_file() or active.is_symlink() or active.read_bytes() != text.encode('utf-8'):
        atomic_write(active, text)


def checked_tree(root):
    root = Path(root)
    if root.is_symlink() or not root.is_dir():
        raise ValueError('Selecciona una carpeta real, no un enlace.')
    for current, folders, files in os.walk(root, followlinks=False):
        for name in folders + files:
            path = Path(current) / name
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise ValueError('No se admiten enlaces ni archivos especiales: ' + str(path))


def find_romfs(root):
    root = Path(root)
    checked_tree(root)
    matches = [root] if root.name.lower() == 'romfs' else [path for path in root.rglob('*') if path.is_dir() and path.name.lower() == 'romfs']
    if len(matches) != 1:
        raise ValueError('Elige un paquete con una sola carpeta romfs.')
    return matches[0]


def import_mod(base, source, translation=False, spanish=False):
    base, source = Path(base), Path(source)
    mods = base / 'mods'
    if mods.is_symlink():
        raise ValueError('La carpeta mods no puede ser un enlace.')
    mods.mkdir(exist_ok=True)
    if mods.resolve() == source.resolve() or mods.resolve() in source.resolve().parents or source.resolve() in mods.resolve().parents:
        raise ValueError('Selecciona un paquete externo a la carpeta mods.')
    with tempfile.TemporaryDirectory(prefix='.import-', dir=mods) as work:
        work = Path(work)
        if source.suffix.lower() == '.zip':
            with zipfile.ZipFile(source) as archive:
                entries = archive.infolist()
                names = set()
                if len(entries) > 100000 or sum(entry.file_size for entry in entries) > 8 * 1024**3:
                    raise ValueError('El ZIP supera el límite de tamaño o archivos.')
                for entry in entries:
                    name = PurePosixPath(entry.filename.replace('\\', '/'))
                    mode = entry.external_attr >> 16
                    if name.is_absolute() or '..' in name.parts or ':' in str(name) or '\x00' in entry.filename or stat.S_ISLNK(mode) or (stat.S_IFMT(mode) and not (stat.S_ISREG(mode) or stat.S_ISDIR(mode))) or str(name).casefold() in names:
                        raise ValueError('ZIP inseguro: rutas, enlaces o nombres duplicados.')
                    names.add(str(name).casefold())
                archive.extractall(work / 'zip')
            payload = find_romfs(work / 'zip')
        else:
            payload = find_romfs(source)
        staging = work / 'mod'
        staging.mkdir()
        shutil.copytree(payload, staging / 'romfs')
        update_ini(staging / 'mod.ini', {'Mod': {'name': source.stem, 'translation': int(translation), 'rey3ds_spanish': int(spanish)}})
        identifier = 'mod-' + uuid.uuid4().hex[:16]
        staging.rename(mods / identifier)
    return identifier


def allowed_download(url):
    parsed = urllib.parse.urlsplit(url)
    return parsed.scheme == 'https' and parsed.hostname in ('gamebanana.com', 'files.gamebanana.com') and parsed.port in (None, 443) and not parsed.username and not parsed.password


class GameBananaRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, new_url):
        if not allowed_download(new_url):
            raise ValueError('GameBanana redirigió a un servidor no autorizado.')
        return super().redirect_request(request, response, code, message, headers, new_url)


def gamebanana_json(identifier=None):
    if identifier is not None and (not isinstance(identifier, int) or identifier <= 0):
        raise ValueError('Identificador de mod no válido.')
    url = f'https://gamebanana.com/apiv11/Mod/{identifier}/ProfilePage' if identifier else 'https://gamebanana.com/apiv11/Mod/Index?_aFilters%5BGeneric_Game%5D=16848&_nPerpage=50'
    opener = urllib.request.build_opener(GameBananaRedirect())
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YKW4Recomp', 'Accept': 'application/json'}), timeout=20) as response:
        raw = response.read(2 * 1024**2 + 1)
    if len(raw) > 2 * 1024**2:
        raise ValueError('La respuesta del catálogo es demasiado grande.')
    result = json.loads(raw)
    if identifier:
        if result.get('_aGame', {}).get('_idRow') != 16848 or result.get('_bIsWithheld') or result.get('_bIsTrashed'):
            raise ValueError('Este mod no está disponible para YKW4++.')
        return [entry for entry in result.get('_aFiles', []) if str(entry.get('_sFile', '')).lower().endswith('.zip') and not entry.get('_bIsArchived') and isinstance(entry.get('_nFilesize'), (int, float)) and 0 < entry['_nFilesize'] <= 2 * 1024**3]
    return [entry for entry in result.get('_aRecords', []) if entry.get('_aGame', {}).get('_idRow') == 16848 and isinstance(entry.get('_idRow'), int) and entry['_idRow'] > 0][:100]


def install_gamebanana(base, entry, translation=False):
    url = entry.get('_sDownloadUrl', '')
    if not allowed_download(url):
        raise ValueError('Solo se permiten descargas HTTPS desde GameBanana.')
    opener = urllib.request.build_opener(GameBananaRedirect())
    with tempfile.TemporaryDirectory(prefix='ykw4-mod-download-') as temporary:
        archive = Path(temporary) / 'GameBanana.zip'
        with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YKW4Recomp'}), timeout=20) as response, archive.open('xb') as output:
            total = 0
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                total += len(block)
                if total > 2 * 1024**3:
                    raise ValueError('La descarga supera 2 GiB.')
                output.write(block)
        return import_mod(base, archive, translation=translation)


def publish_files(base, replacements, backup):
    base, backup = Path(base), Path(backup)
    backup.mkdir()
    moved = []
    try:
        for index, (source, relative) in enumerate(replacements):
            target = base / relative
            if target.is_symlink() or any(parent.is_symlink() for parent in target.parents if parent != base.parent):
                raise ValueError('La instalación contiene enlaces; no se publicará encima.')
            target.parent.mkdir(parents=True, exist_ok=True)
            previous = backup / str(index)
            existed = target.exists()
            if existed:
                target.rename(previous)
            moved.append((Path(source), target, previous, existed, False))
            Path(source).rename(target)
            moved[-1] = (*moved[-1][:4], True)
    except BaseException:
        for source, target, previous, existed, published in reversed(moved):
            if published:
                target.rename(source)
            if existed:
                previous.rename(target)
        raise


class Preparation:
    def __init__(self, runtime, base, emit=print):
        self.runtime, self.base, self.emit = Path(runtime).resolve(), Path(base).resolve(), emit
        self.cancelled = threading.Event()
        self.lock = threading.Lock()
        self.process = None

    def cancel(self):
        with self.lock:
            self.cancelled.set()
            if self.process and self.process.poll() is None:
                process = self.process
                try:
                    os.killpg(self.process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                def force_stop():
                    with self.lock:
                        if process.poll() is None:
                            try:
                                os.killpg(process.pid, signal.SIGKILL)
                            except ProcessLookupError:
                                pass
                timer = threading.Timer(3, force_stop)
                timer.daemon = True
                timer.start()

    def run(self, command):
        with self.lock:
            if self.cancelled.is_set():
                raise RuntimeError('Preparación cancelada; instalación sin publicar.')
            self.process = subprocess.Popen([str(part) for part in command], cwd=self.base, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace', start_new_session=True)
        with self.process.stdout:
            for line in self.process.stdout:
                self.emit(line.rstrip())
        status = self.process.wait()
        if status or self.cancelled.is_set():
            raise RuntimeError('La herramienta ha fallado o se ha cancelado. Consulta el registro de esta preparación.')

    def prepare(self, selection, accepted=False):
        if not accepted:
            raise ValueError('Debes confirmar la preparación y su vinculación antes de continuar.')
        self.base.mkdir(parents=True, exist_ok=True)
        lock = self.base / '.launcher-setup.lock'
        descriptor = os.open(lock, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        import fcntl
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self._prepare(selection)
        finally:
            os.close(descriptor)

    def _prepare(self, selection):
        code = self.base / 'YKW4_code.so'
        policy = subprocess.run([str(self.runtime), '--binding-status', str(code)], check=True, capture_output=True, text=True).stdout.strip()
        bind = policy == 'bound-or-new'
        if bind:
            self.run([self.runtime, '--machine-status'])
        work = self.base / 'setup-work' / uuid.uuid4().hex
        work.mkdir(parents=True)
        previous_emit = self.emit
        with (work / 'setup.log').open('w', encoding='utf-8', buffering=1) as log:
            def logged(line):
                log.write(line + '\n')
                previous_emit(line)
            self.emit = logged
            self.emit('Preparación local. Registro: ' + str(work / 'setup.log'))
            try:
                exefs = Path(selection['exefs']) if selection.get('exefs') else None
                romfs = Path(selection['romfs']) if selection.get('romfs') else None
                dlc = Path(selection['dlc']) if selection.get('dlc') else None
                if selection.get('package'):
                    helper = Path(__file__).parent / 'switchprep.py'
                    if not helper.is_file():
                        helper = Path(__file__).parent / 'switchprep/switchprep.py'
                    request = {'base': selection['package'], 'update': selection.get('update', ''), 'keys': selection['keys'], 'title_keys': selection.get('title_keys', ''), 'dlc_count': len(selection.get('dlcs', []))}
                    request.update({'dlc' + str(index): path for index, path in enumerate(selection.get('dlcs', []))})
                    update_ini(work / 'selection.ini', {'Selection': request})
                    (work / 'selection.ini').write_text((work / 'selection.ini').read_text(encoding='utf-8'), encoding='utf-16')
                    self.run([sys.executable, helper, '--request', work / 'selection.ini', '--result', work / 'prepared.ini', '--output', work / 'imported', '--backend', selection['backend'], '--title-id', '010086C00AF7C000', '--lang', 'es'])
                    paths = read_ini(work / 'prepared.ini')['Paths']
                    exefs, romfs = Path(paths['exefs']), Path(paths['romfs'])
                    dlc = Path(paths['dlc']) if paths.get('dlc') else None
                if exefs is None or romfs is None:
                    raise ValueError('Selecciona ExeFS y RomFS, o un NSP/XCI con tus claves.')
                checked_tree(exefs)
                if not all((exefs / name).is_file() for name in ('main', 'rtld', 'sdk', 'main.npdm')):
                    raise ValueError('ExeFS incompleto: se necesitan main, rtld, sdk y main.npdm.')
                input_exefs = work / 'input/exefs'
                shutil.copytree(exefs, input_exefs)
                self.run([self.runtime, '--launcher-validate-exefs', input_exefs])
                self.run([self.runtime, '--repo', work, '--build-dir', work, '-r', input_exefs, 'YKW4', '--outdir', work / 'compiled', '--runtime', self.runtime, '--jobs', '4', '-O', '3'])
                generated = work / 'compiled/YKW4'
                module = generated / 'YKW4_code.so'
                if not module.is_file():
                    raise ValueError('La compilación no ha generado YKW4_code.so.')
                data = work / 'data'
                data.mkdir()
                shutil.copytree(input_exefs, data / 'exefs')
                if romfs.is_dir():
                    checked_tree(romfs)
                    self.run([self.runtime, '--launcher-pack-romfs', romfs, data / 'romfs.bin'])
                else:
                    if romfs.is_symlink() or not romfs.is_file():
                        raise ValueError('Selecciona una imagen RomFS válida.')
                    shutil.copyfile(romfs, data / 'romfs.bin')
                replacements = [(data / 'exefs', 'YKW4_data/exefs'), (data / 'romfs.bin', 'YKW4_data/romfs.bin'), (module, 'YKW4_code.so')]
                if dlc:
                    checked_tree(dlc)
                    archives = data / 'archives'
                    old_archives = self.base / 'YKW4_data/system/archives'
                    if old_archives.exists():
                        checked_tree(old_archives)
                        shutil.copytree(old_archives, archives)
                    else:
                        archives.mkdir()
                    for payload in dlc.iterdir():
                        identifier = payload.stem if payload.is_file() else payload.name
                        if not re.fullmatch('[0-9a-fA-F]{16}', identifier) or int(identifier, 16) & ~0xfff != 0x010086c00af7d000:
                            raise ValueError('DLC no compatible con YKW4++.')
                        target = archives / (identifier.lower() + '.romfs')
                        if payload.is_dir():
                            self.run([self.runtime, '--launcher-pack-romfs', payload / 'romfs', target])
                        else:
                            shutil.copyfile(payload, target)
                    replacements.append((archives, 'YKW4_data/system/archives'))
                if bind:
                    self.run([self.runtime, '--binding-seal', module, '--accept-local-binding'])
                if self.cancelled.is_set():
                    raise RuntimeError('Cancelado antes de publicar.')
                publish_files(self.base, replacements, work / 'previous')
                atomic_write(work / 'result.json', json.dumps({'status': 'success', 'binding': bind, 'platform': 'linux', 'no_gameplay_validation': True}, indent=2))
                self.emit('Instalación preparada. Guardados, mods y traducciones conservados.')
            finally:
                self.emit = previous_emit


if __name__ == '__main__' and sys.argv[1:] == ['--graphics-info']:
    try:
        information = graphics_summary(vulkan_devices())
    except Exception as error:
        information = {'status': 'unavailable', 'devices': [], 'reason': str(error)[:200]}
    print(json.dumps(information))
