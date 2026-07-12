#!/usr/bin/env bash
# Verify DrawUnitG100 / G8 checkpoint (CP-XX).
# Usage:
#   ./scripts/verify_g100.sh CP-00
#   ./scripts/verify_g100.sh CP-01b --build-only
#   RUN_SEC=45 ./scripts/verify_g100.sh CP-00
#
# See docs/g100_test_results.md and drawunit_g100_design.md §8.8.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${1:-}"
BUILD_ONLY=false
if [[ "${2:-}" == "--build-only" ]]; then
  BUILD_ONLY=true
fi

CONFIG="${CONFIG:-wayland-g100}"
W="${W:-800}"
H="${H:-480}"
RUN_SEC="${RUN_SEC:-0}"
BUILD="${LVGL_BUILD_DIR:-${ROOT}/build-g100-stress}"
BIN="${BUILD}/bin/lvglsim"

usage() {
  cat <<'EOF'
Usage: ./scripts/verify_g100.sh CP-XX [--build-only]

Checkpoints:
  CP-00     G0 Bootstrap: build + stress smoke (DrawUnitG100 ready)
  CP-01a    G1 shader skeleton: build + simple_button
  CP-01b    G1 gradient: build + render demo (GR-12)
  CP-01c    G1 fill/image: build + simple_button + stress short
  CP-02     G2 label: build + stress (dynamic label)
  CP-03a    G3 vector solid/grad/dash
  CP-03b    G3 vector PATTERN
  CP-04a    G4 blur kawase
  CP-04b    G4 FBO pool
  CP-05     G5 complete 2D + SW fallback check
  CP-06     G6 3D BLIT + gltf demo
  CP-07a    G7 benchmark / soak entry
  CP-07b    G7 optional libs/g100 (no regression)
  CP-08     G8.0 3D_VIEWPORT + CLEAR
  CP-09     G8.1 3D_LINE + CALLBACK
  CP-10     G8.2 3D_MESH
  CP-11     G8.3 3D_SCENE + gltf migration
  CP-12     G8.4 phong / lights
  CP-13     G8.5 pick + OBJ loader
  CP-14     G8.6 3D theme/style

Env:
  CONFIG=wayland-g100   LVGL_BUILD_DIR=...   W/H=800/480
  RUN_SEC=45            Run demo N seconds then exit (0=manual)
  STRESS_MULT=1         LV_DEMO_STRESS_DRAW_MULT for stress builds

After PASS: update docs/g100_test_results.md, commit lvgl + main, push, tag if milestone.
EOF
}

if [[ -z "${CP}" ]] || [[ "${CP}" == "-h" ]] || [[ "${CP}" == "--help" ]]; then
  usage
  exit 0
fi

log() { echo "[verify_g100] $*"; }

need_wayland() {
  if [[ -z "${WAYLAND_DISPLAY:-}" ]] && [[ ! -S "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/wayland-0" ]]; then
    log "WARN: Wayland socket not found; run under WSLg or set WAYLAND_DISPLAY"
  fi
}

cmake_build() {
  local demo="${1:-stress}"
  local extra=()
  if [[ "${demo}" == "stress" ]]; then
    extra+=(-DLV_DEMO_STRESS_DRAW_MULT="${STRESS_MULT:-1}")
  fi
  log "cmake -B ${BUILD} -DCONFIG=${CONFIG} -DLVGL_APP_DEMO=${demo} ${extra[*]}"
  cmake -B "${BUILD}" -DCONFIG="${CONFIG}" -DLVGL_APP_DEMO="${demo}" "${extra[@]}"
  cmake --build "${BUILD}" -j"$(nproc)"
}

run_demo() {
  local demo_name="${1:-demo}"
  need_wayland
  if [[ ! -x "${BIN}" ]]; then
    log "ERROR: missing ${BIN}; build first"
    exit 1
  fi
  log "Run ${demo_name} ${W}x${H} (RUN_SEC=${RUN_SEC})"
  if [[ "${RUN_SEC}" -gt 0 ]]; then
    timeout "${RUN_SEC}" "${BIN}" -b wayland -W "${W}" -H "${H}" 2>&1 | tee "/tmp/verify_g100_${CP}.log" || {
      local ec=$?
      if [[ ${ec} -eq 124 ]]; then
        log "Run finished after ${RUN_SEC}s (timeout expected)"
        return 0
      fi
      return "${ec}"
    }
  else
    log "Starting interactively; Ctrl+C to stop. Log: /tmp/verify_g100_${CP}.log"
    "${BIN}" -b wayland -W "${W}" -H "${H}" 2>&1 | tee "/tmp/verify_g100_${CP}.log"
  fi
}

grep_gate() {
  local pattern="$1"
  local msg="$2"
  if grep -qE "${pattern}" "/tmp/verify_g100_${CP}.log" 2>/dev/null; then
    log "PASS gate: ${msg}"
  else
    log "WARN gate not found in log: ${msg} (pattern: ${pattern})"
    log "      Check /tmp/verify_g100_${CP}.log manually"
  fi
}

case "${CP}" in
  CP-00)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo stress
    grep_gate 'DrawUnitG100 ready' 'G0 DrawUnitG100 init'
    grep_gate 'L3-G100|DrawUnitG100' 'G100 draw path active'
    ;;
  CP-01a)
    cmake_build simple_button
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-15}" run_demo simple_button
    ;;
  CP-01b)
    cmake_build render
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-30}" run_demo render
    log "Manual: apitrace AP-01~03; confirm GR-12 with LV_USE_VECTOR_GRAPHIC=0"
    ;;
  CP-01c)
    cmake_build simple_button
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-20}" run_demo simple_button
    RUN_SEC="${RUN_SEC:-30}" run_demo stress
    ;;
  CP-02)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo stress
    log "Manual: TX-02~04 dynamic label GPU; AP-06 no ReadPixels"
    ;;
  CP-03a|CP-03b)
    cmake_build render
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo render
    ;;
  CP-04a|CP-04b)
    cmake_build render
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo render
    log "Manual: BL-03~06 blur scenarios"
    ;;
  CP-05)
    cmake_build render
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo stress
    log "Milestone: tag g100-mvp-2d after PASS + update g100_test_results.md"
    ;;
  CP-06)
    cmake_build gltf
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo gltf
    log "Milestone: tag g100-mvp-3d-blit; gates D3-01~05"
    ;;
  CP-07a)
    cmake_build benchmark
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo benchmark
    log "Optional: 30min soak PF-03"
    ;;
  CP-07b)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    log "Optional libs/g100 path: run same as CP-07a after extract"
    ;;
  CP-08)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-30}" run_demo 3dviewport
    log "Gates D3-06, D3-16~17 (apitrace VP+resolve)"
    ;;
  CP-09)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dviewport
    ;;
  CP-10)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dscene
    ;;
  CP-11)
    cmake_build gltf
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo gltf
    log "Milestone: tag g100-mvp-3d-vp; gate D3-19 no glDraw in widget event"
    ;;
  CP-12|CP-13|CP-14)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dview
    [[ "${CP}" == "CP-14" ]] && log "Milestone: tag g100-mvp-3d-full"
    ;;
  *)
    log "Unknown checkpoint: ${CP}"
    usage
    exit 1
    ;;
esac

log "Done ${CP}. Log: /tmp/verify_g100_${CP}.log"
log "Next: fill docs/g100_test_results.md → commit lvgl → commit main → push → tag if milestone"
