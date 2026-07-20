#!/usr/bin/env bash
# Cross-build aarch64 lvglsim for ATK-MPSoC (Petalinux 2020.2 + Weston Wayland SHM).
# See docs/atk_a53_simple_button.md
#
# Usage:
#   DEMO=simple_button ./scripts/build_a53weston_cross.sh
#
# Env overrides:
#   DEMO              app demo (default: simple_button)
#   CONFIG            cmake -DCONFIG=... (default: a53weston)
#   BUILD             build dir (default: <repo>/build-a53weston)
#   PETALINUX_ENV     SDK environment-setup script
#   CMAKE_BUILD_TYPE  default: Release
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-${LVGL_BUILD_DIR:-$ROOT/build-a53weston}}"
CONFIG="${CONFIG:-a53weston}"
DEMO="${DEMO:-simple_button}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
PETALINUX_ENV="${PETALINUX_ENV:-/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$ROOT/user_cross_compile_a53.cmake}"
CONFIG_DEFAULTS="$ROOT/configs/${CONFIG}.defaults"

die() {
  echo "ERROR: $*" >&2
  exit 1
}

[[ -f "$PETALINUX_ENV" ]] || die "Petalinux SDK not found: $PETALINUX_ENV"
[[ -f "$TOOLCHAIN_FILE" ]] || die "missing toolchain file: $TOOLCHAIN_FILE (see docs/atk_a53_simple_button.md)"
[[ -f "$CONFIG_DEFAULTS" ]] || die "missing config: $CONFIG_DEFAULTS"
[[ -d "$ROOT/lvgl" ]] || die "lvgl submodule missing; run: git submodule update --init --recursive"

# Prefer repo scripts/wayland-scanner (board wayland-client 1.17 compatible) over host scanner.
export PATH="$ROOT/scripts:$PATH"

# Petalinux environment-setup may reference unset vars; relax nounset while sourcing.
set +u
# shellcheck source=/dev/null
source "$PETALINUX_ENV"
set -u

echo "==> A53 Weston cross build"
echo "    ROOT=$ROOT"
echo "    BUILD=$BUILD"
echo "    CONFIG=$CONFIG  DEMO=$DEMO  TYPE=$CMAKE_BUILD_TYPE"
echo "    TOOLCHAIN=$TOOLCHAIN_FILE"
echo "    CC=${CC:-?}  CXX=${CXX:-?}"
command -v wayland-scanner >/dev/null && echo "    wayland-scanner=$(command -v wayland-scanner)"

cmake -B "$BUILD" -S "$ROOT" \
  -DCONFIG="$CONFIG" \
  -DLVGL_APP_DEMO="$DEMO" \
  -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"

cmake --build "$BUILD" -j"$(nproc)" --target lvglsim

BIN="$BUILD/bin/lvglsim"
[[ -x "$BIN" ]] || die "build finished but binary missing: $BIN"

echo "==> product:"
file "$BIN"
echo "Upload: scp $BIN root@192.168.137.42:/root/lvglsim"
echo "Run on board: XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0 /root/lvglsim -b wayland -W 800 -H 480"
