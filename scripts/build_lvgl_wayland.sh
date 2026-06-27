#!/usr/bin/env bash
# Build LVGL Wayland/EGL target (lvgl2d3dbackend-wayland-egl)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build-lvgl-wayland}"

echo "Building Wayland LVGL config -> $BUILD"
cmake -B "$BUILD" -DCONFIG=lvgl2d3dbackend-wayland-egl "$ROOT"
cmake --build "$BUILD" -j"$(nproc)"
echo "Done: $BUILD/bin/lvglsim"
