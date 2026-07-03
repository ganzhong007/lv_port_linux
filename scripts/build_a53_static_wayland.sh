#!/usr/bin/env bash
# Build AArch64 Cortex-A53 static lvglsim (gpu_renderer + Wayland/EGL).
#
# Output: build-a53-wayland-static/bin/lvglsim
# Target: Linux AArch64 (Cortex-A53). Runtime needs libEGL.so + libGLESv2.so + Wayland compositor.
#
# Usage:
#   ./scripts/build_a53_static_wayland.sh
#   LVGL_SCENARIO=4 ./build-a53-wayland-static/bin/lvglsim -b wayland
#
set -euo pipefail

# PetaLinux SDK in the shell leaks CONFIG_SITE / xilinx-* tools into autotools and
# makes libffi configure hang on slow cross-link tests. Use plain Debian cross GCC.
unset CONFIG_SITE
export CONFIG_SITE=
export A53_REAL_CC=aarch64-linux-gnu-gcc

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${LVGL_A53_BUILD_DIR:-$ROOT/build-a53-wayland-static}"
DEPS="$BUILD/deps"
TC="$ROOT/cmake/toolchains/aarch64-cortex-a53-linux-gnu.cmake"
TC_MESON="$ROOT/cmake/toolchains/aarch64-meson.cross"
CONFIG="${LVGL_A53_CONFIG:-lvgl2d3dbackend-a53-wayland-egl}"
JOBS="${JOBS:-$(nproc)}"
SRC="$BUILD/_src"

export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
export AR=aarch64-linux-gnu-ar
export RANLIB=aarch64-linux-gnu-ranlib
export CFLAGS="-mcpu=cortex-a53 -O2 -fno-lto --sysroot=/usr/aarch64-linux-gnu"
export CXXFLAGS="-mcpu=cortex-a53 -O2 -fno-lto --sysroot=/usr/aarch64-linux-gnu"
export LDFLAGS="-fno-lto --sysroot=/usr/aarch64-linux-gnu"
export PKG_CONFIG_PATH="$DEPS/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig"

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "error: install cross compiler: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu" >&2
    exit 1
fi
if ! command -v wayland-scanner >/dev/null 2>&1; then
    echo "error: install wayland-scanner: sudo apt install wayland-scanner libwayland-dev" >&2
    exit 1
fi
if ! command -v meson >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    echo "error: install meson + ninja: sudo apt install meson ninja-build" >&2
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

meson_cross_install() {
    local name="$1" srcdir="$2"
    shift 2
    local bdir="$BUILD/_build/$name"
    echo "==> meson build $name (static, aarch64)"
    meson setup "$bdir" "$srcdir" \
        --cross-file="$TC_MESON" \
        --prefix="$DEPS" \
        --libdir=lib \
        --default-library=static \
        "$@"
    meson compile -C "$bdir" -j"$JOBS"
    meson install -C "$bdir"
}

have_libpng() {
    [[ -f "$DEPS/lib/libpng16.a" || -f "$DEPS/lib/libpng.a" ]]
}

