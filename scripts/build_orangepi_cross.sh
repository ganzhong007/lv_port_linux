#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build-orangepi"

export PATH="${ROOT}/scripts:${PATH}"
export WAYLAND_SCANNER="${ROOT}/scripts/wayland-scanner-compat17.sh"
SYSROOT="/home/gz/opt/x-tools/armv8-rpi3-linux-gnueabihf/armv8-rpi3-linux-gnueabihf/sysroot"
export PKG_CONFIG_SYSROOT_DIR="${SYSROOT}"
export PKG_CONFIG_PATH="${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig"
export PKG_CONFIG_LIBDIR="${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/usr/lib/pkgconfig"
export SYSROOT="${SYSROOT}"

cmake -B "${BUILD}" -S "${ROOT}" \
  -DCONFIG=orangepi-weston \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${ROOT}/user_cross_compile_armhf.cmake" \
  -DCMAKE_PKG_CONFIG_EXECUTABLE="${ROOT}/scripts/pkg-config-armv8.sh" \
  -DWAYLAND_SCANNER="${ROOT}/scripts/wayland-scanner-compat17.sh"

cmake --build "${BUILD}" -j"$(nproc)"
echo "Built: ${BUILD}/bin/lvglsim"
