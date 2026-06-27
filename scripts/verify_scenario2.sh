#!/usr/bin/env bash
# 场景二验收：AR 透明底 + 滚动线框楼群 + segment_pool parallax
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LVGL_SCENARIO2_SCROLL_SPEED="${LVGL_SCENARIO2_SCROLL_SPEED:-220}"
export LVGL_VERIFY_GPU_PATH="${LVGL_VERIFY_GPU_PATH:-1}"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
lvgl_verify_run 2
echo "verify_scenario2: OK (backend=${LVGL_BACKEND:-glfw})"
