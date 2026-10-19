#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'off\n' > "$RADIANCE_PACKAGE_DIR/validation-mode.txt"
printf '0\n' > "$RADIANCE_PACKAGE_DIR/performance-mode.txt"
printf 'Mediciones y validación apagadas para el próximo arranque.\nReinicia el mismo perfil Fabric para comparar los FPS sin instrumentación.\n'
