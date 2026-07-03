#!/usr/bin/env bash
# Build AArch64 Cortex-A53 static lvglsim (gpu_renderer + GLFW).
#
# Output: build-a53-static/bin/lvglsim
# Target: Linux AArch64 (Cortex-A53). Runtime needs libEGL.so + libGLESv2.so on device.
#
# Usage:
#   ./scripts/build_a53_static.sh
#   LVGL_SCENARIO=4 ./build-a53-static/bin/lvglsim -b glfw
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_A53_BUILD_DIR:-$ROOT/build-a53-static}"
DEPS="$BUILD/deps"
TC="$ROOT/cmake/toolchains/aarch64-cortex-a53-linux-gnu.cmake"
CONFIG="${LVGL_A53_CONFIG:-lvgl2d3dbackend-a53-glfw}"
JOBS="${JOBS:-$(nproc)}"
SRC="$BUILD/_src"

export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
export AR=aarch64-linux-gnu-ar
export RANLIB=aarch64-linux-gnu-ranlib
export CFLAGS="-mcpu=cortex-a53 -O2 -fno-lto --sysroot=/usr/aarch64-linux-gnu"
export CXXFLAGS="-mcpu=cortex-a53 -O2 -fno-lto --sysroot=/usr/aarch64-linux-gnu"
export LDFLAGS="-fno-lto --sysroot=/usr/aarch64-linux-gnu"


if ! command -v "$CC" >/dev/null 2>&1; then
    echo "error: install cross compiler: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu" >&2
    exit 1
fi

clone_once() {
    local name="$1" url="$2" tag="$3"
    local dir="$SRC/$name"
    if [[ ! -d "$dir/.git" ]]; then
        echo "==> clone $name ($tag)"
        mkdir -p "$SRC"
        git clone --depth 1 --branch "$tag" "$url" "$dir"
    fi
}

build_glfw() {
    if [[ -f "$DEPS/lib/libglfw3.a" ]]; then
        return
    fi
    clone_once glfw https://github.com/glfw/glfw.git 3.4
    echo "==> build GLFW (static, aarch64)"
    cmake -S "$SRC/glfw" -B "$BUILD/glfw" \
        -DCMAKE_TOOLCHAIN_FILE="$TC" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DGLFW_BUILD_EXAMPLES=OFF \
        -DGLFW_BUILD_TESTS=OFF \
        -DGLFW_BUILD_DOCS=OFF \
        -DGLFW_INSTALL=OFF \
        -DGLFW_BUILD_X11=ON \
        -DGLFW_BUILD_WAYLAND=OFF
    cmake --build "$BUILD/glfw" -j"$JOBS"
    mkdir -p "$DEPS/lib" "$DEPS/include"
    cp "$BUILD/glfw/src/libglfw3.a" "$DEPS/lib/"
    cp -r "$SRC/glfw/include/GLFW" "$DEPS/include/"
}

build_glew() {
    if [[ -f "$DEPS/lib/libGLEW.a" ]]; then
        return
    fi
    clone_once glew-cmake https://github.com/Perlmint/glew-cmake.git v2.2.0-4
    echo "==> build GLEW (static, aarch64)"
    cmake -S "$SRC/glew-cmake" -B "$BUILD/glew" \
        -DCMAKE_TOOLCHAIN_FILE="$TC" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_UTILS=OFF \
        -DONLY_LIBS=ON
    cmake --build "$BUILD/glew" -j"$JOBS"
    mkdir -p "$DEPS/lib" "$DEPS/include"
    cp "$BUILD/glew/lib/libGLEW.a" "$DEPS/lib/"
    cp -r "$SRC/glew-cmake/include/GL" "$DEPS/include/"
}

echo "==> AArch64 Cortex-A53 static build"
echo "    build dir : $BUILD"
echo "    deps dir  : $DEPS"
echo "    config    : $CONFIG"

build_glfw
build_glew

cmake -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$TC" \
    -DCONFIG="$CONFIG" \
    -DLV_A53_STATIC=ON \
    -DA53_DEPS_PREFIX="$DEPS" \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    "$ROOT"

cmake --build "$BUILD" -j"$JOBS" --target lvglsim

OUT="$BUILD/bin/lvglsim"
if [[ ! -x "$OUT" ]]; then
    echo "error: $OUT not produced" >&2
    exit 1
fi

echo ""
echo "==> Built: $OUT"
file "$OUT"
ldd "$OUT" 2>/dev/null || true
echo ""
echo "On device (needs EGL/GLES + X11):"
echo "  LVGL_SCENARIO=4 ./lvglsim -b glfw"
echo "Host smoke test (qemu):"
echo "  qemu-aarch64-static -L /usr/aarch64-linux-gnu $OUT -b glfw"
