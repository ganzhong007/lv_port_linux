#!/usr/bin/env bash
# 场景二验收：静态线框楼群 + AR 透明底 + 中心绿色线框像素
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
lvgl_verify_run 2
echo "verify_scenario2: OK (backend=${LVGL_BACKEND:-glfw})"
