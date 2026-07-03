# Cross toolchain: AArch64 Linux (Cortex-A53, ARMv8-A)
# Usage:
#   cmake -B build-a53-static \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-cortex-a53-linux-gnu.cmake \
#     -DCONFIG=lvgl2d3dbackend-a53-glfw ...

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(A53_TRIPLE aarch64-linux-gnu)
set(CMAKE_C_COMPILER   ${A53_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${A53_TRIPLE}-g++)
set(CMAKE_AR           ${A53_TRIPLE}-ar)
set(CMAKE_RANLIB       ${A53_TRIPLE}-ranlib)
set(CMAKE_STRIP        ${A53_TRIPLE}-strip)

set(A53_SYSROOT /usr/${A53_TRIPLE})
set(A53_LIBDIR  /usr/lib/${A53_TRIPLE})

set(CMAKE_SYSROOT ${A53_SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${A53_SYSROOT} ${A53_LIBDIR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Skip slow cross link during CMake/meson feature tests (compile-only probes).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Cortex-A53; -fno-lto avoids slow cross ld with LTO plugin
set(A53_CPU_FLAGS "-mcpu=cortex-a53 -O2 -fno-lto")
set(CMAKE_C_FLAGS_INIT "${A53_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${A53_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fno-lto")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fno-lto")

# pkg-config for target libraries
set(ENV{PKG_CONFIG_PATH} "${A53_LIBDIR}/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${A53_LIBDIR}/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/")
