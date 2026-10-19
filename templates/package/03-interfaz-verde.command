#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
printf 'flat-ui\n' > "$RADIANCE_PACKAGE_DIR/experiment-mode.txt"
printf 'Interfaz verde de diagnóstico seleccionada.\nCierra Minecraft y arranca el mismo perfil Fabric desde el Launcher oficial.\nSe buscan formas o superficies verdes; no se espera leer textos en esta prueba.\n'
