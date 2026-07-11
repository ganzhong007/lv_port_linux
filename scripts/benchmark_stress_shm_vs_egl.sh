#!/usr/bin/env bash
# Build and run lv_demo_stress on SHM vs EGL; parse sysmon FPS from stderr.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION="${1:-45}"
BUILD_SHM="$ROOT/build-stress-shm"
BUILD_EGL="$ROOT/build-stress-egl"
LOG_DIR="$ROOT/benchmark_logs"
mkdir -p "$LOG_DIR"

CMAKE_COMMON=(
  -DLVGL_APP_DEMO=stress
  -DLVGL_SIMPLE_BUTTON_TRACE=OFF
)

echo "==> Building wayland (SHM) + stress demo -> $BUILD_SHM"
cmake -B "$BUILD_SHM" -DCONFIG=wayland "${CMAKE_COMMON[@]}" "$ROOT"
cmake --build "$BUILD_SHM" -j"$(nproc)"

echo "==> Building wayland-egl + stress demo -> $BUILD_EGL"
cmake -B "$BUILD_EGL" -DCONFIG=wayland-egl "${CMAKE_COMMON[@]}" "$ROOT"
cmake --build "$BUILD_EGL" -j"$(nproc)"

run_capture() {
  local name="$1"
  local bin="$2"
  local log="$LOG_DIR/${name}.log"

  echo "==> Running $name for ${DURATION}s (log: $log)"
  rm -f "$log"
  timeout "$DURATION" "$bin" -b wayland >"$log" 2>&1 || true

  python3 - "$log" "$name" <<'PY'
import re
import sys

path, label = sys.argv[1], sys.argv[2]
text = open(path, errors="replace").read()
fps = [int(x) for x in re.findall(r"sysmon:\s+(\d+)\s+FPS", text)]
cpu = [int(x) for x in re.findall(r"CPU \(total (\d+)%", text)]
render = [int(x) for x in re.findall(r"refr \d+ms \(render (\d+)ms", text)]
flush = [int(x) for x in re.findall(r"refr \d+ms \(render \d+ms \| flush (\d+)ms\)", text)]

# drop first few samples (startup)
fps = fps[3:]
cpu = cpu[3:]
render = render[3:]
flush = flush[3:]

if not fps:
    print(f"{label}: no FPS samples in log")
    sys.exit(0)

avg = sum(fps) / len(fps)
cpu_avg = sum(cpu) / len(cpu) if cpu else 0
render_avg = sum(render) / len(render) if render else 0
flush_avg = sum(flush) / len(flush) if flush else 0
print(f"{label}: fps_avg={avg:.1f} min={min(fps)} max={max(fps)} n={len(fps)} | cpu_avg={cpu_avg:.1f}% render={render_avg:.1f}ms flush={flush_avg:.1f}ms")
PY
}

run_capture "shm" "$BUILD_SHM/bin/lvglsim"
run_capture "egl" "$BUILD_EGL/bin/lvglsim"

echo
echo "Raw logs: $LOG_DIR/shm.log , $LOG_DIR/egl.log"
echo "Tip: grep 'sysmon:' benchmark_logs/*.log | tail -20"
