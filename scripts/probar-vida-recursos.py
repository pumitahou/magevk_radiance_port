#!/usr/bin/env python3
"""Stress BDA resource registration while valid compute dispatches are submitted."""
import argparse
import json
import os
from pathlib import Path
from port_paths import sdk, tool
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
package = root / 'dist/Radiance-M4-experimental'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime-dir', '--runtime', dest='runtime_dir', type=Path, default=package / 'runtime',
    help='Runtime directory containing libvulkan.1.dylib, libMoltenVK.1.dylib and magevk_icd.json')
parser.add_argument('--seconds', type=int, default=30)
parser.add_argument('--buffer-bytes', type=int, default=4096)
parser.add_argument('--lifetime-us', type=int, default=0,
    help='Optional pause before freeing each unreferenced BDA buffer; zero maximizes churn')
parser.add_argument('--validation', action='store_true')
parser.add_argument('--metal-validation', action='store_true')
parser.add_argument('--require-retain-marker', action='store_true',
    help='Fail unless the runtime prints the optional E17 retain-helper marker')
args = parser.parse_args()
if not 1 <= args.seconds <= 300:
    parser.error('--seconds must be between 1 and 300')
if not 256 <= args.buffer_bytes <= 1048576:
    parser.error('--buffer-bytes must be between 256 and 1048576')
if not 0 <= args.lifetime_us <= 1000000:
    parser.error('--lifetime-us must be between 0 and 1000000')

runtime = args.runtime_dir.expanduser().resolve()
if not (runtime / 'libMoltenVK.1.dylib').is_file():
    parser.error(f'Runtime is missing libMoltenVK.1.dylib: {runtime}')
loader_dir = runtime if (runtime / 'libvulkan.1.dylib').is_file() else package / 'runtime'
if not (loader_dir / 'libvulkan.1.dylib').is_file():
    parser.error(f'Vulkan loader not found in {runtime} or {package / "runtime"}')

state = root / 'experiments/resource-lifetime/repeticiones' / time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True, exist_ok=False)
probe = state / 'resource_lifetime_stress'
shader = state / 'resource_lifetime_stress.spv'
icd_path = runtime / 'magevk_icd.json'
if not icd_path.is_file():
    icd_path = state / 'magevk_icd.json'
    icd_path.write_text(json.dumps({'file_format_version': '1.0.0',
        'ICD': {'library_path': str(runtime / 'libMoltenVK.1.dylib',), 'api_version': '1.4.0'}}) + '\n')
env = os.environ.copy()
env.setdefault('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')
subprocess.run(['/usr/bin/clang++', '-O2', '-std=c++17', '-pthread',
    str(root / 'probes/resource_lifetime_stress.cpp'),
    '-I' + str(sdk() / 'include'), '-L' + str(loader_dir), '-lvulkan.1',
    '-Wl,-rpath,' + str(loader_dir), '-o', str(probe)], check=True, env=env)
subprocess.run([tool('glslangValidator'), '-V', '--target-env', 'vulkan1.2',
    str(root / 'probes/resource_lifetime_stress.comp'), '-o', str(shader)],
    check=True, stdout=subprocess.DEVNULL)

env.update(VK_DRIVER_FILES=str(icd_path),
    DYLD_LIBRARY_PATH=os.pathsep.join((str(runtime), str(loader_dir))),
    RADIANCE_MAGEVK_RETAIN_RESOURCES='1')
for key in ('VK_ICD_FILENAMES', 'VK_INSTANCE_LAYERS', 'VK_LAYER_PATH',
            'VK_LAYER_SETTINGS_PATH', 'VK_LOADER_DEBUG'):
    env.pop(key, None)
if args.validation:
    env.update(VK_LAYER_PATH=str(sdk() / 'share/vulkan/explicit_layer.d'),
        VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
        VK_LAYER_SETTINGS_PATH=str(package / 'runtime/validation'))
if args.metal_validation:
    env['MTL_DEBUG_LAYER'] = '1'
else:
    env.pop('MTL_DEBUG_LAYER', None)

log_path = state / 'stress.log'
command = [str(probe), str(shader), str(args.seconds), str(args.buffer_bytes), str(args.lifetime_us)]
with log_path.open('w') as output:
    completed = subprocess.run(command, cwd=root, env=env, stdout=output,
        stderr=subprocess.STDOUT, timeout=args.seconds + 120)
log_text = log_path.read_text(errors='replace')
result = re.search(r'^STRESS_RESULT (.*)$', log_text, re.M)
marker = '[RADIANCE-E17]' in log_text
if completed.returncode or result is None:
    raise SystemExit(f'Probe failed (exit {completed.returncode}); evidence: {log_path}')
if args.require_retain_marker and not marker:
    raise SystemExit(f'Retention marker missing; evidence: {log_path}')
if args.validation and re.search(r'Validation (?:Error|Warning):', log_text):
    raise SystemExit(f'Vulkan validation reported an issue; evidence: {log_path}')

summary = json.loads(result.group(1))
summary.update(runtime=str(runtime), validation=args.validation,
    metal_validation=args.metal_validation, retain_marker=marker)
(state / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, sort_keys=True))
print(f'Evidence: {state}')
