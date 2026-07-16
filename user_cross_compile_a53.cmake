# Cross-compile for Alientek ATK-MPSoC (ZynqMP A53) using Petalinux SDK.
#
# Usage:
#   source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
#   cmake -B build-a53weston -S . \
#     -DCONFIG=a53weston \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake
#   cmake --build build-a53weston -j$(nproc)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(PETALINUX_ROOT "/opt/petalinux/2020.2" CACHE PATH "Petalinux SDK root")
set(SYSROOT "${PETALINUX_ROOT}/sysroots/aarch64-xilinx-linux")

set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")

set(TOOLCHAIN_PREFIX "${PETALINUX_ROOT}/sysroots/x86_64-petalinux-linux/usr/bin/aarch64-xilinx-linux/aarch64-xilinx-linux")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}-g++")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}-ar")
set(CMAKE_RANLIB "${TOOLCHAIN_PREFIX}-ranlib")
set(CMAKE_STRIP "${TOOLCHAIN_PREFIX}-strip")

set(CMAKE_C_FLAGS_INIT "-march=armv8-a+crc -mtune=cortex-a72.cortex-a53 --sysroot=${SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a+crc -mtune=cortex-a72.cortex-a53 --sysroot=${SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${SYSROOT}")
set(ENV{PKG_CONFIG_PATH} "${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig")
set(ENV{SDKTARGETSYSROOT} "${SYSROOT}")
