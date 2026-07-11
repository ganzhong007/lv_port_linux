#!/usr/bin/env bash
# Build and run lv_demo_benchmark on SHM vs EGL; parse CSV summary from log.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TIMEOUT_SEC="${1:-120}"
BUILD_SHM="$ROOT/build-bench-shm"
BUILD_EGL="$ROOT/build-bench-egl"
LOG_DIR="$ROOT/benchmark_logs"
mkdir -p "$LOG_DIR"

CMAKE_COMMON=(
  -DLVGL_APP_DEMO=benchmark
  -DLVGL_SIMPLE_BUTTON_TRACE=OFF
)

echo "==> Building wayland (SHM) + benchmark -> $BUILD_SHM"
cmake -B "$BUILD_SHM" -DCONFIG=wayland "${CMAKE_COMMON[@]}" "$ROOT"
cmake --build "$BUILD_SHM" -j"$(nproc)"

echo "==> Building wayland-egl + benchmark -> $BUILD_EGL"
cmake -B "$BUILD_EGL" -DCONFIG=wayland-egl "${CMAKE_COMMON[@]}" "$ROOT"
cmake --build "$BUILD_EGL" -j"$(nproc)"

run_benchmark() {
  local name="$1"
  local bin="$2"
  local log="$LOG_DIR/benchmark_${name}.log"

  echo "==> Running lv_demo_benchmark ($name), timeout ${TIMEOUT_SEC}s -> $log"
  rm -f "$log"
  timeout "$TIMEOUT_SEC" "$bin" -b wayland >"$log" 2>&1 || true

  python3 - "$log" "$name" <<'PY'
import re
import sys

path, label = sys.argv[1], sys.argv[2]
lines = open(path, errors="replace").read().splitlines()

rows = []
for line in lines:
    # CSV rows from lv_demo_benchmark_summary_display
    m = re.search(
        r"^(.+?), ?(\d+)%, ?(\d+), ?(\d+), ?(\d+), ?(\d+)\s*$",
        line.strip().split("\t")[-1] if "\t" in line else line.strip(),
    )
    if not m:
        m = re.search(r"(.+?), ?(\d+)%, ?(\d+), ?(\d+), ?(\d+), ?(\d+)\s*$", line)
    if m:
        rows.append(m.groups())

if not rows:
    # fallback: grep-like loose match
    for line in lines:
        if ", %" in line or re.search(r", \d+%, \d+,", line):
            parts = re.findall(r"([^,\r\n]+)", line)
            if len(parts) >= 6 and parts[1].strip().endswith("%"):
                try:
                    rows.append((
                        parts[0].strip(),
                        parts[1].replace("%", "").strip(),
                        parts[2].strip(), parts[3].strip(),
                        parts[4].strip(), parts[5].strip(),
                    ))
                except Exception:
                    pass

print(f"\n=== {label.upper()} (lv_demo_benchmark) ===")
if not rows:
    print("  (benchmark did not finish — no CSV summary in log)")
    print(f"  tail: {path}")
    sys.exit(0)

print(f"  {'Scene':<32} {'CPU%':>5} {'FPS':>5} {'total':>6} {'render':>7} {'flush':>6}")
print(f"  {'-'*32} {'-'*5} {'-'*5} {'-'*6} {'-'*7} {'-'*6}")
for name, cpu, fps, total, render, flush in rows:
    print(f"  {name[:32]:<32} {cpu:>5} {fps:>5} {total:>6} {render:>7} {flush:>6}")

avg = next((r for r in rows if r[0].startswith("All scenes avg")), None)
if avg:
    print(f"\n  >> Overall: FPS={avg[2]}  CPU={avg[1]}%  render={avg[4]}ms  flush={avg[5]}ms")
PY
}

run_benchmark shm "$BUILD_SHM/bin/lvglsim"
run_benchmark egl "$BUILD_EGL/bin/lvglsim"

echo
echo "Full logs: $LOG_DIR/benchmark_shm.log , $LOG_DIR/benchmark_egl.log"
