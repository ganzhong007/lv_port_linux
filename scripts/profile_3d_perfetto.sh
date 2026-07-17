#!/usr/bin/env bash
# Capture an LVGL built-in profiler trace of a 3D demo and prepare it for Perfetto.
#
# Flow:
#   1. build lvglsim with configs/wayland-evgpu-profile.defaults (profiler ON)
#   2. run the chosen 3D demo for N seconds on WSLg Wayland-EGL
#   3. keep only the systrace lines (stdout) -> raw trace log
#   4. trace_filter.py -> <name>.systrace  (drag&drop into https://ui.perfetto.dev)
#   5. print a quick bottleneck summary (top functions by total/self time)
#
# Usage:
#   ./scripts/profile_3d_perfetto.sh [demo] [seconds] [W] [H]
#   ./scripts/profile_3d_perfetto.sh 3dscene 5
#
# demo: 3dscene (default) | 3dviewport | 3dview | gltf
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEMO="${1:-3dscene}"
SEC="${2:-5}"
W="${3:-800}"
H="${4:-480}"

CONFIG="wayland-evgpu-profile"
BUILD="${ROOT}/build-evgpu-3dscene-profile"
LOG_DIR="${ROOT}/benchmark_logs"
RAW_LOG="${LOG_DIR}/${DEMO}_trace.txt"
SYSTRACE="${LOG_DIR}/${DEMO}_trace.systrace"

mkdir -p "${LOG_DIR}"

# WSLg GLES runtime
export LD_LIBRARY_PATH="/usr/lib/wsl/lib:${LD_LIBRARY_PATH:-}"

echo "[profile] cmake configure (CONFIG=${CONFIG}, DEMO=${DEMO})"
cmake -B "${BUILD}" -DCONFIG="${CONFIG}" -DLVGL_APP_DEMO="${DEMO}" -DLVGL_SIMPLE_BUTTON_TRACE=OFF >/dev/null
echo "[profile] build"
cmake --build "${BUILD}" -j"$(nproc)" >/dev/null

echo "[profile] run ${DEMO} for ${SEC}s @ ${W}x${H} (trace -> stdout)"
rm -f "${RAW_LOG}"
# stdout = profiler systrace stream ; stderr = sysmon/logs (kept separate)
timeout "${SEC}" "${BUILD}/bin/lvglsim" -b wayland -W "${W}" -H "${H}" \
    >"${RAW_LOG}" 2>"${LOG_DIR}/${DEMO}_sysmon.log" || true

LINES="$(grep -c 'tracing_mark_write' "${RAW_LOG}" || true)"
echo "[profile] captured ${LINES} trace events -> ${RAW_LOG}"
if [[ "${LINES}" == "0" ]]; then
    echo "[profile] ERROR: no trace events captured. Check ${LOG_DIR}/${DEMO}_sysmon.log"
    exit 1
fi

echo "[profile] filter -> ${SYSTRACE}"
python3 "${ROOT}/lvgl/scripts/trace_filter.py" "${RAW_LOG}" "${SYSTRACE}" >/dev/null

echo "[profile] ===== bottleneck summary (top by total wall time) ====="
python3 "${ROOT}/scripts/analyze_trace.py" "${SYSTRACE}"

echo
echo "[profile] Perfetto: open https://ui.perfetto.dev and drag in:"
echo "          ${SYSTRACE}"
