#!/usr/bin/env python3
"""Validate real FSR1 EASU/RCAS shaders on the package's Vulkan-to-Metal runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from port_paths import sdk, tool
import re
import subprocess
import time
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',action='store_true',help='Compile the working sources instead of checking the final JAR')
parser.add_argument('--metal-validation',action='store_true')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
package=root/'dist/Radiance-M4-experimental'
state=root/'experiments/E15-fsr1/repeticiones'/time.strftime('%Y%m%d-%H%M%S')
state.mkdir(parents=True,exist_ok=False)
env=os.environ.copy()
env.setdefault('DEVELOPER_DIR','/Applications/Xcode.app/Contents/Developer')
subprocess.run(['/usr/bin/clang++','-O2','-std=c++17',str(root/'probes/fsr1_compute.cpp'),
    '-I'+str(sdk()/'include'),'-L'+str(package/'runtime'),'-lvulkan.1',
    '-Wl,-rpath,'+str(package/'runtime'),'-o',str(root/'probes/fsr1_compute')],check=True,env=env)
for name in ('fsr1_easu','fsr1_rcas'):
    target=state/(name+'_comp.spv')
    if args.source:
        subprocess.run([tool('glslangValidator'),'-V','--target-env','vulkan1.4',
            '-I'+str(root/'MCVR/extern/FidelityFX-SDK/sdk/include/FidelityFX/gpu'),
            str(root/f'MCVR/src/shader/world/upscaler/{name}.comp'),'-o',str(target)],check=True)
    else:
        with zipfile.ZipFile(package/'mods/Radiance-0.1.6-alpha-fabric-1.21.1-macos-arm64-experimental.jar') as jar:
            target.write_bytes(jar.read('shaders/world/upscaler/'+target.name))
env.update(VK_DRIVER_FILES=str(package/'runtime/magevk_icd.json'),DYLD_LIBRARY_PATH=str(package/'runtime'),
    RADIANCE_MAGEVK_RETAIN_RESOURCES='1',
    VK_LAYER_PATH=str(sdk()/'share/vulkan/explicit_layer.d'),
    VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_SETTINGS_PATH=str(package/'runtime/validation'))
for key in ('VK_ICD_FILENAMES','VK_LOADER_DEBUG','MTL_DEBUG_LAYER'):
    env.pop(key,None)
if args.metal_validation:
    env['MTL_DEBUG_LAYER']='1'
cases=[(1,1,4,4,'flat',0),*( (127,65,508,260,pattern,0.12) for pattern in ('flat','hdr','gradient','edge')),
    (320,180,960,540,'gradient',0.7),(320,180,640,360,'edge',1),
    (320,180,320,180,'hdr',0),(160,90,640,360,'gradient',0.12),(256,144,1024,576,'hdr',0.7)]
rows=[]
for index,case in enumerate(cases):
    log=state/f'{index:02d}-{case[4]}.log'
    with log.open('w') as out:
        result=subprocess.run([str(root/'probes/fsr1_compute'),*[str(v) for v in case],str(state)],
            env=env,cwd=root,stdout=out,stderr=subprocess.STDOUT,timeout=45)
    text=log.read_text(errors='replace')
    match=re.search(r'^RESULT (.*)$',text,re.M)
    if (result.returncode or not match or '[RADIANCE-E13-MAGEVK] retainedReferences=1' not in text or
            re.search(r'Validation Error|Validation Warning|\[mvk-error\]|\[mvk-warn\]',text)):
        raise SystemExit('Fallo: '+str(log)+'\n'+text[-1200:])
    if args.metal_validation and 'Metal API Validation Enabled' not in text:
        raise SystemExit('Metal validation was not loaded: '+str(log))
    row=json.loads(match[1]);row.update(log=log.name,source=args.source,metal_validation=args.metal_validation)
    rows.append(row)
    (state/'resultados.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(f'{case[4]} {case[0]}x{case[1]} -> {case[2]}x{case[3]}: {row["gpu_ms_median"]:.3f} ms; pixeles comprobados',flush=True)
metadata={'shader_origin':'source' if args.source else 'installed_package_jar',
    'shader_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in state.glob('*.spv')},
    'magevk_sha256':hashlib.sha256((package/'runtime/libMoltenVK.1.dylib').read_bytes()).hexdigest()}
(state/'metadatos.json').write_text(json.dumps(metadata,indent=2)+'\n')
print('Evidencia: '+str(state))
