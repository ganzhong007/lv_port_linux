#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
export LVGL_SCENARIO=6
export LVGL_VERIFY_GPU_PATH=1
export LVGL_VERIFY_FRAMES="${LVGL_VERIFY_FRAMES:-24}"
export LVGL_VERIFY_WARMUP="${LVGL_VERIFY_WARMUP:-6}"
lvgl_verify_run 6
echo "verify_scenario6: OK (backend=${LVGL_BACKEND:-glfw})"
