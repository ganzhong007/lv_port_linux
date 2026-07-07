#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
export LVGL_SCENARIO=8
export LVGL_VERIFY_GPU_PATH=1
export LVGL_GPU_GLYPH_ATLAS_SIZE="${LVGL_GPU_GLYPH_ATLAS_SIZE:-128}"
export LVGL_VERIFY_FRAMES="${LVGL_VERIFY_FRAMES:-30}"
export LVGL_VERIFY_WARMUP="${LVGL_VERIFY_WARMUP:-8}"
export LV_SIM_WINDOW_WIDTH="${LV_SIM_WINDOW_WIDTH:-1280}"
export LV_SIM_WINDOW_HEIGHT="${LV_SIM_WINDOW_HEIGHT:-720}"
lvgl_verify_run 8
echo "verify_scenario8: OK (backend=${LVGL_BACKEND:-glfw})"