build_zlib() {
    [[ -f "$DEPS/lib/libz.a" ]] && return
    clone_once zlib https://github.com/madler/zlib.git v1.3.1
    echo "==> build zlib (static, aarch64, direct compile)"
    local odir="$BUILD/zlib-obj"
    rm -rf "$odir"
    mkdir -p "$odir" "$DEPS/lib" "$DEPS/include" "$DEPS/lib/pkgconfig"
    local srcs=(adler32.c compress.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
    local objs=()
    local c
    for c in "${srcs[@]}"; do
        $CC $CFLAGS -I"$SRC/zlib" -c "$SRC/zlib/$c" -o "$odir/${c%.c}.o"
        objs+=("$odir/${c%.c}.o")
    done
    $AR rcs "$DEPS/lib/libz.a" "${objs[@]}"
    $RANLIB "$DEPS/lib/libz.a"
    cp "$SRC/zlib/zlib.h" "$SRC/zlib/zconf.h" "$DEPS/include/"
    cat > "$DEPS/lib/pkgconfig/zlib.pc" <<EOF
prefix=$DEPS
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: zlib
Version: 1.3.1
Description: zlib compression library
Libs: -L\${libdir} -lz
Cflags: -I\${includedir}
EOF
}

build_libpng() {
    have_libpng && return
    build_zlib
    clone_once libpng https://github.com/pnggroup/libpng.git v1.6.43
    cp "$SRC/libpng/scripts/pnglibconf.h.prebuilt" "$SRC/libpng/pnglibconf.h"
    echo "==> build libpng (static, aarch64, direct compile)"
    local odir="$BUILD/libpng-obj"
    rm -rf "$odir"
    mkdir -p "$odir" "$DEPS/lib" "$DEPS/include" "$DEPS/lib/pkgconfig"
    local srcs=(png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c)
    local objs=()
    local c
    for c in "${srcs[@]}"; do
        $CC $CFLAGS -I"$SRC/libpng" -I"$DEPS/include" -c "$SRC/libpng/$c" -o "$odir/${c%.c}.o"
        objs+=("$odir/${c%.c}.o")
    done
    $AR rcs "$DEPS/lib/libpng16.a" "${objs[@]}"
    $RANLIB "$DEPS/lib/libpng16.a"
    cp "$SRC/libpng/png.h" "$SRC/libpng/pngconf.h" "$DEPS/include/"
    cp "$SRC/libpng/scripts/pnglibconf.h.prebuilt" "$DEPS/include/pnglibconf.h"
    cat > "$DEPS/lib/pkgconfig/libpng16.pc" <<EOF
prefix=$DEPS
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: libpng
Version: 1.6.43
Description: PNG compression library
Requires: zlib
Libs: -L\${libdir} -lpng16 -lz
Cflags: -I\${includedir}
EOF
}

fetch_tarball() {
    local name="$1" url="$2"
    local dir="$SRC/$name"
    if [[ ! -d "$dir" ]]; then
        echo "==> fetch $name"
        mkdir -p "$SRC"
        local tgz="$SRC/${name}.tar.gz"
        curl -fsSL "$url" -o "$tgz"
        tar -xzf "$tgz" -C "$SRC"
        if [[ ! -d "$dir" ]]; then
            local extracted
            extracted="$(find "$SRC" -maxdepth 1 -type d -name "${name}*" ! -path "$SRC" | head -1)"
            [[ -n "$extracted" ]] && mv "$extracted" "$dir"
        fi
    fi
}

build_libffi() {
    [[ -f "$DEPS/lib/libffi.a" ]] && return
    fetch_tarball libffi-3.4.6 https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz
    echo "==> build libffi (static, aarch64, autotools)"
    chmod +x "$ROOT/scripts/a53-autoconf-cc"
    rm -rf "$BUILD/libffi-build"
    mkdir -p "$BUILD/libffi-build"
    pushd "$BUILD/libffi-build" >/dev/null
    CC="$ROOT/scripts/a53-autoconf-cc" \
    "$SRC/libffi-3.4.6/configure" \
        --host=aarch64-linux-gnu --build=x86_64-linux-gnu \
        --prefix="$DEPS" --enable-static --disable-shared --disable-docs \
        --disable-builddir
    CC=aarch64-linux-gnu-gcc make -j"$JOBS"
    make install
    popd >/dev/null
}

build_libxml2() {
    [[ -f "$DEPS/lib/libxml2.a" ]] && return
    clone_once libxml2 https://gitlab.gnome.org/GNOME/libxml2.git v2.12.7
    meson_cross_install libxml2 "$SRC/libxml2" \
        -Dpython=disabled -Dzlib=false -Dlzma=false -Diconv=disabled
}

build_wayland() {
    [[ -f "$DEPS/lib/libwayland-client.a" ]] && return
    build_libffi
    build_libpng
    clone_once wayland https://gitlab.freedesktop.org/wayland/wayland.git 1.22.0
    meson_cross_install wayland "$SRC/wayland" \
        -Ddocumentation=false -Dtests=false -Ddtd_validation=false -Dscanner=true
}

build_xkbcommon() {
    [[ -f "$DEPS/lib/libxkbcommon.a" ]] && return
    build_libxml2
    build_wayland
    clone_once xkbcommon https://github.com/xkbcommon/libxkbcommon.git xkbcommon-1.6.0
    meson_cross_install xkbcommon "$SRC/xkbcommon" \
        -Dx11-support=false -Dwayland=enabled -Ddocs=false -Dtools=false -Dman-pages=false
}

build_all_deps() {
    build_zlib
    build_libpng
    build_libffi
    build_libxml2
    build_wayland
    build_xkbcommon
}

echo "==> AArch64 Cortex-A53 static Wayland build"
echo "    build dir : $BUILD"
echo "    deps dir  : $DEPS"
echo "    config    : $CONFIG"

build_all_deps

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
readelf -d "$OUT" 2>/dev/null | grep NEEDED || true
echo ""
echo "On device (needs Wayland compositor + EGL/GLES):"
echo "  LVGL_SCENARIO=4 WAYLAND_DISPLAY=wayland-0 ./lvglsim -b wayland"
