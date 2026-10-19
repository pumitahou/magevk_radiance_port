#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'off\n' > "$RADIANCE_PACKAGE_DIR/validation-mode.txt"
printf 'Validación desactivada para el próximo arranque. El modo gráfico sigue como estaba.\n'
