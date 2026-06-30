#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
if [[ -z "${LVGL_BUILD_DIR:-}" && -d "$ROOT/build-vgl-glfw/bin" ]]; then
  export LVGL_BUILD_DIR="$ROOT/build-vgl-glfw"
fi
export LVGL_SCENARIO=4
export LVGL_VERIFY_GPU_PATH=0
export LVGL_VERIFY_DUMP="${LVGL_VERIFY_DUMP:-$ROOT/tmp/s4dump}"
mkdir -p "$LVGL_VERIFY_DUMP"
lvgl_verify_run 4
if [[ -f "${LVGL_VERIFY_DUMP:-}/frame_lvgl.rgba" ]]; then
  python3 "$ROOT/scripts/analyze_scenario4_frame.py" "${LVGL_VERIFY_DUMP}/frame_lvgl.rgba"
fi
echo "verify_scenario4: OK (backend=${LVGL_BACKEND:-glfw})"
