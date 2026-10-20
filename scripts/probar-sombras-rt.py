#!/usr/bin/env python3
"""Compare baseline and first-hit shadow trace flags on layered triangle cases."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

from port_paths import sdk, tool

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--width', type=int, default=640)
parser.add_argument('--rays', type=int, default=4)
parser.add_argument('--repeats', type=int, default=7)
parser.add_argument('--validation', action='store_true')
args = parser.parse_args()
if not 0 < args.width <= 1_048_576 or not 1 <= args.rays <= 64 or not 1 <= args.repeats <= 30:
    parser.error('Width, ray count, or repeat count is out of bounds')

package = root / 'dist/Radiance-M4-experimental'
state = root / 'experiments/shadow-flags/repeticiones' / time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
env.setdefault('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')
exe = Path('/tmp/radiance-rt-shadow-flags-probe')
subprocess.run(['/usr/bin/clang++', '-O2', '-std=c++17', str(root / 'probes/rt_shadow_flags.cpp'),
                '-I' + str(sdk() / 'include'), '-L' + str(package / 'runtime'), '-lvulkan.1',
                '-Wl,-rpath,' + str(package / 'runtime'), '-o', str(exe)], env=env, check=True)
for stage in ('rgen', 'rmiss', 'rchit', 'rahit'):
    source = root / 'probes' / f'rt_shadow_flags.{stage}'
    subprocess.run([tool('glslangValidator'), '-V', '--target-env', 'vulkan1.2', str(source),
                    '-o', str(source) + '.spv'], check=True, stdout=subprocess.DEVNULL)

env.update(VK_DRIVER_FILES=str(package / 'runtime/magevk_icd.json'),
           DYLD_LIBRARY_PATH=str(package / 'runtime'),
           MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING='1',
           RADIANCE_MAGEVK_RT_GROUP_SIZE='32')
for key in ('VK_ICD_FILENAMES', 'VK_LAYER_PATH', 'VK_INSTANCE_LAYERS',
            'VK_LAYER_SETTINGS_PATH', 'VK_LOADER_DEBUG', 'MTL_DEBUG_LAYER'):
    env.pop(key, None)
if args.validation:
    env.update(VK_LAYER_PATH=str(sdk() / 'share/vulkan/explicit_layer.d'),
               VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
               VK_LAYER_SETTINGS_PATH=str(package / 'runtime/validation'))

log_path = state / 'shadow-flags.log'
with log_path.open('w') as output:
    run = subprocess.run([str(exe), str(args.width), str(args.rays), str(args.repeats)],
                         cwd=root, env=env, stdout=output, stderr=subprocess.STDOUT, timeout=120)
log = log_path.read_text(errors='replace')
match = re.search(r'^RESULT (.*)$', log, re.M)
if run.returncode or match is None or 'RT direct group=32 ' not in log:
    raise SystemExit(f'Probe failed; see {log_path}')
if args.validation and ('Validation Error:' in log or 'Validation Warning:' in log):
    raise SystemExit(f'Vulkan validation reported an issue; see {log_path}')
result = json.loads(match[1])
if not result['baseline_correct'] or not result['fast_correct']:
    raise SystemExit(f'Shadow output mismatch; see {log_path}')
result.update(log=log_path.name, validation=args.validation)
(state / 'resultados.json').write_text(json.dumps(result, indent=2) + '\n')
print(f"Layered shadow outputs match. None={result['baseline_gpu_ms_median']:.3f} ms; "
      f"terminate+skip-closest={result['fast_gpu_ms_median']:.3f} ms")
print(f'Evidence: {state}')
