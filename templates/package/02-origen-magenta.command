#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'source-magenta\n' > "$RADIANCE_PACKAGE_DIR/experiment-mode.txt"
printf 'E06: origen magenta seleccionado.\nCierra Minecraft y arranca el mismo perfil Fabric. En esta prueba se espera una pantalla magenta; los menús estarán ocultos a propósito.\nDespués vuelve al modo normal con 01-interfaz-normal.command.\n'
