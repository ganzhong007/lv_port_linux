#!/usr/bin/env bash
# Pass 简化验收：Scenario 2 NAV AR（HUD→GPU OVERLAY + unified 3D/2D pass）
# 默认无窗口：通过 xvfb 离屏渲染（CI/SSH 环境可用）。要看画面请用 run_pass_simplify_visual.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LVGL_BUILD_DIR="${LVGL_BUILD_DIR:-$ROOT/build-lvgl-glfw}"
export LVGL_BACKEND="${LVGL_BACKEND:-glfw}"
export LVGL_VERIFY=1
export LVGL_VERIFY_FG=1
export LVGL_VERIFY_GPU_PATH=1
export LVGL_FG_UNIFIED_PASS=1
export LVGL_VERIFY_QUIET=1
export LVGL_VERIFY_FRAMES="${LVGL_VERIFY_FRAMES:-40}"
export LVGL_VERIFY_WARMUP="${LVGL_VERIFY_WARMUP:-6}"
export LV_SIM_WINDOW_WIDTH="${LV_SIM_WINDOW_WIDTH:-1280}"
export LV_SIM_WINDOW_HEIGHT="${LV_SIM_WINDOW_HEIGHT:-720}"

LOG="$ROOT/logs/verify_pass_simplify.log"
mkdir -p "$ROOT/logs"
: > "$LOG"

# shellcheck source=scripts/verify_lvgl_common.sh
source "$ROOT/scripts/verify_lvgl_common.sh"

echo "verify_pass_simplify: 离屏模式（xvfb，无窗口），完整日志 -> $LOG"
echo "  要看 NAV 画面: ./scripts/run_pass_simplify_visual.sh"
echo "=== scenario 2 (NAV AR) ==="

lvgl_verify_run 2 > "$LOG" 2>&1
rc=$?
if [[ "$rc" -ne 0 ]]; then
  echo "verify_pass_simplify: FAIL (exit=$rc)" >&2
  grep -E 'LVGL_VERIFY: (FAIL|PASS|gpu_path|fg PASS)' "$LOG" | tail -10 >&2 || true
  echo "  详见 $LOG" >&2
  exit "$rc"
fi

echo ""
echo "========== 验收摘要 =========="
grep -E 'LVGL_VERIFY: (start|PASS|gpu_path|fg PASS|scenario2 parallax)|GPU flush_3d' "$LOG" | grep -v '^\[User\]' || true
echo ""
steady=$(grep 'LVGL_VERIFY: path' "$LOG" | tail -1 || true)
if [[ -n "$steady" ]]; then
  echo "steady_state: $steady"
fi
echo "verify_pass_simplify: OK"
echo "完整日志: $LOG"
