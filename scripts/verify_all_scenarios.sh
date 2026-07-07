#!/usr/bin/env bash
# Run LVGL_VERIFY for scenarios 1–8 (glfw + xvfb).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LVGL_BUILD_DIR="${LVGL_BUILD_DIR:-$ROOT/build-vgl-glfw}"
export LVGL_BACKEND="${LVGL_BACKEND:-glfw}"
for s in 1 2 3 4 5 6 7 8; do
  echo "=== verify scenario $s ==="
  "$ROOT/scripts/verify_scenario${s}.sh"
done
echo "verify_all_scenarios: OK (backend=${LVGL_BACKEND})"
