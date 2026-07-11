#!/usr/bin/env bash
# Run simple button demo and save layer trace to trace.log
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build-egl}"
BIN="$BUILD/bin/lvglsim"
LOG="${1:-$ROOT/trace.log}"

if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build-egl -DCONFIG=wayland-egl && cmake --build build-egl"
  exit 1
fi

echo "Starting lvglsim (Wayland-EGL / NanoVG), trace -> $LOG"
echo "Window: 800x480, title 'LVGL Simulator'"
echo "Click anywhere on the button; label changes to 'Clicked!'"
echo "Press Ctrl+C to stop."
echo

exec "$BIN" -b wayland 2>"$LOG"
