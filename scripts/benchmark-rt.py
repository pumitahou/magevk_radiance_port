#!/usr/bin/env python3
"""Reproduce the E12 ray-dispatch comparison outside Minecraft."""
import argparse
import json
import os
from pathlib import Path
from port_paths import sdk, tool
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--groups', default='1,32')
parser.add_argument('--payloads', default='uint,wide,any')
parser.add_argument('--width', type=int, default=640)
parser.add_argument('--height', type=int, default=360)
parser.add_argument('--rays', type=int, default=4)
parser.add_argument('--repeats', type=int, default=7)
parser.add_argument('--rounds', type=int, default=2)
parser.add_argument('--validation', action='store_true')
parser.add_argument('--indirect', action='store_true')
args = parser.parse_args()
groups = [int(value) for value in args.groups.split(',')]
payloads = args.payloads.split(',')
if not groups or any(group not in (1,8,16,32,64,128) for group in groups):
    parser.error('Grupos permitidos: 1,8,16,32,64,128')
if not payloads or any(payload not in ('uint', 'wide', 'any') for payload in payloads):
    parser.error('Cargas permitidas: uint,wide,any')
if not 1 <= args.rounds <= 4:
    parser.error('Usa entre 1 y 4 rondas')
if not (0 < args.width and 0 < args.height and args.width * args.height <= 1048576
        and 1 <= args.rays <= 16 and 1 <= args.repeats <= 20):
    parser.error('Dimensiones, trazados o repeticiones fuera de los límites del benchmark')

package = root / 'dist/Radiance-M4-experimental'
env = os.environ.copy()
env.setdefault('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')
subprocess.run(['/usr/bin/clang++', '-O2', '-std=c++17', str(root/'probes/rt_benchmark.cpp'),
    '-I'+str(sdk()/'include'), '-L'+str(package/'runtime'), '-lvulkan.1',
    '-Wl,-rpath,'+str(package/'runtime'), '-o', str(root/'probes/rt_benchmark')], env=env, check=True)
for shader in sorted((root/'probes').glob('rt_perf*')):
    if shader.suffix in ('.rgen','.rchit','.rmiss','.rahit'):
        subprocess.run([tool('glslangValidator'),'-V','--target-env','vulkan1.2',
            str(shader),'-o',str(shader)+'.spv'],check=True,stdout=subprocess.DEVNULL)

state = root/'experiments/E12-rt-groups/repeticiones'/time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True, exist_ok=False)
env.update(VK_DRIVER_FILES=str(package/'runtime/magevk_icd.json'),
    DYLD_LIBRARY_PATH=str(package/'runtime'), MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING='1')
for key in ['VK_INSTANCE_LAYERS','VK_ICD_FILENAMES','VK_LAYER_PATH','VK_LAYER_SETTINGS_PATH','VK_LOADER_DEBUG']:
    env.pop(key,None)
if args.validation:
    env.update(VK_LAYER_PATH=str(sdk()/'share/vulkan/explicit_layer.d'),
        VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
        VK_LAYER_SETTINGS_PATH=str(package/'runtime/validation'))
rows = []
for payload in payloads:
    for round_index in range(args.rounds):
        # Reverse alternating rounds to reduce ordering bias from warm-up/heat.
        for group in groups if round_index % 2 == 0 else reversed(groups):
            env['RADIANCE_MAGEVK_RT_GROUP_SIZE'] = str(group)
            name = f'{len(rows):02d}-{payload}-g{group}.log'
            command = [str(root/'probes/rt_benchmark'), str(args.width), str(args.height),
                str(args.rays), str(args.repeats), payload] + (['indirect'] if args.indirect else [])
            with (state/name).open('w') as output:
                completed = subprocess.run(command,cwd=root,env=env,stdout=output,
                    stderr=subprocess.STDOUT,timeout=60)
            log = (state/name).read_text(errors='replace')
            result = re.search(r'^RESULT (.*)$',log,re.M)
            if (completed.returncode or result is None or f'RT direct group={group} ' not in log
                or 'Validation Error:' in log or 'Validation Warning:' in log):
                raise SystemExit('Fallo de prueba o biblioteca incorrecta: '+str(state/name))
            row = json.loads(result[1])
            if not row['all_pixels_correct']:
                raise SystemExit('Resultados de GPU incorrectos: '+str(state/name))
            row.update(group=group,round=round_index,payload=payload,log=name,library_marker_verified=True)
            rows.append(row)
            (state/'resultados.json').write_text(json.dumps(rows,indent=2)+'\n')
            print(f'{payload}: grupo={group}, mediana GPU={row["gpu_ms_median"]:.3f} ms, todos los píxeles correctos',flush=True)
print('Resultados guardados en '+str(state))
