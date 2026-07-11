#!/usr/bin/env bash
# Capture before/after PNGs for docs/simple_button_guide.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$ROOT/docs/images"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build}"
BIN="$BUILD/bin/lvglsim"

mkdir -p "$IMG"
rm -f "$IMG"/simple_button_before.ppm "$IMG"/simple_button_after.ppm \
      "$IMG"/simple_button_before.png "$IMG"/simple_button_after.png

echo "Capturing LVGL snapshots -> $IMG"
SIMPLE_BUTTON_DOC_SHOTS="$IMG" timeout 15 "$BIN" -b wayland 2>/dev/null || true

for name in before after; do
  ppm="$IMG/simple_button_${name}.ppm"
  png="$IMG/simple_button_${name}.png"
  if [[ ! -f "$ppm" ]]; then
    echo "Missing $ppm"
    exit 1
  fi
  convert "$ppm" "$png"
  rm -f "$ppm"
  echo "Wrote $png"
done
