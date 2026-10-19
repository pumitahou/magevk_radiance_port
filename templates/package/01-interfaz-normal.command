#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'normal\n' > "$RADIANCE_PACKAGE_DIR/experiment-mode.txt"
printf 'Interfaz normal seleccionada.\nCierra Minecraft si sigue abierto y arranca tu perfil Fabric desde el Launcher oficial.\n'
