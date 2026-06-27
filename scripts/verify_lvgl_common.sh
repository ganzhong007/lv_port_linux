#!/usr/bin/env bash
# Shared helpers for LVGL scenario verification.
set -euo pipefail

_LVGL_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

lvgl_verify_run() {
  local scenario="$1"
  local backend="${LVGL_BACKEND:-glfw}"

  local root="${LVGL_ROOT:-$(cd "$_LVGL_SCRIPT_DIR/.." && pwd)}"

  local build config backend_args
  case "$backend" in
    glfw)
      build="${LVGL_BUILD_DIR:-$root/build-lvgl-glfw}"
      config="lvgl2d3dbackend-glfw"
      backend_args=(-b glfw)
      ;;
    wayland)
      build="${LVGL_BUILD_DIR:-$root/build-lvgl-wayland}"
      config="lvgl2d3dbackend-wayland-egl"
      backend_args=(-b wayland)
      ;;
    *)
      echo "verify: unknown LVGL_BACKEND=$backend (use glfw or wayland)" >&2
      return 2
      ;;
  esac

  local bin="$build/bin/lvglsim"
  if [[ ! -x "$bin" ]]; then
    echo "verify: configuring $config -> $build"
    cmake -B "$build" -DCONFIG="$config" "$root"
    cmake --build "$build" -j"$(nproc)"
  fi

  export LVGL_SCENARIO="$scenario"
  export LVGL_VERIFY=1
  export LVGL_VERIFY_FRAMES="${LVGL_VERIFY_FRAMES:-60}"
  export LVGL_VERIFY_WARMUP="${LVGL_VERIFY_WARMUP:-8}"
  export LV_SIM_WINDOW_WIDTH="${LV_SIM_WINDOW_WIDTH:-1920}"
  export LV_SIM_WINDOW_HEIGHT="${LV_SIM_WINDOW_HEIGHT:-1080}"

  local w="${LV_SIM_WINDOW_WIDTH}"
  local h="${LV_SIM_WINDOW_HEIGHT}"
  local run_cmd=(timeout 45 "$bin" "${backend_args[@]}" -W "$w" -H "$h")

  echo "verify: backend=$backend scenario=$scenario ${w}x${h} frames=${LVGL_VERIFY_FRAMES} bin=$bin"

  if [[ "$backend" == wayland ]]; then
    if [[ -z "${WAYLAND_DISPLAY:-}" ]] && [[ -z "${XDG_RUNTIME_DIR:-}" ]]; then
      echo "verify: WARN no WAYLAND_DISPLAY — wayland run may fail on headless hosts" >&2
    fi
    "${run_cmd[@]}"
  elif command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run -a "${run_cmd[@]}"
  else
    "${run_cmd[@]}"
  fi
}
