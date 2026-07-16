#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REAL_SCANNER="${WAYLAND_SCANNER_REAL:-/usr/bin/wayland-scanner}"

"${REAL_SCANNER}" "$@"

if [[ "$1" == "client-header" && -n "${3:-}" ]]; then
  python3 "${ROOT}/scripts/wayland-scanner-compat17.py" "$3"
fi
