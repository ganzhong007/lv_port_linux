#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-a53weston"
SDK_ENV="/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux"

if [[ ! -f "${SDK_ENV}" ]]; then
  echo "Petalinux SDK not found at ${SDK_ENV}" >&2
  exit 1
fi

set +u
# shellcheck disable=SC1090
source "${SDK_ENV}"
set -u

export PATH="${ROOT}/scripts:${PATH}"
export WAYLAND_SCANNER="${ROOT}/scripts/wayland-scanner-compat17.sh"

cmake -B "${BUILD_DIR}" -S "${ROOT}" \
  -DCONFIG=a53weston \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${ROOT}/user_cross_compile_a53.cmake" \
  -DWAYLAND_SCANNER="${ROOT}/scripts/wayland-scanner-compat17.sh"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Built: ${BUILD_DIR}/bin/lvglsim"
file "${BUILD_DIR}/bin/lvglsim"
