#!/bin/bash
set -euo pipefail
RADIANCE_PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
python3 "$RADIANCE_PACKAGE_DIR/bin/configurar-lowcost.py" apply
