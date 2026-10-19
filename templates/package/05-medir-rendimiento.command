#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'off\n' > "$RADIANCE_PACKAGE_DIR/validation-mode.txt"
printf '1\n' > "$RADIANCE_PACKAGE_DIR/performance-mode.txt"
printf 'Medición seleccionada, validación apagada.\nReinicia el mismo perfil Fabric y prueba el mismo mundo y resolución.\n'
