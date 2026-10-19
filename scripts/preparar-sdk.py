#!/usr/bin/env python3
"""Reuse a Vulkan SDK or install the ZIP supplied manually in downloads/."""
from pathlib import Path
import os
import subprocess
import tempfile
from port_paths import ROOT

choices = [Path(os.environ['VULKAN_SDK'])] if os.environ.get('VULKAN_SDK') else []
choices += [ROOT / '.tools/vulkan-sdk/macOS', ROOT / 'vulkan-sdk/macOS']
for path in choices:
    if (path / 'bin/dxc').is_file():
        print('Vulkan SDK existente: ' + str(path))
        raise SystemExit(0)
downloads = ROOT / 'downloads'
downloads.mkdir(exist_ok=True)
archives = sorted(p for p in downloads.glob('vulkansdk-macos-*.zip') if p.is_file())
if not archives:
    raise SystemExit(
        'Falta el instalador de Vulkan SDK para macOS.\n'
        '1. Descarga el ZIP desde https://vulkan.lunarg.com/sdk/home/\n'
        '2. Colócalo sin descomprimir en: ' + str(downloads) + '\n'
        '   Nombre esperado: vulkansdk-macos-*.zip\n'
        '3. Vuelve a ejecutar ./setup.sh.'
    )
if len(archives) != 1:
    raise SystemExit('Hay varios ZIP de Vulkan SDK. Deja solo el que quieras instalar en: ' + str(downloads))
archive = archives[0]
tools = ROOT / '.tools'
tools.mkdir(exist_ok=True)
print('Instalando Vulkan SDK desde ' + archive.name, flush=True)
with tempfile.TemporaryDirectory(prefix='vulkan-installer-', dir=tools) as extracted:
    subprocess.run(['/usr/bin/ditto', '-x', '-k', str(archive), extracted], check=True)
    applications = list(Path(extracted).glob('vulkansdk-macOS-*.app'))
    if len(applications) != 1:
        raise SystemExit('El ZIP no contiene un único instalador Vulkan SDK para macOS.')
    installer = applications[0] / 'Contents/MacOS' / applications[0].stem
    if not installer.is_file():
        raise SystemExit('No se encontró el ejecutable del instalador dentro del ZIP.')
    subprocess.run([str(installer), '--root', str(tools / 'vulkan-sdk'), '--accept-licenses',
                    '--default-answer', '--confirm-command', 'install', 'copy_only=1'], check=True)
if not (tools / 'vulkan-sdk/macOS/bin/dxc').is_file():
    raise SystemExit('La instalación no produjo DXC en la ruta esperada')
