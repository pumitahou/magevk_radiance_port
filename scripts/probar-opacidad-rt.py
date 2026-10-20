#!/usr/bin/env python3
"""Check triangle any-hit behavior for geometry and instance opacity flags."""
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
parser.add_argument('--width', type=int, default=64)
parser.add_argument('--height', type=int, default=33)
parser.add_argument('--repeats', type=int, default=3)
parser.add_argument('--validation', action='store_true')
args = parser.parse_args()
if not (0 < args.width and 0 < args.height and args.width * args.height <= 1048576
        and 1 <= args.repeats <= 20):
    parser.error('Dimensiones o repeticiones fuera de límites')

package = root / 'dist/Radiance-M4-experimental'
env = os.environ.copy()
env.setdefault('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')
exe = Path('/tmp/radiance-rt-opacity-probe')
subprocess.run(['/usr/bin/clang++', '-O2', '-std=c++17', str(root/'probes/rt_opacity.cpp'),
    '-I'+str(sdk()/'include'), '-L'+str(package/'runtime'), '-lvulkan.1',
    '-Wl,-rpath,'+str(package/'runtime'), '-o', str(exe)], env=env, check=True)
for shader in ('rt_perf_any.rgen', 'rt_perf_any.rmiss', 'rt_perf_any.rchit', 'rt_perf_any.rahit'):
    source = root/'probes'/shader
    subprocess.run([tool('glslangValidator'), '-V', '--target-env', 'vulkan1.2',
        str(source), '-o', str(source)+'.spv'], check=True, stdout=subprocess.DEVNULL)

state = root/'experiments/opacity-rt/repeticiones'/time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True, exist_ok=False)
env.update(VK_DRIVER_FILES=str(package/'runtime/magevk_icd.json'),
    DYLD_LIBRARY_PATH=str(package/'runtime'), MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING='1')
for key in ('VK_INSTANCE_LAYERS', 'VK_ICD_FILENAMES', 'VK_LAYER_PATH',
            'VK_LAYER_SETTINGS_PATH', 'VK_LOADER_DEBUG'):
    env.pop(key, None)
if args.validation:
    env.update(VK_LAYER_PATH=str(sdk()/'share/vulkan/explicit_layer.d'),
        VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
        VK_LAYER_SETTINGS_PATH=str(package/'runtime/validation'))

rows = []
for mode in ('opaque', 'nonopaque', 'force-opaque', 'force-no-opaque'):
    env['RADIANCE_MAGEVK_RT_GROUP_SIZE'] = '1'
    name = mode+'.log'
    with (state/name).open('w') as output:
        run = subprocess.run([str(exe), str(args.width), str(args.height), '1',
            str(args.repeats), mode], cwd=root, env=env, stdout=output,
            stderr=subprocess.STDOUT, timeout=60)
    log = (state/name).read_text(errors='replace')
    result = re.search(r'^RESULT (.*)$', log, re.M)
    if (run.returncode or result is None or 'RT direct group=1 ' not in log
            or 'Validation Error:' in log or 'Validation Warning:' in log):
        raise SystemExit('Fallo de prueba o validación: '+str(state/name))
    row = json.loads(result[1])
    if not row['all_pixels_correct']:
        raise SystemExit('Resultado GPU incorrecto: '+str(state/name))
    row.update(log=name, validation=args.validation)
    rows.append(row)
    (state/'resultados.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(f'{mode}: resultado correcto, GPU {row["gpu_ms_median"]:.3f} ms', flush=True)
print('Resultados guardados en '+str(state))
