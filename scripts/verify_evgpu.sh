#!/usr/bin/env bash
# Verify DrawUnitEVGPU / G8 checkpoint (CP-XX).
# Usage:
#   ./scripts/verify_evgpu.sh CP-00
#   ./scripts/verify_evgpu.sh CP-01b --build-only
#   RUN_SEC=45 ./scripts/verify_evgpu.sh CP-00
#
# See docs/evgpu_test_results.md and drawunit_evgpu_design.md §8.8.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${1:-}"
BUILD_ONLY=false
if [[ "${2:-}" == "--build-only" ]]; then
  BUILD_ONLY=true
fi

CONFIG="${CONFIG:-wayland-evgpu}"
W="${W:-800}"
H="${H:-480}"
RUN_SEC="${RUN_SEC:-0}"
BUILD="${LVGL_BUILD_DIR:-${ROOT}/build-evgpu-stress}"
BIN="${BUILD}/bin/lvglsim"

usage() {
  cat <<'EOF'
Usage: ./scripts/verify_evgpu.sh CP-XX [--build-only]

Checkpoints:
  CP-00     G0 Bootstrap: build + stress smoke (DrawUnitEVGPU ready)
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
  CP-07b    G7 optional libs/evgpu (no regression)
  CP-08     G8.0 3D_VIEWPORT + CLEAR
  CP-09     G8.1 3D_LINE + CALLBACK
  CP-10     G8.2 3D_MESH
  CP-11     G8.3 3D_SCENE + gltf migration
  CP-12     G8.4 phong / lights
  CP-13     G8.5 pick + OBJ loader
  CP-14     G8.6 3D theme/style

Env:
  CONFIG=wayland-evgpu   LVGL_BUILD_DIR=...   W/H=800/480
  RUN_SEC=45            Run demo N seconds then exit (0=manual)
  STRESS_MULT=1         LV_DEMO_STRESS_DRAW_MULT for stress builds

After PASS: update docs/evgpu_test_results.md, commit lvgl + main, push, tag if milestone.
EOF
}

if [[ -z "${CP}" ]] || [[ "${CP}" == "-h" ]] || [[ "${CP}" == "--help" ]]; then
  usage
  exit 0
fi

log() { echo "[verify_evgpu] $*"; }

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
    timeout "${RUN_SEC}" "${BIN}" -b wayland -W "${W}" -H "${H}" 2>&1 | tee "/tmp/verify_evgpu_${CP}.log" || {
      local ec=$?
      if [[ ${ec} -eq 124 ]]; then
        log "Run finished after ${RUN_SEC}s (timeout expected)"
        return 0
      fi
      return "${ec}"
    }
  else
    log "Starting interactively; Ctrl+C to stop. Log: /tmp/verify_evgpu_${CP}.log"
    "${BIN}" -b wayland -W "${W}" -H "${H}" 2>&1 | tee "/tmp/verify_evgpu_${CP}.log"
  fi
}

grep_gate() {
  local pattern="$1"
  local msg="$2"
  if grep -qE "${pattern}" "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
    log "PASS gate: ${msg}"
  else
    log "WARN gate not found in log: ${msg} (pattern: ${pattern})"
    log "      Check /tmp/verify_evgpu_${CP}.log manually"
  fi
}

