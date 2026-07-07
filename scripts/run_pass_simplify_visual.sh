#!/usr/bin/env bash
# 交互查看 Scenario 2 NAV AR（有 GLFW 窗口，不做自动验收）
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LVGL_BUILD_DIR="${LVGL_BUILD_DIR:-$ROOT/build-vgl-glfw}"
BIN="$LVGL_BUILD_DIR/bin/lvglsim"

if [[ ! -x "$BIN" ]]; then
  echo "run_pass_simplify_visual: building $BIN ..."
  "$ROOT/scripts/build_glfw.sh"
fi

export LVGL_SCENARIO=2
export LVGL_FG_UNIFIED_PASS=1
export LV_SIM_WINDOW_WIDTH="${LV_SIM_WINDOW_WIDTH:-1280}"
export LV_SIM_WINDOW_HEIGHT="${LV_SIM_WINDOW_HEIGHT:-720}"

echo "run_pass_simplify_visual: 打开 GLFW 窗口（Scenario 2 NAV AR）"
echo "  unified_pass=1  HUD=GPU OVERLAY  关闭窗口即退出"
exec "$BIN" -b glfw -W "$LV_SIM_WINDOW_WIDTH" -H "$LV_SIM_WINDOW_HEIGHT"
