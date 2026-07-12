#!/usr/bin/env bash
# G100 perf baseline: stress (PF-01) + lv_demo_benchmark (PF-02) on wayland-g100.
#
# Usage:
#   ./scripts/benchmark_g100.sh [stress_sec] [benchmark_sec]
#   ./scripts/benchmark_g100.sh 60 120
#
# Logs: benchmark_logs/g100_stress.log , benchmark_logs/g100_benchmark.log
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STRESS_SEC="${1:-60}"
BENCH_SEC="${2:-120}"
CONFIG="${CONFIG:-wayland-g100}"
W="${W:-800}"
H="${H:-480}"
DRAW_MULT="${DRAW_MULT:-1}"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build-g100-stress}"
LOG_DIR="$ROOT/benchmark_logs"
mkdir -p "$LOG_DIR"

RUN_ARGS=(-b wayland -W "$W" -H "$H")
CMAKE_COMMON=(-DCONFIG="$CONFIG" -DLVGL_SIMPLE_BUTTON_TRACE=OFF)

log() { echo "[benchmark_g100] $*"; }

cmake_build() {
  local demo="$1"
  local extra=()
  if [[ "$demo" == "stress" ]]; then
    extra+=(-DLV_DEMO_STRESS_DRAW_MULT="$DRAW_MULT")
  fi
  log "cmake -B $BUILD -DLVGL_APP_DEMO=$demo ${extra[*]}"
  cmake -B "$BUILD" "${CMAKE_COMMON[@]}" -DLVGL_APP_DEMO="$demo" "${extra[@]}"
  cmake --build "$BUILD" -j"$(nproc)"
}

parse_stress_fps() {
  local log="$1"
  python3 - "$log" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
fps = [int(x) for x in re.findall(r"sysmon:\s+(\d+)\s+FPS", text)]
fps = fps[3:]
if not fps:
    print("  no FPS samples")
    sys.exit(1)
avg = sum(fps) / len(fps)
print(f"  stress fps_avg={avg:.1f} min={min(fps)} max={max(fps)} n={len(fps)}")
PY
}

parse_benchmark_csv() {
  local log="$1"
  python3 - "$log" <<'PY'
import re, sys
lines = open(sys.argv[1], errors="replace").read().splitlines()
rows = []
for line in lines:
    if "All scenes avg" in line or re.search(r"All scenes avg\.,", line):
        m = re.search(r"All scenes avg\.?,(\d+)%,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)", line.replace(" ", ""))
        if not m:
            m = re.search(r"All scenes avg\.?,(\d+)%,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)", line)
        if m:
            cpu, fps, total, render, flush = m.groups()
            print(f"  benchmark summary: CPU={cpu}% FPS={fps} render={render}ms flush={flush}ms")
            sys.exit(0)
print("  benchmark summary not found (increase benchmark_sec?)")
sys.exit(1)
PY
}

log "==> PF-01 stress ${STRESS_SEC}s @ ${W}x${H} (DRAW_MULT=${DRAW_MULT})"
cmake_build stress
STRESS_LOG="$LOG_DIR/g100_stress.log"
rm -f "$STRESS_LOG"
timeout "$STRESS_SEC" "$BUILD/bin/lvglsim" "${RUN_ARGS[@]}" >"$STRESS_LOG" 2>&1 || true
parse_stress_fps "$STRESS_LOG"

log "==> PF-02 benchmark ${BENCH_SEC}s"
cmake_build benchmark
BENCH_LOG="$LOG_DIR/g100_benchmark.log"
rm -f "$BENCH_LOG"
timeout "$BENCH_SEC" "$BUILD/bin/lvglsim" "${RUN_ARGS[@]}" >"$BENCH_LOG" 2>&1 || true
parse_benchmark_csv "$BENCH_LOG" || log "WARN: benchmark may need more time (default 120s+)"

log "Done. Logs: $STRESS_LOG , $BENCH_LOG"
log "Optional PF-03 soak: RUN_SEC=1800 ./scripts/verify_g100.sh CP-07a (stress 30min)"
