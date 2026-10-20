#!/usr/bin/env python3
"""Compare baseline and patched NRD compose using known synthetic transmission inputs."""
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
parser.add_argument('--validation', action='store_true')
parser.add_argument('--baseline-ref', default='13cb28860f9189221dd34c98ebb3dcc557dc63b1',
    help='Revisión MCVR anterior a E16 para reproducir la omisión de refracción')
args = parser.parse_args()
package = root/'dist/Radiance-M4-experimental'
probe = Path('/tmp/radiance-nrd-transparency-probe')
baseline_source = Path('/tmp/radiance-compose-baseline.comp')
baseline_spv = Path('/tmp/radiance-compose-baseline.spv')
patched_spv = Path('/tmp/radiance-compose-patched.spv')
half_spv = Path('/tmp/radiance-compose-half-rate.spv')
subprocess.run(['/usr/bin/clang++','-O2','-std=c++17',str(root/'probes/nrd_transparency.cpp'),
    '-I'+str(sdk()/'include'),'-L'+str(package/'runtime'),'-lvulkan.1',
    '-Wl,-rpath,'+str(package/'runtime'),'-o',str(probe)],check=True)

compose = root/'MCVR/src/shader/world/nrd/compose.comp'
baseline = subprocess.run(['git','-C',str(root/'MCVR'),'show',args.baseline_ref+':src/shader/world/nrd/compose.comp'],
    check=True,capture_output=True,text=True).stdout
baseline = baseline.replace('../../../extern/nrd/Shaders/Include/NRD.hlsli','NRD.hlsli')
baseline_source.write_text(baseline)
base_cmd = [tool('glslangValidator'),'-V','--target-env','vulkan1.2','-S','comp',
    '-I'+str(root/'MCVR/src'),'-I'+str(root/'MCVR/src/shader/world/nrd'),
    '-I'+str(root/'MCVR/src/shader/util'),
    '-I'+str(root/'MCVR/extern/nrd/Shaders/Include')]
subprocess.run(base_cmd+['-DMCVR_NRD_HALF_RATE_RESOLVE=0',str(baseline_source),'-o',str(baseline_spv)],check=True)
subprocess.run(base_cmd+['-DMCVR_NRD_HALF_RATE_RESOLVE=0',str(compose),'-o',str(patched_spv)],check=True)
subprocess.run(base_cmd+['-DMCVR_NRD_HALF_RATE_RESOLVE=1',str(compose),'-o',str(half_spv)],check=True)

state = root/'experiments/nrd-transparency/repeticiones'/time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True,exist_ok=False)
env=os.environ.copy(); env.update(VK_DRIVER_FILES=str(package/'runtime/magevk_icd.json'),
    DYLD_LIBRARY_PATH=str(package/'runtime'),MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING='1')
for key in ('VK_INSTANCE_LAYERS','VK_ICD_FILENAMES','VK_LAYER_PATH','VK_LAYER_SETTINGS_PATH','VK_LOADER_DEBUG'):
    env.pop(key,None)
if args.validation:
    env.update(VK_LAYER_PATH=str(sdk()/'share/vulkan/explicit_layer.d'),
        VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_SETTINGS_PATH=str(package/'runtime/validation'))
rows=[]
for mode,spv in (('baseline',baseline_spv),('patched',patched_spv)):
    name=mode+'.log'
    with (state/name).open('w') as out:
        run=subprocess.run([str(probe),str(spv),mode],cwd=root,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=60)
    log=(state/name).read_text(errors='replace'); result=re.search(r'^RESULT (.*)$',log,re.M)
    if run.returncode or not result or 'Validation Error:' in log or 'Validation Warning:' in log:
        raise SystemExit('Falló '+mode+': '+str(state/name))
    rows.append(json.loads(result[1]))
    print(f'{mode}: {result[1]}',flush=True)
(state/'resultados.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Half-rate shader compiló; resultados guardados en '+str(state))
