#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/SoCloud-win.zip"

cat "$ROOT"/SoCloud-win.zip.part.* > "$OUT"
echo "Restored $OUT ($(du -h "$OUT" | cut -f1))"
