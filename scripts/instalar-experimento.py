#!/usr/bin/env python3
"""Copy the prepared experimental JARs to this user's official Launcher profile."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile
from port_paths import minecraft_base, require_game_closed

root = Path(__file__).resolve().parents[1]
package = root / 'dist/Radiance-M4-experimental'
experiment = sys.argv[1] if len(sys.argv) > 1 else (package / 'experiment-id.txt').read_text().strip()
if not experiment.replace('-', '').isalnum():
    raise SystemExit('Identificador de experimento no válido')
state = root / '.local' / 'installations' / experiment
state.mkdir(parents=True, exist_ok=True)
base = minecraft_base()
if not (base / 'launcher_profiles.json').exists():
    raise SystemExit('Paquete listo. Instala Fabric 1.21.1 y configura el ejecutable Java del perfil con dist/Radiance-M4-experimental/bin/java-magevk.')
profiles = json.loads((base / 'launcher_profiles.json').read_text()).get('profiles', {})
wrapper = (package / 'bin/java-magevk').resolve()
matched_profiles = [row for row in profiles.values()
                    if 'fabric' in row.get('lastVersionId', '').lower()
                    and row.get('javaDir') and Path(row['javaDir']).expanduser().resolve() == wrapper]
game_dirs = {Path(row.get('gameDir') or base).expanduser().resolve() for row in matched_profiles}
if len(game_dirs) != 1:
    raise SystemExit('No se identificó un único directorio del perfil Fabric que usa java-magevk; no se ha instalado')
mods = game_dirs.pop() / 'mods'
require_game_closed()
mods.mkdir(parents=True, exist_ok=True)
backup = state / 'instalacion-anterior' / time.strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True, exist_ok=False)
mod_name = 'Radiance-0.1.6-alpha-fabric-1.21.1-macos-arm64-experimental.jar'
with zipfile.ZipFile(package / 'mods' / mod_name) as jar:
    native = jar.read('libcore.dylib')
    marker = ('[RADIANCE-' + experiment.split('-')[0] + ']').encode()
    runtime = package / 'runtime/libMoltenVK.1.dylib'
    if marker not in native and (not runtime.is_file() or marker not in runtime.read_bytes()):
        raise SystemExit('Ni el motor ni MageVK corresponden al experimento; no se ha instalado')
    if 'shaders/world/ray_tracing/vanilla-pt.zip' not in jar.namelist():
        raise SystemExit('Falta Vanilla PT; no se ha instalado')
other_radiance = [p.name for p in mods.glob('*.jar') if 'radiance' in p.name.lower() and p.name != mod_name]
if other_radiance:
    raise SystemExit('Hay otro Radiance que revisar antes de instalar: ' + ', '.join(other_radiance))
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
installed = []
for name in [mod_name, 'fabric-api-0.102.0+1.21.1.jar']:
    src = package / 'mods' / name
    dst = mods / name
    previous = sha(dst) if dst.exists() else None
    if dst.exists():
        shutil.copy2(dst, backup / name)
    tmp = mods / (name + '.installing')
    try:
        shutil.copy2(src, tmp)
        if sha(src) != sha(tmp):
            raise RuntimeError('La copia temporal no coincide: ' + name)
        os.replace(tmp, dst)
    finally:
        tmp.unlink(missing_ok=True)
    assert sha(src) == sha(dst)
    installed.append({'jar': name, 'destino': str(dst), 'sha256': sha(dst), 'sha256_anterior': previous})
record = {'experimento': experiment, 'fecha_local': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
          'modo_preparado': (package / 'experiment-mode.txt').read_text().strip(),
          'carpeta_mods': str(mods),
          'perfiles': [{'nombre': row.get('name'), 'version': row.get('lastVersionId')} for row in matched_profiles],
          'backup': str(backup), 'archivos': installed}
(state / 'instalacion.json').write_text(json.dumps(record, indent=2) + '\n')
print('Experimento instalado: ' + experiment)
for row in installed:
    print(row['jar'] + ': SHA-256 verificado ' + row['sha256'])
print('Respaldo anterior: ' + str(backup))
