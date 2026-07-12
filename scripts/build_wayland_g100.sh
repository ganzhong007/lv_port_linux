#!/usr/bin/env bash
# Build and run lvglsim with DrawUnitG100 on WSLg Wayland-EGL.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build-g100"
CONFIG="${CONFIG:-wayland-g100}"
DEMO="${DEMO:-simple_button}"
W="${W:-800}"
H="${H:-480}"

cmake -B "${BUILD}" -DCONFIG="${CONFIG}" -DLVGL_APP_DEMO="${DEMO}"
cmake --build "${BUILD}" -j"$(nproc)"

echo "==> Running DrawUnitG100 (${CONFIG}, demo=${DEMO}) ${W}x${H}"
exec "${BUILD}/bin/lvglsim" -b wayland -W "${W}" -H "${H}" "$@"
