# Cross-compile for Orange Pi / armv7l (armhf) using a portable toolchain.
#
# Usage:
#   cmake -B build-orangepi -S . \
#     -DCONFIG=orangepi-weston \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_armhf.cmake
#   cmake --build build-orangepi -j$(nproc)

set(TOOLCHAIN_ROOT "/home/gz/opt/x-tools/armv8-rpi3-linux-gnueabihf")
set(TRIPLET "armv8-rpi3-linux-gnueabihf")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(SYSROOT "${TOOLCHAIN_ROOT}/${TRIPLET}/sysroot")

set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT}/bin/${TRIPLET}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/${TRIPLET}-g++")
set(CMAKE_AR "${TOOLCHAIN_ROOT}/bin/${TRIPLET}-ar")
set(CMAKE_RANLIB "${TOOLCHAIN_ROOT}/bin/${TRIPLET}-ranlib")
set(CMAKE_STRIP "${TOOLCHAIN_ROOT}/bin/${TRIPLET}-strip")

set(CMAKE_C_FLAGS_INIT "--sysroot=${SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${SYSROOT}")
set(ENV{PKG_CONFIG_PATH} "${SYSROOT}/lib/pkgconfig:${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig:${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/usr/lib/pkgconfig")
set(ENV{SYSROOT} "${SYSROOT}")