case "${CP}" in
  CP-00)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo stress
    grep_gate 'DrawUnitEVGPU ready' 'G0 DrawUnitEVGPU init'
    grep_gate 'L3-EVGPU|DrawUnitEVGPU' 'EVGPU draw path active'
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
    grep_gate 'EVGPU native gradient ready' 'EVGPU native grad shader init'
    if grep -qE 'Gradient fill is not supported' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: grad fallback warn found (GR-12 may fail)"
    else
      log "PASS gate: no grad VECTOR fallback warn"
    fi
    log "Manual: apitrace AP-01~03; confirm GR-12 with LV_USE_VECTOR_GRAPHIC=0"
    ;;
  CP-01c)
    cmake_build simple_button
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-20}" run_demo simple_button
    grep_gate 'EVGPU native solid fill ready' 'EVGPU native solid fill init'
    grep_gate 'EVGPU native texture draw ready' 'EVGPU native tex draw init'
    RUN_SEC="${RUN_SEC:-30}" run_demo stress
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under stress'
    ;;
  CP-02)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-5}" run_demo stress
    grep_gate 'EVGPU label text hash' 'label text hash cache active'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under stress'
    log "Manual: TX-02~04 dynamic label GPU; AP-06 no ReadPixels"
    ;;
  CP-03a)
    cmake_build render
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-5}" run_demo render
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under render'
    grep_gate 'EVGPU vector core ready' 'vector SOLID/GRAD/dash path active'
    if grep -qE 'unsupported style' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: unsupported style warn found (VC-01 may fail)"
    else
      log "PASS gate: no unsupported style warn"
    fi
    ;;
  CP-03b)
    cmake_build vector_graphic
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-5}" run_demo vector_graphic
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under vector_graphic'
    grep_gate 'EVGPU vector pattern fill' 'vector PATTERN GPU fill active'
    if grep -qE 'unsupported style' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: unsupported style warn found (VC-04 may fail)"
    else
      log "PASS gate: no unsupported style warn"
    fi
    ;;
  CP-04a)
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-5}" run_demo stress
    grep_gate 'EVGPU kawase blur ready' 'kawase blur init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under stress'
    if grep -qE 'exceeds backend limit \\(256\\)' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: legacy 256 blur skip found (BL-03 may fail)"
    else
      log "PASS gate: no 256 blur radius skip"
    fi
    ;;
  CP-04b)
    cmake_build render
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-5}" run_demo render
    grep_gate 'EVGPU blur FBO pool ready' 'blur FBO pool init'
    grep_gate 'fbo_ok=true' 'FBO pool capability flag'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under render'
    log "Manual: BL-05~06 blur FBO pool + box_shadow scenarios"
    ;;
  CP-05)
    cmake_build render
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo stress
    grep_gate 'EVGPU G5 complete 2D ready' 'G5 2D task coverage'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under stress'
    if grep -qE 'unsupported style|Gradient fill is not supported' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: unsupported grad/style warn found (D2/GR may fail)"
    else
      log "PASS gate: no unsupported grad/style warn"
    fi
    log "Milestone: tag evgpu-mvp-2d after PASS + update evgpu_test_results.md"
    ;;
  CP-06)
    cmake_build gltf
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo gltf
    grep_gate 'EVGPU 3D BLIT ready' 'G6 3D BLIT init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under gltf'
    if grep -qE 'GL error|Failed to load glTF|Failed to read the entire gltf|assert\\(0\\)' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: GL/gltf load error in log"
    else
      log "PASS gate: no GL/gltf load error"
    fi
    log "Manual: D3-01 visual (3D+2D panel); tag evgpu-mvp-3d-blit after PASS"
    ;;
  CP-07a)
    cmake_build benchmark
    cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo stress
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active (PF-01 stress)'
    if grep -qE 'sysmon: [1-9][0-9]* FPS' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "PASS gate: sysmon FPS samples (PF-01)"
    else
      log "WARN gate: no stable FPS in stress log (PF-01)"
    fi
    if [[ "${BENCH_RUN:-true}" == true ]]; then
      bench_sec="${BENCH_SEC:-120}"
      log "Running benchmark ${bench_sec}s for PF-02"
      cmake_build benchmark
      timeout "${bench_sec}" "${BIN}" -b wayland -W "${W}" -H "${H}" 2>&1 | tee -a "/tmp/verify_evgpu_${CP}.log" || true
    fi
    if grep -qE 'All scenes avg' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "PASS gate: benchmark CSV summary (PF-02)"
    else
      log "WARN gate: no benchmark summary — try BENCH_SEC=180 or ./scripts/benchmark_evgpu.sh 60 120"
    fi
    log "Perf script: ./scripts/benchmark_evgpu.sh [stress_sec] [benchmark_sec]"
    log "Optional PF-03 soak: RUN_SEC=1800 ./scripts/verify_evgpu.sh CP-07a (stress only)"
    ;;
  CP-07b)
    log "Path A (LV_USE_EVGPU_LIB=0): no regression"
    CONFIG="${CONFIG:-wayland-evgpu}" cmake_build stress
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-15}" run_demo stress
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU path A stress'
    if grep -qE 'EVGPU lib ready' "/tmp/verify_evgpu_${CP}.log" 2>/dev/null; then
      log "WARN gate: EVGPU lib log on path A (expected LV_USE_EVGPU_LIB=0)"
    else
      log "PASS gate: path A uses draw/evgpu inline runtime"
    fi
    log "Path B (LV_USE_EVGPU_LIB=1): libs/evgpu facade"
    CONFIG=wayland-evgpu-lib cmake_build stress
    RUN_SEC="${RUN_SEC:-15}" run_demo stress
    grep_gate 'EVGPU lib ready' 'libs/evgpu runtime init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU path B stress'
    log "Optional: full perf regression ./scripts/benchmark_evgpu.sh 60 120"
    ;;
  CP-08)
    cmake_build 3dviewport
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-30}" run_demo 3dviewport
    grep_gate 'EVGPU 3D viewport ready' 'G8.0 3D viewport init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dviewport'
    log "Gates D3-06, D3-16~17 (apitrace VP+resolve)"
    ;;
  CP-09)
    cmake_build 3dviewport
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dviewport
    grep_gate 'EVGPU 3D line ready' 'G8.1 LINE3D shader init'
    grep_gate 'EVGPU 3D callback ready' 'G8.1 3D_CALLBACK hook'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dviewport'
    log "Gates D3-07~08, AP-08 (grid + render_cb + orbit)"
    ;;
  CP-10)
    cmake_build 3dscene
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dscene
    grep_gate 'EVGPU 3D mesh ready' 'G8.2 MESH shader init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dscene'
    log "Gates D3-11, D3-18 (mesh + depth occlusion)"
    ;;
  CP-11)
    cmake_build gltf
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-60}" run_demo gltf
    grep_gate 'EVGPU 3D scene ready' 'G8.3 SCENE task init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under gltf'
    log "Milestone: tag evgpu-mvp-3d-vp; gate D3-19 no glDraw in widget event"
    ;;
  CP-12)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dview
    grep_gate 'EVGPU 3D phong ready' 'G8.4 phong shader init'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dview'
    log "Gate D3-13 (phong + lv_3dlight)"
    ;;
  CP-13)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dview
    grep_gate 'EVGPU 3D pick ready' 'G8.5 pick ray-mesh'
    grep_gate 'EVGPU OBJ loader ready' 'G8.5 wavefront loader'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dview'
    log "Gate D3-14 (pick + OBJ loader + lv_3dcaps)"
    ;;
  CP-14)
    cmake_build 3dview
    [[ "${BUILD_ONLY}" == true ]] && exit 0
    RUN_SEC="${RUN_SEC:-45}" run_demo 3dview
    grep_gate 'EVGPU 3D theme ready' 'G8.6 LV_STYLE_3D props'
    grep_gate 'DrawUnitEVGPU ready' 'EVGPU unit active under 3dview'
    log "Gate D3-15 (3D theme/style)"
    log "Milestone: tag evgpu-mvp-3d-full"
    ;;
  *)
    log "Unknown checkpoint: ${CP}"
    usage
    exit 1
    ;;
esac

log "Done ${CP}. Log: /tmp/verify_evgpu_${CP}.log"
log "Next: fill docs/evgpu_test_results.md → commit lvgl → commit main → push → tag if milestone"
