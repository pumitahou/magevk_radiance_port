#!/usr/bin/env python3
"""Compare the original material image and per-ray payload using production GLSL helpers."""
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
parser.add_argument('--width', type=int, default=127)
parser.add_argument('--height', type=int, default=65)
parser.add_argument('--repeats', type=int, default=5)
parser.add_argument('--groups', default='1,32')
parser.add_argument('--metal-validation', action='store_true')
args = parser.parse_args()
groups = [int(value) for value in args.groups.split(',')]
if not (0 < args.width and 0 < args.height and args.width * args.height <= 1048576 and
        1 <= args.repeats <= 20 and groups and all(g in (1,32) for g in groups)):
    parser.error('Dimensiones, repeticiones o grupos fuera de límites')
package = root/'dist/Radiance-M4-experimental'
experiment = root/'experiments/E14-material-payload'
state = experiment/'repeticiones'/time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
env.setdefault('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')
subprocess.run(['/usr/bin/clang++','-O2','-std=c++17',str(root/'probes/rt_material.cpp'),
    '-I'+str(sdk()/'include'),'-L'+str(package/'runtime'),'-lvulkan.1',
    '-Wl,-rpath,'+str(package/'runtime'),'-o',str(root/'probes/rt_material')],env=env,check=True)
for mode in ('image','payload','payload-glass'):
    for stage in ('rgen','rchit','rmiss'):
        includes = (['-I'+str(root/'probes/material-control')] if mode == 'image' else [])
        subprocess.run([tool('glslangValidator'),'-V','--target-env','vulkan1.2',
            *includes,'-I'+str(root/'MCVR/src/shader'),'-I'+str(root/'MCVR/src'),
            '-DMCVR_RAY_MATERIAL_PAYLOAD='+str(int(mode!='image')),
            '-DMCVR_TEST_GLASS_SPECULAR='+str(int(mode=='payload-glass')),
            str(root/('probes/rt_material.'+stage)),'-o',str(root/('probes/rt_material_'+mode+'.'+stage+'.spv'))],
            check=True,stdout=subprocess.DEVNULL)
    subprocess.run([tool('glslangValidator'),'-V','--target-env','vulkan1.2',
        '-I'+str(root/'MCVR/src/shader'),'-I'+str(root/'MCVR/src'),
        str(root/'probes/rt_material_shadow.rmiss'),'-o',
        str(root/('probes/rt_material_'+mode+'_shadow.rmiss.spv'))],check=True,stdout=subprocess.DEVNULL)
env.update(VK_DRIVER_FILES=str(package/'runtime/magevk_icd.json'),DYLD_LIBRARY_PATH=str(package/'runtime'),
    MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING='1',RADIANCE_MAGEVK_RETAIN_RESOURCES='1',
    VK_LAYER_PATH=str(sdk()/'share/vulkan/explicit_layer.d'),
    VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_SETTINGS_PATH=str(package/'runtime/validation'))
for key in ('VK_ICD_FILENAMES','VK_LOADER_DEBUG','MTL_DEBUG_LAYER'):
    env.pop(key,None)
if args.metal_validation:
    env['MTL_DEBUG_LAYER']='1'
rows=[]
for group in groups:
    for mode in ('image','payload','payload-glass'):
        env['RADIANCE_MAGEVK_RT_GROUP_SIZE']=str(group)
        log=state/(mode+'-g'+str(group)+'.log')
        with log.open('w') as output:
            proc=subprocess.run([str(root/'probes/rt_material'),str(args.width),str(args.height),
                '1',str(args.repeats),mode],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,timeout=60)
        text=log.read_text(errors='replace')
        match=re.search(r'^RESULT (.*)$',text,re.M)
        if (proc.returncode or match is None or f'RT direct group={group} ' not in text or
                'Validation Error:' in text or 'Validation Warning:' in text or '[mvk-error]' in text):
            raise SystemExit('Fallo de ejecución/validación: '+str(log))
        row=json.loads(match[1])
        if mode!='image' and not row['all_pixels_correct']:
            raise SystemExit('El material del payload no coincide: '+str(log))
        row.update(mode=mode,group=group,log=log.name,metal_validation=args.metal_validation)
        rows.append(row)
        (state/'resultados.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(f'{mode}, grupo {group}: GPU {row["gpu_ms_median"]:.3f} ms; muestras incorrectas {row["mismatch_samples"]}',flush=True)
print('Evidencia: '+str(state))
