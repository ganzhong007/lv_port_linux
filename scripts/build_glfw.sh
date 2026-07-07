#!/usr/bin/env bash
# Host x86_64 build: lvglsim with GLFW + gpu_renderer (lvgl2d3dbackend-glfw).
#
# Clears PetaLinux/OE SDK variables so host gcc/pkg-config are used even when
# environment-setup-aarch64-xilinx-linux was sourced in the same shell.
#
# Usage:
#   ./scripts/build_glfw.sh
#   LVGL_SCENARIO=4 ./build-vgl-glfw/bin/lvglsim -b glfw
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_BUILD_DIR:-$ROOT/build-vgl-glfw}"
CONFIG="${LVGL_GLFW_CONFIG:-lvgl2d3dbackend-glfw}"
JOBS="${JOBS:-$(nproc)}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

# Drop cross-SDK pollution (PetaLinux/Yocto) for native host compile.
for v in \
    OECORE_TARGET_SYSROOT OECORE_NATIVE_SYSROOT SDKTARGETSYSROOT \
    PKG_CONFIG_PATH PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR \
    CC CXX CPP LD AR RANLIB STRIP NM OBJCOPY OBJDUMP \
    CMAKE_TOOLCHAIN_FILE OE_CMAKE_TOOLCHAIN_FILE \
    OE_QMAKE_CC OE_QMAKE_CXX OE_QMAKE_LINK \
    CONFIG_SITE KCFLAGS CONFIGURE_FLAGS CFLAGS CXXFLAGS LDFLAGS \
    CPPFLAGS CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH \
    QMAKESPEC QT_CONF_PATH OPENSSL_CONF \
    OE_QMAKE_INCDIR_QT OE_QMAKE_LIBDIR_QT OE_QMAKE_PATH_HOST_BINS
do
    unset "$v" 2>/dev/null || true
done

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${HOME}/.local/bin"
export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"

echo "==> Host GLFW build"
echo "    build dir : $BUILD"
echo "    config    : $CONFIG"
echo "    CC        : $CC"
echo "    CXX       : $CXX"

CMAKE_ARGS=(
    -DCMAKE_C_COMPILER="$CC"
    -DCMAKE_CXX_COMPILER="$CXX"
    -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config
    -DCONFIG="$CONFIG"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

cmake -B "$BUILD" "${CMAKE_ARGS[@]}" "$ROOT"
cmake --build "$BUILD" -j"$JOBS" --target lvglsim

OUT="$BUILD/bin/lvglsim"
if [[ ! -x "$OUT" ]]; then
    echo "error: $OUT not produced" >&2
    exit 1
fi

echo ""
echo "==> Built: $OUT"
file "$OUT"
echo ""
echo "Run:"
echo "  LVGL_SCENARIO=1 $OUT -b glfw"
echo "  ./scripts/verify_all_scenarios.sh"
