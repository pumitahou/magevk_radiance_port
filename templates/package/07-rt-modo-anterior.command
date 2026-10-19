#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf '1\n' > "$RADIANCE_PACKAGE_DIR/rt-threadgroup-size.txt"
printf 'Ray tracing en grupos de 1, como control anterior.\nReinicia el mismo perfil Fabric para comparar. Los JAR y la calidad no cambian.\n'
