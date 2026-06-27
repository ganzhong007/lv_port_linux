#!/usr/bin/env bash
# Run both LVGL scenarios on Wayland backend (requires compositor / WAYLAND_DISPLAY)
set -euo pipefail
export LVGL_BACKEND=wayland
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/scripts/build_lvgl_wayland.sh"
bash "$ROOT/scripts/verify_scenario2.sh"
bash "$ROOT/scripts/verify_scenario1.sh"
echo "verify_scenario_wayland: all OK"
