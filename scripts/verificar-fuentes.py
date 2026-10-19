#!/usr/bin/env python3
"""Check the recursive gitlinks and pinned compiler dependencies without updating them."""
import json
import subprocess
from port_paths import ROOT

status = subprocess.check_output(['git', '-C', str(ROOT), 'submodule', 'status', '--recursive'], text=True)
bad = [line for line in status.splitlines() if line and line[0] in '-+U']
if bad:
    raise SystemExit('Submódulos ausentes o diferentes del commit guardado:\n' + '\n'.join(bad))
requirements = json.loads((ROOT / 'requirements.json').read_text())
for name, expected in requirements['fetched_sources'].items():
    path = ROOT / 'third_party' / name
    if not path.is_dir():
        raise SystemExit('Falta el submódulo ' + str(path))
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != expected:
        raise SystemExit('Revisión inesperada de ' + name)
print('Submódulos y herramientas de shaders fijados correctamente.')
