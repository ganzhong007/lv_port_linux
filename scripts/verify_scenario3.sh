#!/usr/bin/env bash
# 场景三验收：大量线框 3D + GPU 3D draw 路径
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
export LVGL_VERIFY_GPU_PATH=1
lvgl_verify_run 3
echo "verify_scenario3: OK (backend=${LVGL_BACKEND:-glfw})"
