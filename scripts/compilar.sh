#!/bin/bash
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/entorno.sh"
cd "$RADIANCE_ROOT"
RADIANCE_INSTALL=1
RADIANCE_EXPERIMENT=E15-fsr1
for arg in "$@"; do
  case "$arg" in
    --no-install) RADIANCE_INSTALL=0 ;;
    E[0-9]*-*) RADIANCE_EXPERIMENT="$arg" ;;
    *) echo 'Uso: ./compile.sh [--no-install] [E15-fsr1]'; exit 2 ;;
  esac
done
"$RADIANCE_ROOT/setup.sh" --check
python3 scripts/generar-materiales-vidrio.py
RADIANCE_JOBS="${RADIANCE_JOBS:-4}"
RADIANCE_BUILD_DIR="${RADIANCE_BUILD_DIR:-$RADIANCE_ROOT/build}"
mkdir -p "$RADIANCE_BUILD_DIR" "$RADIANCE_ROOT/.cache/cpm"
cmake -S magevk -B "$RADIANCE_BUILD_DIR/magevk" -G Ninja \
  -DCPM_SPIRV-Cross_SOURCE="$RADIANCE_ROOT/magevk-spirv-cross" \
  -DCPM_SOURCE_CACHE="$RADIANCE_ROOT/.cache/cpm" -DMVK_EXCLUDE_CEREAL=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0
cmake --build "$RADIANCE_BUILD_DIR/magevk" --parallel "$RADIANCE_JOBS"
./Radiance/gradlew -p Radiance compileJava -PradianceUiBackend=vulkan
cmake -S MCVR -B "$RADIANCE_BUILD_DIR/mcvr" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DJAVA_PROJECT_ROOT_DIR="$RADIANCE_ROOT/Radiance" \
  -DMCVR_UI_BACKEND=VULKAN -DMCVR_ENABLE_NRD=ON -DMCVR_ENABLE_FFX_UPSCALER=OFF -DMCVR_ENABLE_FSR1_SPATIAL=ON \
  -DMCVR_ENABLE_XESS=OFF -DUSE_AMD=OFF \
  -DFETCHCONTENT_SOURCE_DIR_SHADERMAKE="$RADIANCE_ROOT/third_party/ShaderMake" \
  -DFETCHCONTENT_SOURCE_DIR_MATHLIB="$RADIANCE_ROOT/third_party/MathLib" \
  -DFETCHCONTENT_SOURCE_DIR_SSE2NEON="$RADIANCE_ROOT/third_party/sse2neon" \
  -DSHADERMAKE_DXC_VK_PATH="$VULKAN_SDK/bin/dxc"
cmake --build "$RADIANCE_BUILD_DIR/mcvr" --parallel "$RADIANCE_JOBS"
cmake --install "$RADIANCE_BUILD_DIR/mcvr"
./Radiance/gradlew -p Radiance build -PradianceUiBackend=vulkan
python3 scripts/empaquetar.py --build-dir "$RADIANCE_BUILD_DIR" --experiment "$RADIANCE_EXPERIMENT"
if [[ "$RADIANCE_INSTALL" == 1 ]]; then
  python3 scripts/instalar-experimento.py "$RADIANCE_EXPERIMENT"
else
  echo 'Paquete preparado en dist/Radiance-M4-experimental; instalación omitida por --no-install.'
fi
