#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', required=True, choices=['linux', 'macos', 'ios', 'android'])
    parser.add_argument('--logs-only', action='store_true')
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('This uploader runs only inside GitHub Actions.')
    repo = os.environ['GITHUB_REPOSITORY']
    release = os.environ['DRAFT_RELEASE_ID']
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo) or not release.isdecimal():
        raise SystemExit('Invalid repository or draft release ID')
    token = os.environ['GITHUB_TOKEN']
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'User-Agent': 'yw4port-ci-packager'}
    api = 'https://api.github.com/repos/' + repo + '/releases/' + release

    def get(url):
        with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as r:
            return json.load(r)

    release_data = get(api)
    if not release_data.get('draft'):
        raise SystemExit('Refusing to change a published release')

    def upload(path, shared=False):
        if path.stat().st_size >= 2 * 1024**3:
            raise SystemExit('Release asset is too large: ' + path.name)
        expected = digest(path)
        for attempt in range(15):
            assets = get(api + '/assets?per_page=100')
            existing = next((a for a in assets if a['name'] == path.name), None)
            if existing:
                if existing['state'] == 'uploaded':
                    if existing['size'] != path.stat().st_size or (existing.get('digest') and existing['digest'] != 'sha256:' + expected):
                        raise SystemExit('Existing release asset differs: ' + path.name)
                    print('Asset already present: ' + path.name, flush=True)
                    return
                time.sleep(4)
                continue
            url = 'https://uploads.github.com/repos/' + repo + '/releases/' + release + '/assets?' + urllib.parse.urlencode({'name': path.name})
            upload_headers = dict(headers, **{'Content-Type': 'application/octet-stream', 'Content-Length': str(path.stat().st_size)})
            try:
                with path.open('rb') as data:
                    request = urllib.request.Request(url, data=data, headers=upload_headers, method='POST')
                    with urllib.request.urlopen(request, timeout=1200) as response:
                        uploaded = json.load(response)
                if uploaded['size'] != path.stat().st_size:
                    raise SystemExit('Upload size mismatch')
                print('Uploaded: ' + path.name, flush=True)
                return
            except urllib.error.HTTPError as error:
                if shared and error.code == 422:
                    time.sleep(4)
                    continue
                raise SystemExit('Release upload failed, HTTP ' + str(error.code) + ': ' + path.name) from None
        raise SystemExit('Another upload did not finish: ' + path.name)

    root = Path(os.environ['GITHUB_WORKSPACE']).resolve()
    sha = subprocess.check_output(['git', '-c', 'safe.directory=' + str(root), 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    run_id = os.environ['GITHUB_RUN_ID'] + '-' + os.environ.get('GITHUB_RUN_ATTEMPT', '1')
    stem = 'experimental-' + args.platform + '-' + sha[:8] + '-' + run_id
    output = root / 'ci-package' / args.platform
    output.mkdir(parents=True, exist_ok=True)
    logs = output / (stem + '-logs.tar.gz')
    with tarfile.open(logs, 'w:gz') as archive:
        for name in ['configure.txt', 'compile.txt', 'llvm.txt']:
            path = root / name
            if path.is_file():
                archive.add(path, arcname=name)
    if args.logs_only:
        upload(logs)
        return

    mobile_asset = None
    archive_path = output / (stem + '.tar.gz')
    if args.platform == 'android':
        payload = root / 'build/android/stage/gradle/outputs/apk/debug/YKW4Recomp-debug.apk'
        if not payload.is_file():
            raise SystemExit('Expected debug APK is missing')
        mobile_asset = output / (stem + ".apk")
        import shutil
        shutil.copy2(payload, mobile_asset)
        payloads = [payload]
        prefix = payload.parent
    else:
        prefix = root / 'stage'
        payloads = sorted(prefix.rglob('*'))
        if not any(p.is_file() and p.name in ['YKW4Recomp', 'YKW4Recomp-gui'] for p in payloads):
            raise SystemExit('Installed runtime is missing')
    if args.platform == 'ios':
        import shutil
        import zipfile
        ipa = root / 'app/YKW4Recomp.ipa'
        with zipfile.ZipFile(ipa) as archive:
            if archive.testzip() or 'Payload/YKW4Recomp.app/Info.plist' not in archive.namelist():
                raise SystemExit('Invalid IPA payload')
        mobile_asset = output / (stem + '-unsigned.ipa')
        shutil.copy2(ipa, mobile_asset)
    manifest = {'platform': args.platform, 'source_commit': sha, 'run_id': run_id,
                'experimental': True, 'gameplay_verified': False, 'files': []}
    forbidden = {'.nsp', '.xci', '.nca', '.nso', '.npdm', '.keys', '.jitcache'}
    with tarfile.open(archive_path, 'w:gz', dereference=False) as archive:
        for path in payloads:
            if path.suffix.lower() in forbidden:
                raise SystemExit('Forbidden game/key content in install stage')
            if path.is_symlink():
                path.resolve().relative_to(prefix.resolve())
            if path.is_file() or path.is_symlink():
                relative = path.relative_to(prefix).as_posix()
                archive.add(path, arcname=relative, recursive=False)
                if not path.is_symlink():
                    manifest['files'].append({'path': relative, 'bytes': path.stat().st_size, 'sha256': digest(path)})
        for name in ['LICENSE', 'LICENSE.apache', 'NOTICE', 'docs/CREDITS.md']:
            path = root / name
            relative = 'project-notices/' + name
            archive.add(path, arcname=relative, recursive=False)
            manifest['files'].append({'path': relative, 'bytes': path.stat().st_size, 'sha256': digest(path)})
        if args.platform == 'android':
            licenses = root / 'build/android/stage/share/licenses'
            if not licenses.is_dir():
                raise SystemExit('Installed dependency licenses are missing')
            for path in sorted(licenses.rglob('*')):
                if path.is_file() and not path.is_symlink():
                    relative = 'licenses/' + path.relative_to(licenses).as_posix()
                    archive.add(path, arcname=relative, recursive=False)
                    manifest['files'].append({'path': relative, 'bytes': path.stat().st_size, 'sha256': digest(path)})
    manifest_path = output / (stem + '-manifest.json')
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

    sources = output / (stem + '-source.tar.gz')
    source_manifest = {'commit': sha, 'dependencies': []}
    with tarfile.open(sources, 'w:gz', dereference=False) as archive:
        names = subprocess.check_output(['git', '-c', 'safe.directory=' + str(root), 'ls-files', '-z'], cwd=root).decode('utf-8').split('\0')
        for name in names:
            if name:
                archive.add(root / name, arcname='source/' + name, recursive=False)
        cache = root / '.cache/cpm'
        if cache.is_dir():
            for folder, dirs, files in os.walk(cache, followlinks=False):
                dirs[:] = [name for name in dirs if name != '.git' and not (Path(folder) / name).is_symlink()]
                for name in files:
                    path = Path(folder) / name
                    if not path.is_symlink():
                        archive.add(path, arcname='dependency-sources/' + path.relative_to(cache).as_posix(), recursive=False)
            source_manifest['dependencies'].append('Pinned CPM dependency sources are included; recipes and hashes are in cmake/dependencies.cmake.')


    if args.platform in ['macos', 'ios', 'android']:
        version = '23.1.3'
        filename = 'llvm-project-' + version + '.src.tar.xz'
        found = list(root.glob('*llvm*.build/' + version + '/' + filename))
        found += list(root.glob('android-llvm/*.build/' + version + '/' + filename))
        llvm_source = found[0] if found else output / filename
        if not llvm_source.exists():
            url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/' + filename
            urllib.request.urlretrieve(url, llvm_source)
        if llvm_source.name != filename:
            raise SystemExit('Unexpected LLVM source filename')
        source_manifest['dependencies'].append({'name': filename, 'sha256': digest(llvm_source)})
        upload(llvm_source, shared=True)
    manifest['source'] = source_manifest
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    checksum = output / (stem + '-SHA256SUMS.txt')
    assets = [archive_path, sources, manifest_path, logs]
    if mobile_asset:
        manifest['mobile_package'] = {'name': mobile_asset.name, 'sha256': digest(mobile_asset), 'unsigned': args.platform == 'ios'}
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        assets.append(mobile_asset)
    checksum.write_text(''.join(digest(p) + '  ' + p.name + '\n' for p in assets), encoding='ascii')
    for path in [sources, archive_path, manifest_path, checksum, logs] + ([mobile_asset] if mobile_asset else []):
        upload(path)


if __name__ == '__main__':
    main()
