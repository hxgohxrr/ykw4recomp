import json
import os
from pathlib import Path
import socket
import stat
import struct
import threading
import time


class DiscordPresence:
    def __init__(self, settings, process_id, dlc=False):
        self.settings, self.process_id, self.dlc = settings, process_id, dlc
        self.stopped = threading.Event()
        self.thread = None
        self.started = int(time.time())

    def start(self):
        if self.settings.get('enabled', '1') == '1':
            self.thread = threading.Thread(target=self._worker, daemon=True)
            self.thread.start()

    def stop(self):
        self.stopped.set()
        if self.thread:
            self.thread.join(timeout=3)

    @staticmethod
    def send(connection, opcode, data):
        payload = json.dumps(data, ensure_ascii=False).encode('utf-8')
        connection.sendall(struct.pack('<II', opcode, len(payload)) + payload)

    @staticmethod
    def receive(connection):
        def exact(size):
            result = bytearray()
            while len(result) < size:
                block = connection.recv(size - len(result))
                if not block:
                    raise OSError('Discord desconectado')
                result.extend(block)
            return result
        opcode, size = struct.unpack('<II', exact(8))
        if size > 65536:
            raise ValueError('Discord frame demasiado grande')
        return opcode, json.loads(exact(size).decode('utf-8'))

    def activity(self):
        activity = {'details': self.settings.get('details', 'YKW4Recomp')[:128]}
        context = self.settings.get('context', '')[:128]
        if context:
            activity['state'] = context
        if self.settings.get('show_time', '1') == '1':
            activity['timestamps'] = {'start': self.started}
        image = self.settings.get('dlc_image' if self.dlc else 'normal_image', '')[:128]
        if image:
            activity['assets'] = {'large_image': image, 'large_text': 'YKW4++' if self.dlc else 'YKW4'}
        return activity

    def _connect(self):
        roots = [os.environ.get('XDG_RUNTIME_DIR'), os.environ.get('TMPDIR'), f'/run/user/{os.getuid()}', f'/run/user/{os.getuid()}/app/com.discordapp.Discord', '/tmp']
        for root in dict.fromkeys(root for root in roots if root):
            for index in range(10):
                path = Path(root) / f'discord-ipc-{index}'
                try:
                    info = path.lstat()
                    if not stat.S_ISSOCK(info.st_mode) or info.st_uid != os.getuid():
                        continue
                    connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    connection.settimeout(1)
                    try:
                        connection.connect(str(path))
                        return connection
                    except BaseException:
                        connection.close()
                        raise
                except OSError:
                    continue
        raise OSError('Discord no está abierto en esta sesión Linux')

    def _worker(self):
        while not self.stopped.is_set():
            try:
                with self._connect() as connection:
                    self.send(connection, 0, {'v': 1, 'client_id': self.settings.get('application_id', '1561797893252317345')})
                    opcode, response = self.receive(connection)
                    if opcode != 1 or response.get('evt') != 'READY':
                        raise ValueError('Discord no confirmó el handshake')
                    while not self.stopped.is_set():
                        self.send(connection, 1, {'cmd': 'SET_ACTIVITY', 'args': {'pid': self.process_id, 'activity': self.activity()}, 'nonce': str(time.monotonic_ns())})
                        opcode, response = self.receive(connection)
                        if opcode == 2 or response.get('evt') == 'ERROR':
                            break
                        if opcode == 3:
                            self.send(connection, 4, response)
                        self.stopped.wait(15)
                    if self.stopped.is_set():
                        self.send(connection, 1, {'cmd': 'SET_ACTIVITY', 'args': {'pid': self.process_id, 'activity': None}, 'nonce': str(time.monotonic_ns())})
            except (OSError, ValueError, KeyError, TypeError):
                pass
            self.stopped.wait(5)
