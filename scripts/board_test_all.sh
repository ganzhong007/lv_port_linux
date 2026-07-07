#!/bin/sh
# Board-side regression: Phase 1 perf smoke + Phase 2 verify-lite.
#   chmod +x board_test_all.sh lvglsim start_mali_gpu.sh
#   ./board_test_all.sh
set -eu

LOG="${BOARD_TEST_LOG:-board_test.log}"
RES="${BOARD_TEST_RES:--W 1920 -H 1080}"
PERF_SEC="${BOARD_TEST_PERF_SEC:-5}"
S56_SEC="${BOARD_TEST_S56_SEC:-8}"
RUN_VERIFY="${BOARD_TEST_VERIFY:-1}"

: > "$LOG"

log() {
    printf '%s\n' "$*"
    printf '%s\n' "$*" >> "$LOG"
}

run_lvglsim() {
    log "+ $*"
    "$@" >> "$LOG" 2>&1
}

log "=== board_test_all $(date '+%Y-%m-%d %H:%M:%S') ==="
log "RES=$RES PERF_SEC=$PERF_SEC S56_SEC=$S56_SEC RUN_VERIFY=$RUN_VERIFY pwd=$(pwd)"

chmod +x lvglsim start_mali_gpu.sh 2>/dev/null || true

if [ -f lvglsim ]; then
    ls -lh lvglsim >> "$LOG" 2>&1
    ls -lh lvglsim
    file lvglsim >> "$LOG" 2>&1 || true
else
    log "FAIL: lvglsim not found in $(pwd)"
    exit 1
fi

log "--- start_mali_gpu ---"
./start_mali_gpu.sh >> "$LOG" 2>&1
./start_mali_gpu.sh

log "--- backends ---"
./lvglsim -B >> "$LOG" 2>&1 || true
./lvglsim -B || true

FAIL=0

log "=== PERF smoke (LVGL_DRM_TURBO=1, no VERIFY) ==="
for s in 1 2 3 4 5 6; do
    SEC="$PERF_SEC"
    if [ "$s" = "5" ] || [ "$s" = "6" ]; then
        SEC="$S56_SEC"
    fi
    log "--- PERF scenario $s (${SEC}s) ---"
    if command -v timeout >/dev/null 2>&1; then
        timeout "$SEC" env LVGL_DRM_TURBO=1 LVGL_SCENARIO="$s" ./lvglsim -b drm $RES >> "$LOG" 2>&1 || true
    else
        log "WARN: no timeout(1); run scenario $s until Ctrl+C"
        env LVGL_DRM_TURBO=1 LVGL_SCENARIO="$s" ./lvglsim -b drm $RES >> "$LOG" 2>&1 || true
    fi
    log "PASS PERF scenario $s (visual check)"
done

if [ "$RUN_VERIFY" = "0" ]; then
    log "=== VERIFY-LITE skipped (BOARD_TEST_VERIFY=0) ==="
    log "=== summary fail_count=$FAIL log=$LOG ==="
    exit "$FAIL"
fi

log "=== VERIFY-LITE (LVGL_VERIFY_LITE=1, short frames) ==="
for s in 1 2 3 4; do
    log "--- VERIFY-LITE scenario $s ---"
    if run_lvglsim env LVGL_SCENARIO="$s" LVGL_VERIFY=1 LVGL_VERIFY_LITE=1 LVGL_VERIFY_FG=1 \
            LVGL_VERIFY_SW_RASTER=1 LVGL_VERIFY_FRAMES=12 LVGL_VERIFY_WARMUP=4 LVGL_DRM_TURBO=1 \
            ./lvglsim -b drm $RES; then
        log "PASS VERIFY-LITE scenario $s exit=0"
    else
        log "FAIL VERIFY-LITE scenario $s exit=$?"
        FAIL=$((FAIL + 1))
    fi
done

log "--- VERIFY-LITE scenario 8 (GPU 2D edges, sw_raster=0) ---"
if run_lvglsim env LVGL_SCENARIO=8 LVGL_VERIFY=1 LVGL_VERIFY_LITE=1 LVGL_VERIFY_FG=1 \
        LVGL_VERIFY_GPU_PATH=1 LVGL_VERIFY_SW_RASTER=1 \
        LVGL_GPU_GLYPH_ATLAS_SIZE=128 \
        LVGL_VERIFY_FRAMES=12 LVGL_VERIFY_WARMUP=4 LVGL_DRM_TURBO=1 \
        ./lvglsim -b drm $RES; then
    log "PASS VERIFY-LITE scenario 8 exit=0"
else
    log "FAIL VERIFY-LITE scenario 8 exit=$?"
    FAIL=$((FAIL + 1))
fi

log "=== summary fail_count=$FAIL log=$LOG ==="
exit "$FAIL"
