#!/bin/bash
set -euo pipefail
RADIANCE_SETUP_ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$RADIANCE_SETUP_ROOT/scripts/entorno.sh"
case "${1:-}" in
  '') RADIANCE_CHECK_ONLY=0 ;;
  --check) RADIANCE_CHECK_ONLY=1 ;;
  *) echo 'Uso: ./setup.sh [--check]'; exit 2 ;;
esac
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || { echo 'Esta versión requiere macOS Apple Silicon y una terminal arm64.'; exit 1; }
[[ -x "$DEVELOPER_DIR/usr/bin/xcodebuild" ]] || { echo 'Instala Xcode completo y define DEVELOPER_DIR si está en otra ruta.'; exit 1; }
xcrun --find clang++ >/dev/null
if ! command -v brew >/dev/null 2>&1; then
  echo 'Instala Homebrew desde https://brew.sh y vuelve a ejecutar ./setup.sh'; exit 1
fi
RADIANCE_FORMULAE=(cmake ninja openjdk@21 shaderc glslang vulkan-loader vulkan-headers spirv-tools python@3.13)
for formula in "${RADIANCE_FORMULAE[@]}"; do
  if ! brew list --versions "$formula" >/dev/null 2>&1; then
    if [[ "$RADIANCE_CHECK_ONLY" == 1 ]]; then echo "Falta Homebrew: $formula"; exit 1; fi
    brew install "$formula"
  fi
done
source "$RADIANCE_SETUP_ROOT/scripts/entorno.sh"
java -version 2>&1 | head -1 | grep -E 'version "21[.\"]' >/dev/null || { echo 'JAVA_HOME debe señalar un JDK 21.'; exit 1; }
python3 -c 'import sys; assert sys.version_info >= (3, 11), "Se requiere Python 3.11 o posterior"'
if [[ "$RADIANCE_CHECK_ONLY" == 0 ]]; then
  git -C "$RADIANCE_ROOT" submodule sync --recursive
  git -C "$RADIANCE_ROOT" submodule update --init --recursive --jobs 4
  python3 "$RADIANCE_ROOT/scripts/preparar-sdk.py"
  source "$RADIANCE_ROOT/scripts/entorno.sh"
fi
[[ -n "${VULKAN_SDK:-}" && -x "$VULKAN_SDK/bin/dxc" ]] || { echo 'Falta Vulkan SDK; ejecuta ./setup.sh o define VULKAN_SDK.'; exit 1; }
for tool in cmake ninja glslangValidator; do command -v "$tool" >/dev/null; done
python3 "$RADIANCE_ROOT/scripts/verificar-fuentes.py"
printf 'Entorno listo. SDK: %s\nCompila con ./compile.sh\n' "$VULKAN_SDK"
