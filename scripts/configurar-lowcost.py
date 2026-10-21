#!/usr/bin/env python3
"""Apply or restore a short low-cost experiment using existing game settings only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import time
from port_paths import minecraft_base, require_game_closed

PREFIX = 'render_pipeline.module.'


def update_values(text, values, separator):
    for key, value in values.items():
        pattern = r'(?m)^' + re.escape(key + separator) + r'[^\r\n]*'
        text, count = re.subn(pattern, lambda _: key + separator + value, text)
        if count != 1:
            raise ValueError('Se esperaba un único ajuste: ' + key)
    return text


def update_pipeline(text, modules):
    for module, values in modules.items():
        blocks = list(re.finditer(r'(?ms)^- attributes:\n.*?(?=^- attributes:|\Z)', text))
        matches = [b for b in blocks if re.search(
            r'^  entryName: ' + re.escape(PREFIX + module + '.name') + r'\s*$', b[0], re.M)]
        if len(matches) != 1:
            raise ValueError('Se esperaba un único módulo: ' + module)
        block = matches[0]
        changed = block[0]
        for key, value in values.items():
            name = PREFIX + module + '.attribute.' + key
            pattern = (r'(?m)(^  - name: ' + re.escape(name) +
                       r'\n(?:(?!  - name:)[ \t]+.*\n)*?    value: )[^\n]*')
            changed, count = re.subn(pattern, lambda m: m[1] + value, changed)
            if count != 1:
                raise ValueError('No se pudo actualizar el ajuste: ' + name)
        text = text[:block.start()] + changed + text[block.end():]
    return text


def write_atomic(path, contents):
    temporary = path.with_name(path.name + '.lowcost-temporary')
    try:
        temporary.write_bytes(contents)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('apply', 'restore'))
    parser.add_argument('--check', action='store_true', help='validar sin modificar ajustes')
    args = parser.parse_args()
    parent = Path(__file__).resolve().parents[1]
    packaged = parent.name == 'Radiance-M4-experimental'
    package = parent if packaged else parent / 'dist/Radiance-M4-experimental'
    definition = parent / 'bin/muy-lowcost.json' if packaged else parent / 'experiments/E19-lowcost/preset.json'
    preset = json.loads(definition.read_text())
    base = minecraft_base()
    profiles = json.loads((base / 'launcher_profiles.json').read_text())['profiles']
    game_dirs = {Path(row.get('gameDir') or base).expanduser().resolve()
        for row in profiles.values() if 'fabric' in row.get('lastVersionId', '').lower()
        and row.get('javaDir') and Path(row['javaDir']).expanduser().resolve() ==
        (package / 'bin/java-magevk').resolve()}
    if len(game_dirs) != 1:
        raise ValueError('No se identificó un único perfil Fabric para este paquete')
    game = game_dirs.pop()
    pipeline = game / 'radiance/pipeline.yaml'
    original_pipeline = pipeline.read_text()
    pack_match = re.search(r'name: ' + re.escape(PREFIX + 'main_render.attribute.shader_pack_path') +
                           r'\n[ \t]+type: string\n[ \t]+value: ([^\n]+)', original_pipeline)
    if not pack_match or pack_match[1].strip() != 'shaders/world/ray_tracing/vanilla-pt.zip':
        raise ValueError('Este preset solo admite el shader pack Vanilla PT interno')
    pack_path = game / 'radiance/shaders/world/ray_tracing/vanilla-pt.zip'
    digest = hashlib.sha256(str(pack_path).encode()).hexdigest()
    shader_relative = 'radiance/shader-pack-settings/vanilla-pt.zip-' + digest + '.properties'
    relatives = ['radiance/pipeline.yaml', shader_relative, 'radiance/options.properties', 'options.txt']
    state_dir = game / 'radiance/preset-backups/muy-lowcost'
    active_path = state_dir / 'active.json'
    active = json.loads(active_path.read_text()) if active_path.exists() else None
    before = {name: (game / name).read_bytes() for name in relatives}
    if args.mode == 'restore':
        if not active or not active.get('active'):
            raise ValueError('No hay un preset low cost activo que restaurar')
        # Only the fixed settings paths above may be restored, never arbitrary paths from state.
        backup = state_dir / active['backup']
        if backup.parent != state_dir or not backup.is_dir():
            raise ValueError('Respaldo inválido')
        after = {name: (backup / str(i)).read_bytes() for i, name in enumerate(relatives)}
    else:
        shader_values = {PREFIX + 'main_render.attribute.' + k: v for k, v in preset['shader'].items()}
        after = {
            relatives[0]: update_pipeline(before[relatives[0]].decode(), preset['modules']).encode(),
            relatives[1]: update_values(before[relatives[1]].decode(), shader_values, '=').encode(),
            relatives[2]: update_values(before[relatives[2]].decode(), preset['radiance_options'], '=').encode(),
            relatives[3]: update_values(before[relatives[3]].decode(), preset['minecraft_options'], ':').encode()
        }
    if args.check:
        print('Preset validado: ' + args.mode + '; cuatro archivos de ajustes, sin cambios.')
        return
    require_game_closed()
    state_dir.mkdir(parents=True, exist_ok=True)
    if args.mode == 'apply' and (not active or not active.get('active')):
        backup = state_dir / (time.strftime('%Y%m%d-%H%M%S') + '-' + str(time.time_ns())[-6:])
        backup.mkdir()
        for i, name in enumerate(relatives):
            shutil.copy2(game / name, backup / str(i))
        active = {'active': True, 'backup': backup.name, 'files': relatives,
                  'preset': preset['name'], 'package_experiment': (package / 'experiment-id.txt').read_text().strip()}
    try:
        for name, contents in after.items():
            write_atomic(game / name, contents)
            if (game / name).read_bytes() != contents:
                raise ValueError('La verificación del ajuste no coincide')
        active['active'] = args.mode == 'apply'
        write_atomic(active_path, (json.dumps(active, indent=2) + '\n').encode())
    except Exception:
        for name, contents in before.items():
            write_atomic(game / name, contents)
        raise
    print('Preset muy low cost ' + ('activado' if args.mode == 'apply' else 'restaurado a tus ajustes anteriores') + '.')
    print('Respaldo: ' + str(state_dir / active['backup']))
    print('Inicia el mismo perfil Fabric; la primera carga puede recompilar los shaders por los ajustes nuevos.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(str(error))
