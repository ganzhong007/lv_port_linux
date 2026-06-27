#!/usr/bin/env bash
# 场景一验收：9 宫格 AR 透明底 + 中心区域彩色像素
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"
lvgl_verify_run 1
echo "verify_scenario1: OK (backend=${LVGL_BACKEND:-glfw})"
