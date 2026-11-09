import argparse
import os
from pathlib import Path
import shutil
import subprocess
import struct
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--fixture', required=True, type=Path)
    args = parser.parse_args()
    runtime, fixture = args.runtime.resolve(), args.fixture.resolve()
    with tempfile.TemporaryDirectory(prefix='ykw4-runtime-binding-') as temporary:
        folder = Path(temporary)
        module = folder / ('synthetic_code.dll' if os.name == 'nt' else 'synthetic_code.so')
        shutil.copyfile(fixture, module)
        env = dict(os.environ)
        if os.name != 'nt':
            env['HOME'] = str(folder)
        def invoke(*arguments, expected=0):
            result = subprocess.run([str(runtime), *map(str, arguments)], env=env, capture_output=True, text=True, timeout=30)
            if (result.returncode == 0) != (expected == 0):
                raise AssertionError(f'{arguments[0]} returned {result.returncode}: {result.stderr}')
            return result
        invoke('--machine-status')
        assert invoke('--binding-status', module).stdout.strip() == 'legacy'
        original = module.read_bytes()
        invoke('--binding-seal', module, expected=1)
        assert original == module.read_bytes()
        invoke('--binding-check', module)
        invoke('--binding-seal', module, '--accept-local-binding')
        assert module.read_bytes() != original
        assert invoke('--binding-status', module).stdout.strip() == 'bound-or-new'
        invoke('--binding-check', module)
        loaded = invoke('--headless', '--game=' + str(folder / 'synthetic'), expected=1)
        assert 'is not a YKW4Recomp module' in loaded.stdout + loaded.stderr
        invoke('--binding-seal', module, '--accept-local-binding')
        damaged = bytearray(module.read_bytes())
        damaged[len(damaged) // 2] ^= 1
        module.write_bytes(damaged)
        invoke('--binding-check', module, expected=1)
        invoke('--binding-seal', module, '--accept-local-binding', expected=1)
        source = folder / 'romfs'
        source.mkdir()
        (source / 'fixture.txt').write_text('synthetic fixture, not game content')
        image = folder / 'romfs.bin'
        invoke('--launcher-pack-romfs', source, image)
        assert struct.unpack('<Q', image.read_bytes()[:8])[0] == 80
        exefs = folder / 'exefs'
        exefs.mkdir()
        invoke('--launcher-validate-exefs', exefs, expected=1)
        (exefs / 'main').write_bytes(b'synthetic metadata fixture only')
        metadata = bytearray(0x98)
        metadata[:4] = b'META'
        metadata[12] = 1
        struct.pack_into('<II', metadata, 0x70, 0x80, 0x18)
        metadata[0x80:0x84] = b'ACI0'
        struct.pack_into('<Q', metadata, 0x90, 0x010086c00af7c000)
        (exefs / 'main.npdm').write_bytes(metadata)
        invoke('--launcher-validate-exefs', exefs)
        print('Built runtime: legacy load, consent requirement, binding, reload and tamper rejection OK; no game used')


if __name__ == '__main__':
    main()
