#!/usr/bin/env bash
# Build and run LVGL simulator with Wayland on WSL2 (WSLg)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build}"

echo "Building Wayland config -> $BUILD"
cmake -B "$BUILD" -DCONFIG=wayland "$ROOT"
cmake --build "$BUILD" -j"$(nproc)"
echo "Run: $BUILD/bin/lvglsim -b wayland"
