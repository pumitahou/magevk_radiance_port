#!/usr/bin/env python3
"""Create a complete relocatable package from compiled sources, never from an old dist."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import time
import urllib.request
import zipfile
from pathlib import Path
from port_paths import ROOT, sdk

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
parser.add_argument('--experiment', default='E21-presets-nativos')
args = parser.parse_args()
if not re.fullmatch(r'E\d+-[a-zA-Z0-9-]+', args.experiment):
    parser.error('Identificador de experimento no válido')
build = args.build_dir.resolve()
dist = ROOT / 'dist'
dist.mkdir(exist_ok=True)
staging = dist / '.package-staging'
if staging.exists():
    shutil.rmtree(staging)
shutil.copytree(ROOT / 'templates/package', staging)
for name in ('mods', 'licenses', 'runtime', 'logs/runs'):
    (staging / name).mkdir(parents=True, exist_ok=True)
mod_name = 'Radiance-0.1.6-alpha-fabric-1.21.1-macos-arm64-experimental.jar'
jar_source = ROOT / 'Radiance/build/libs/Radiance-0.1.6-alpha-fabric-1.21.1-macos.jar'
with zipfile.ZipFile(jar_source) as jar:
    for entry in ('libcore.dylib', 'shaders/world/ray_tracing/vanilla-pt.zip',
                  'shaders/world/upscaler/fsr1_easu_comp.spv', 'shaders/world/upscaler/fsr1_rcas_comp.spv'):
        if entry not in jar.namelist():
            raise SystemExit('Falta recurso del mod: ' + entry)
shutil.copy2(jar_source, staging / 'mods' / mod_name)
native = sorted(p for p in (build / 'magevk/MoltenVK').glob('libMoltenVK*.dylib') if not p.is_symlink())
if len(native) != 1:
    raise SystemExit('No se identificó una única biblioteca de MageVK: ' + str(native))
shutil.copy2(native[0], staging / 'runtime/libMoltenVK.1.dylib')
loader = sdk() / 'lib/libvulkan.1.dylib'
if not loader.is_file():
    raise SystemExit('No se encontró Vulkan Loader en ' + str(loader))
shutil.copy2(loader.resolve(), staging / 'runtime/libvulkan.1.dylib')
(staging / 'runtime/magevk_icd.json').write_text(json.dumps({
    'file_format_version': '1.0.0', 'ICD': {'library_path': './libMoltenVK.1.dylib', 'api_version': '1.4.0'}
}, indent=2) + '\n')
fabric_name = 'fabric-api-0.102.0+1.21.1.jar'
fabric = ROOT / '.cache/downloads' / fabric_name
fabric.parent.mkdir(parents=True, exist_ok=True)
if not fabric.exists():
    url = 'https://maven.fabricmc.net/net/fabricmc/fabric-api/fabric-api/0.102.0+1.21.1/' + fabric_name
    temporary = fabric.with_suffix('.downloading')
    try:
        urllib.request.urlretrieve(url, temporary)
        temporary.replace(fabric)
    finally:
        temporary.unlink(missing_ok=True)
if hashlib.sha256(fabric.read_bytes()).hexdigest() != 'fd021e3ea8d043805207f39d43ac4b358629b30f0d9280ffc5a886a433291bd1':
    raise SystemExit('SHA-256 inesperado de Fabric API; no se empaquetó')
shutil.copy2(fabric, staging / 'mods' / fabric_name)
for name in ('configurar-fsr1.py', 'configurar-lowcost.py', 'port_paths.py'):
    shutil.copy2(ROOT / 'scripts' / name, staging / 'bin' / name)
shutil.copy2(ROOT / 'experiments/E19-lowcost/preset.json', staging / 'bin/muy-lowcost.json')
(staging / 'experiment-id.txt').write_text(args.experiment + '\n')
shutil.copy2(ROOT / 'templates/PAQUETE.md', staging / 'LEEME.md')
shutil.copy2(ROOT / 'THIRD_PARTY.md', staging / 'licenses/FUENTES.md')
license_dirs = {
    'Radiance': ROOT / 'Radiance', 'MCVR': ROOT / 'MCVR', 'MageVK': ROOT / 'magevk',
    'SPIRV-Cross': ROOT / 'magevk-spirv-cross',
    **{p.name: p for p in (ROOT / 'MCVR/extern').iterdir() if p.is_dir()},
    **{p.name: p for p in (ROOT / 'third_party').iterdir() if p.is_dir()},
}
for label, folder in license_dirs.items():
    for source in folder.iterdir():
        if source.is_file() and any(word in source.name.lower() for word in ('license', 'licence', 'notice', 'thirdpartylicenses')):
            shutil.copy2(source, staging / 'licenses' / (label + '-' + source.name))
for label, source in [('VulkanSDK', sdk().parent / 'Licenses'),
                      ('SPIRV-Cross-extra', ROOT / 'magevk-spirv-cross/LICENSES'),
                      ('FidelityFX', ROOT / 'MCVR/extern/FidelityFX-SDK/sdk/LICENSE.txt')]:
    if source.is_dir():
        shutil.copytree(source, staging / 'licenses' / label, dirs_exist_ok=True)
    elif source.is_file():
        shutil.copy2(source, staging / 'licenses' / (label + '-LICENSE.txt'))
revisions = {}
for path in ('Radiance', 'MCVR', 'MCVR/extern/nrd', 'magevk', 'magevk-spirv-cross',
             'third_party/ShaderMake', 'third_party/MathLib', 'third_party/sse2neon'):
    revisions[path] = subprocess.check_output(['git', '-C', str(ROOT / path), 'rev-parse', 'HEAD'], text=True).strip()
integration = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--verify', 'HEAD'], capture_output=True, text=True)
(staging / 'revisiones.json').write_text(json.dumps({
    'version': (ROOT / 'VERSION').read_text().strip(), 'experiment': args.experiment, 'sources': revisions,
    'integration_commit': integration.stdout.strip() if integration.returncode == 0 else None,
    'fsr1': True, 'frame_generation': False,
}, indent=2) + '\n')
checksums = []
for file in sorted(staging.rglob('*')):
    if file.is_file():
        checksums.append(hashlib.sha256(file.read_bytes()).hexdigest() + '  ' + str(file.relative_to(staging)))
(staging / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')
package = dist / 'Radiance-M4-experimental'
if package.exists():
    backup = ROOT / '.local/package-backups' / (time.strftime('%Y%m%d-%H%M%S') + '-' + str(time.time_ns())[-6:])
    backup.parent.mkdir(parents=True, exist_ok=True)
    package.rename(backup)
staging.rename(package)
archive = dist / 'Radiance-M4-experimental.zip'
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as out:
    for file in sorted(package.rglob('*')):
        if file.is_file() and 'logs' not in file.relative_to(package).parts:
            info = zipfile.ZipInfo.from_file(file, str(file.relative_to(dist)))
            info.compress_type = zipfile.ZIP_DEFLATED
            out.writestr(info, file.read_bytes())
print('Paquete completo: ' + str(package))
print('ZIP: ' + str(archive))
