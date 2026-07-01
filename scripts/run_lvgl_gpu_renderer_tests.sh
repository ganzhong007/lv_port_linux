#!/usr/bin/env bash
# Run LVGL upstream unit tests for gpu_renderer (OPTIONS_TEST_GPU_RENDERER).
# Prerequisites and full notes: docs/LVGL_2D3D_BACKEND_PLAN.md §14
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LVGL="$ROOT/lvgl"
if [[ ! -d "$LVGL/tests" ]]; then
  echo "run_lvgl_gpu_renderer_tests: missing lvgl submodule at $LVGL" >&2
  exit 1
fi
cd "$LVGL"
if ! command -v ruby >/dev/null 2>&1; then
  echo "run_lvgl_gpu_renderer_tests: install ruby — see docs/LVGL_2D3D_BACKEND_PLAN.md §14" >&2
  exit 1
fi
RUN=(./tests/main.py --build-options OPTIONS_TEST_GPU_RENDERER test)
if command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a "${RUN[@]}"
else
  "${RUN[@]}"
fi
