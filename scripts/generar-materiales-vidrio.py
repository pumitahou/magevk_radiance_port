#!/usr/bin/env python3
"""Generate LabPBR numeric specular maps for vanilla glass, without changing albedo."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1]
assets = root / 'Radiance/src/main/resources/assets/minecraft/textures/block'
colors = ('white orange magenta light_blue yellow lime pink gray light_gray cyan '
          'purple blue brown green red black').split()
names = ['glass', 'glass_pane_top']
for color in colors:
    names += [color + '_stained_glass', color + '_stained_glass_pane_top']

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

# LabPBR: smoothness 1, dielectric F0=10/255, no SSS and no emission.
# These are material parameters; the existing albedo supplies the alpha.
row = bytes([0]) + bytes([255, 10, 0, 255]) * 16
png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 6, 0, 0, 0)) +
       chunk(b'IDAT', zlib.compress(row * 16)) + chunk(b'IEND', b''))
for name in names:
    target = assets / (name + '_s.png')
    if target.exists() and target.read_bytes() != png:
        raise SystemExit('Material existente diferente: ' + str(target))
    target.write_bytes(png)
print(f'{len(names)} mapas LabPBR de vidrio generados')
