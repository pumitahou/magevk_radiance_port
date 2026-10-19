#!/usr/bin/env python3
"""Compile every built-in Vanilla PT ray stage with its configured defaults."""
import json
from pathlib import Path
from port_paths import sdk, tool
import subprocess

root=Path(__file__).resolve().parents[1]
pack=root/'MCVR/src/shader/world/ray_tracing/internal/vanilla-pt'
config=json.loads((pack/'configs.json').read_text())
state=root/'experiments/E14-material-payload'
output=state/'vanilla-spirv'
output.mkdir(parents=True,exist_ok=True)
world=next(p for p in config['passes'] if p['name']=='world')
definitions=dict(world['definitions'])
definitions.update(USE_SHARC='0',SHARC_QUERY='0',MCVR_SER_EXT='0',MCVR_SER_NV='0')
for attribute in config['attributes']:
    if 'define' not in attribute:
        continue
    value=attribute['default_value']
    kind=attribute['type']
    if kind=='bool':
        value='1' if value=='render_pipeline.true' else '0'
    elif kind.startswith('enum:'):
        value=str(kind.split(':',1)[1].split('-').index(value))
    define=attribute['define']
    if isinstance(define,str):
        definitions[define]=value
    else:
        for key,expression in define.items():
            definitions[key]=expression.replace('X',value)
checks=[]
with (state/'comprobacion-shaders.log').open('w') as log:
    for source in sorted((pack/'world').iterdir()):
        if source.suffix not in ('.rgen','.rchit','.rahit','.rmiss'):
            continue
        command=[tool('glslangValidator'),'-V','--target-env','vulkan1.2',
            '-I'+str(pack),'-I'+str(root/'MCVR/src/shader'),'-I'+str(root/'MCVR/src')]
        command+=['-D'+key+'='+str(value) for key,value in definitions.items()]
        command+=[str(source),'-o',str(output/(source.name+'.spv'))]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        log.write(result.stdout)
        if result.returncode:
            raise SystemExit(result.stdout)
        checks.append(source.name)
    log.write(f'PASS: {len(checks)} shaders Vanilla PT compilados con material en payload\n')
(state/'comprobacion-shaders.json').write_text(json.dumps({
    'shader_count':len(checks),'definitions':definitions,'shaders':checks},indent=2)+'\n')
print(f'PASS: {len(checks)} shaders Vanilla PT compilados')
