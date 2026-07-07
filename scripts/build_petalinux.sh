#!/usr/bin/env bash
# Cross-build lvglsim for PetaLinux AArch64.
#
# Host x86_64 dev build (GLFW sim): ./scripts/build_glfw.sh
#
# Configs:
#   petalinux-a53-drm-gpu  — DRM/EGL + Mali gpu_renderer + scenarios 1–8 (default)
#   petalinux-a53-fbdev    — software fbdev fallback
#
# Usage:
#   ./scripts/build_petalinux.sh
#   LVGL_PLNX_CONFIG=petalinux-a53-fbdev ./scripts/build_petalinux.sh
#
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLNX_ENV="${PETALINUX_ENV:-/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux}"
CONFIG="${LVGL_PLNX_CONFIG:-petalinux-a53-drm-gpu}"
BUILD="${LVGL_PLNX_BUILD_DIR:-$ROOT/build-petalinux-${CONFIG#petalinux-a53-}}"
TC="$ROOT/cmake/toolchains/petalinux-aarch64-xilinx.cmake"
JOBS="${JOBS:-$(nproc)}"
STATIC="${LV_PETALINUX_STATIC:-0}"

if [[ ! -f "$PLNX_ENV" ]]; then
    echo "error: PetaLinux env not found: $PLNX_ENV" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "$PLNX_ENV"

echo "==> PetaLinux AArch64 build"
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
if [[ "$CONFIG" == *drm-gpu* ]]; then
    echo "On board (after Mali is up):"
    echo "  ./scripts/start_mali_gpu.sh"
    echo "  LVGL_SCENARIO=8 ./lvglsim -b drm   # GPU 2D edge-case demo (needs DejaVuSans on target for outline)"
else
    echo "On board:"
    echo "  ./lvglsim -b fbdev -W 800 -H 480"
fi
