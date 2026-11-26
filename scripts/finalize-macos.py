#!/usr/bin/env python3
"""Bundle OpenCV's transitive dependencies and verify a local macOS app."""
import argparse
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('bundle', type=Path)
parser.add_argument('--qt-prefix', type=Path, required=True)
parser.add_argument('--refresh-executable', type=Path)
args = parser.parse_args()
bundle = args.bundle.resolve()
if args.refresh_executable:
    shutil.copy2(args.refresh_executable, bundle / 'Contents/MacOS/ScanCleaner')
frameworks = bundle / 'Contents/Frameworks'
frameworks.mkdir(parents=True, exist_ok=True)
magic = {b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}

def macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open('rb') as stream:
        return stream.read(4) in magic

# Keep internal framework links, materialize links pointing outside the app.
for path in list(bundle.rglob('*')):
    if path.is_symlink() and path.exists() and not path.resolve().is_relative_to(bundle):
        source = path.resolve()
        path.unlink()
        if source.is_dir():
            shutil.copytree(source, path, symlinks=True)
        else:
            shutil.copy2(source, path)

seen = set()
while True:
    pending = [p for p in bundle.rglob('*') if p not in seen and macho(p)]
    if not pending:
        break
    for binary in pending:
        seen.add(binary)
        identities = subprocess.check_output(['otool', '-D', str(binary)], text=True).splitlines()[1:]
        if identities and binary.is_relative_to(frameworks):
            subprocess.run(['install_name_tool', '-id', '@rpath/' + str(binary.relative_to(frameworks)), str(binary)], check=True)
        changes = []
        for line in subprocess.check_output(['otool', '-L', str(binary)], text=True).splitlines()[1:]:
            dependency = line.strip().split(' (compatibility')[0]
            target = None
            match = re.search(r'(Qt[^/]+\.framework)/(Versions/[^/]+/Qt[^/]+)$', dependency)
            if match:
                name, tail = match.groups()
                target = frameworks / name / tail
                if not target.exists():
                    shutil.copytree((args.qt_prefix / 'lib' / name).resolve(), frameworks / name, symlinks=True)
            elif dependency.startswith(('/opt/homebrew/', '/usr/local/')):
                target = frameworks / Path(dependency).name
                if not target.exists():
                    shutil.copy2(Path(dependency).resolve(strict=True), target)
            if target:
                replacement = '@loader_path/' + os.path.relpath(target, binary.parent)
                if replacement != dependency:
                    changes += ['-change', dependency, replacement]
        if changes:
            subprocess.run(['install_name_tool', *changes, str(binary)], check=True)

minimum = (0, 0)
for binary in seen:
    text = subprocess.check_output(['otool', '-l', str(binary)], text=True)
    for version in re.findall(r'\bminos ([0-9.]+)', text):
        minimum = max(minimum, tuple(map(int, version.split('.'))))
    dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    assert '/opt/homebrew/' not in '\n'.join(dependencies.splitlines()[1:]), binary
    assert '/usr/local/' not in '\n'.join(dependencies.splitlines()[1:]), binary
info_path = bundle / 'Contents/Info.plist'
info = plistlib.loads(info_path.read_bytes())
info['LSMinimumSystemVersion'] = '.'.join(map(str, minimum))
info_path.chmod(info_path.stat().st_mode | 0o200)
info_path.write_bytes(plistlib.dumps(info))
assert not [p for p in bundle.rglob('*') if p.is_symlink() and not p.exists()], 'Dangling app links'
for binary in sorted(seen):
    subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(binary)], check=True, stdout=subprocess.DEVNULL)
containers = [p for p in bundle.rglob('*') if p.is_dir() and not p.is_symlink() and p.suffix in {'.app', '.framework'}]
for path in sorted(containers, key=lambda p: len(p.parts), reverse=True) + [bundle]:
    subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(path)], check=True, stdout=subprocess.DEVNULL)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
print(f'Verified {len(seen)} binaries; minimum macOS {info["LSMinimumSystemVersion"]}; no external Homebrew dependencies.')
