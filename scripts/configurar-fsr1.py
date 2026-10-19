#!/usr/bin/env python3
"""Select a reversible FSR1 render scale in this Launcher profile, while Minecraft is closed."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from port_paths import minecraft_base, require_game_closed

mode=sys.argv[1] if len(sys.argv)>1 else '4x'
choices={'4x':'ultra','2x':'performance','native':'native'}
if mode not in choices:
    raise SystemExit('Usa 4x, 2x o native')
parent=Path(__file__).resolve().parents[1]
package=parent if parent.name=='Radiance-M4-experimental' else parent/'dist/Radiance-M4-experimental'
base=minecraft_base()
profiles=json.loads((base/'launcher_profiles.json').read_text())['profiles']
matches=[v for v in profiles.values() if 'fabric' in v.get('lastVersionId','').lower() and v.get('javaDir')
    and Path(v['javaDir']).expanduser().resolve()==(package/'bin/java-magevk').resolve()]
dirs={Path(v.get('gameDir') or base).expanduser().resolve() for v in matches}
if len(dirs)!=1:
    raise SystemExit('No se identificó un único perfil Fabric para este paquete')
game=dirs.pop()
require_game_closed()
path=game/'radiance/pipeline.yaml'
before=path.read_text()
blocks=list(re.finditer(r'(?ms)^- attributes:\n.*?(?=^- attributes:|\Z)',before))
block=next((m for m in blocks if re.search(r'^  entryName: render_pipeline\.module\.fsr_upscaler\.name\s*$',m[0],re.M)),None)
if block is None:
    raise SystemExit('El perfil no contiene el módulo FSR esperado; no se modificó')
text=block[0]
values={'enable':'render_pipeline.false' if mode=='native' else 'render_pipeline.true',
    'quality_mode':'render_pipeline.module.fsr_upscaler.attribute.quality_mode.'+choices[mode]}
for key,value in values.items():
    pattern=r'(?m)(^  - name: render_pipeline\.module\.fsr_upscaler\.attribute\.'+key+r'\n(?:(?!  - name:)[ \t]+.*\n)*?    value: )[^\n]*'
    text,count=re.subn(pattern,lambda match:match[1]+value,text,count=1)
    if count!=1:
        raise SystemExit('No se pudo actualizar '+key+'; no se modificó el perfil')
after=before[:block.start()]+text+before[block.end():]
backup=package/'logs/fsr1-config'/(time.strftime('%Y%m%d-%H%M%S')+'-'+str(time.time_ns())[-6:])
backup.mkdir(parents=True,exist_ok=False)
shutil.copy2(path,backup/'pipeline.yaml')
temporary=path.with_name('pipeline.yaml.fsr1-temporary')
try:
    temporary.write_text(after)
    temporary.replace(path)
finally:
    temporary.unlink(missing_ok=True)
assert path.read_text()==after
record={'mode':mode,'pipeline':str(path),'backup':str(backup),
    'sha256_before':hashlib.sha256(before.encode()).hexdigest(),
    'sha256_after':hashlib.sha256(after.encode()).hexdigest()}
(backup/'configuracion.json').write_text(json.dumps(record,indent=2)+'\n')
state=parent/'experiments/E15-fsr1'
if state.is_dir():
    (state/'configuracion.json').write_text(json.dumps(record,indent=2)+'\n')
print('Modo preparado: FSR 1 '+mode+'. Inicia el mismo perfil Fabric. Respaldo: '+str(backup))
