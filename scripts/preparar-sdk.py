#!/usr/bin/env python3
"""Reuse a Vulkan SDK or download and verify the tested LunarG installer."""
from pathlib import Path
import hashlib
import os
import subprocess
import urllib.request
from port_paths import ROOT

version = '1.4.363.0'
expected = '163431e0203c8215286a17cf393643ce6b951f77d1b4f74ab30821a1f0beb09b'
choices = [Path(os.environ['VULKAN_SDK'])] if os.environ.get('VULKAN_SDK') else []
choices += [ROOT / '.tools/vulkan-sdk/macOS', ROOT / 'vulkan-sdk/macOS']
for path in choices:
    if (path / 'bin/dxc').is_file():
        print('Vulkan SDK existente: ' + str(path))
        raise SystemExit(0)
cache = ROOT / '.cache/downloads'
cache.mkdir(parents=True, exist_ok=True)
archive = cache / f'vulkansdk-macos-{version}.zip'
if not archive.exists():
    url = f'https://sdk.lunarg.com/sdk/download/{version}/mac/{archive.name}'
    print('Descargando Vulkan SDK ' + version, flush=True)
    temporary = archive.with_suffix('.downloading')
    try:
        urllib.request.urlretrieve(url, temporary)
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)
with archive.open('rb') as source:
    digest = hashlib.file_digest(source, 'sha256').hexdigest()
if digest != expected:
    raise SystemExit('El SHA-256 del SDK no coincide; no se ejecutó el instalador: ' + str(archive))
tools = ROOT / '.tools'
tools.mkdir(exist_ok=True)
subprocess.run(['/usr/bin/ditto', '-x', '-k', str(archive), str(tools / 'installer')], check=True)
installer = tools / f'installer/vulkansdk-macOS-{version}.app/Contents/MacOS/vulkansdk-macOS-{version}'
subprocess.run([str(installer), '--root', str(tools / 'vulkan-sdk'), '--accept-licenses',
                '--default-answer', '--confirm-command', 'install', 'copy_only=1'], check=True)
if not (tools / 'vulkan-sdk/macOS/bin/dxc').is_file():
    raise SystemExit('La instalación no produjo DXC en la ruta esperada')
