#!/usr/bin/env bash
# Cross-build lvglsim for PetaLinux AArch64 (fbdev + evdev).
#
# Prerequisite: PetaLinux 2020.2 SDK installed at /opt/petalinux/2020.2
#
# Output: build-petalinux-fbdev/bin/lvglsim
#
# Usage:
#   ./scripts/build_petalinux_fbdev.sh
#   LV_PETALINUX_STATIC=1 ./scripts/build_petalinux_fbdev.sh   # mostly static binary
#
# On target board:
#   ./lvglsim -b fbdev -W 800 -H 480
#   LVGL_SCENARIO=1 ./lvglsim -b fbdev   # 3D scenarios need GPU backend
#
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLNX_ENV="${PETALINUX_ENV:-/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux}"
BUILD="${LVGL_PLNX_BUILD_DIR:-$ROOT/build-petalinux-fbdev}"
TC="$ROOT/cmake/toolchains/petalinux-aarch64-xilinx.cmake"
CONFIG="${LVGL_PLNX_CONFIG:-petalinux-a53-fbdev}"
JOBS="${JOBS:-$(nproc)}"
STATIC="${LV_PETALINUX_STATIC:-0}"

if [[ ! -f "$PLNX_ENV" ]]; then
    echo "error: PetaLinux env not found: $PLNX_ENV" >&2
    echo "set PETALINUX_ENV to your environment-setup-aarch64-xilinx-linux path" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "$PLNX_ENV"

echo "==> PetaLinux AArch64 fbdev build"
echo "    SDK env   : $PLNX_ENV"
echo "    sysroot   : $SDKTARGETSYSROOT"
echo "    CC        : $CC"
echo "    build dir : $BUILD"
echo "    config    : $CONFIG"
echo "    static    : $STATIC"

CMAKE_ARGS=(
    -DCMAKE_TOOLCHAIN_FILE="$TC"
    -DCONFIG="$CONFIG"
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_BUILD_TYPE=Release
)

if [[ "$STATIC" == "1" ]]; then
    CMAKE_ARGS+=(-DLV_PETALINUX_STATIC=ON)
fi

cmake -B "$BUILD" "${CMAKE_ARGS[@]}" "$ROOT"
cmake --build "$BUILD" -j"$JOBS" --target lvglsim

OUT="$BUILD/bin/lvglsim"
if [[ ! -x "$OUT" ]]; then
    echo "error: $OUT not produced" >&2
    exit 1
fi

echo ""
echo "==> Built: $OUT"
file "$OUT"
readelf -d "$OUT" 2>/dev/null | grep NEEDED || true
echo ""
echo "Deploy to board and run:"
echo "  ./lvglsim -b fbdev -W 800 -H 480"
