#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf '32\n' > "$RADIANCE_PACKAGE_DIR/rt-threadgroup-size.txt"
printf 'Ray tracing optimizado en grupos de 32.\nReinicia el mismo perfil Fabric. Se conservan resolución, rebotes y NRD.\n'
