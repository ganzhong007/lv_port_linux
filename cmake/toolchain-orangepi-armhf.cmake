#
# Cross-compile toolchain for Orange Pi (Allwinner H3, armv7l / armhf)
#
# Toolchain : Bootlin armv7-eabihf--glibc--stable-2020.08-1 (gcc 9.3.0, glibc 2.31)
#             -> exactly matches board (Ubuntu 20.04, gcc 9.3, glibc 2.31)
# Sysroot   : pulled from the board (EGL / GLESv2 / wayland / xkbcommon / glibc)
#
# Usage:
#   cmake -S . -B build-cross-armhf \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-orangepi-armhf.cmake \
#         -DCONFIG=orangepi-evgpu
#
# Override paths via -DOPI_TOOLCHAIN_ROOT / -DOPI_SYSROOT if needed.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED OPI_TOOLCHAIN_ROOT)
    set(OPI_TOOLCHAIN_ROOT "$ENV{HOME}/xtools/armv7-eabihf--glibc--stable-2020.08-1")
endif()
if(NOT DEFINED OPI_SYSROOT)
    set(OPI_SYSROOT "$ENV{HOME}/xtools/opi-sysroot")
endif()

set(_tc_prefix "${OPI_TOOLCHAIN_ROOT}/bin/arm-buildroot-linux-gnueabihf-")

set(CMAKE_C_COMPILER   "${_tc_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_tc_prefix}g++")
set(CMAKE_AR           "${_tc_prefix}ar"      CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${_tc_prefix}ranlib"  CACHE FILEPATH "" FORCE)
set(CMAKE_STRIP        "${_tc_prefix}strip"   CACHE FILEPATH "" FORCE)

# Use the board sysroot as the link/include root (glibc 2.31 + EGL/wayland dev).
set(CMAKE_SYSROOT "${OPI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${OPI_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must resolve .pc files from the board sysroot and rewrite paths.
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR}
    "${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${OPI_SYSROOT}/usr/lib/pkgconfig:${OPI_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${OPI_SYSROOT}")

# Help the linker resolve indirect (NEEDED) deps inside the sysroot, and
# statically pull gcc9.3 libstdc++/libgcc so the binary never depends on the
# board's exact C++ runtime version.
set(_opi_link
    "-L${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf -L${OPI_SYSROOT}/lib/arm-linux-gnueabihf \
-Wl,-rpath-link,${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf \
-Wl,-rpath-link,${OPI_SYSROOT}/lib/arm-linux-gnueabihf \
-static-libstdc++ -static-libgcc")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${_opi_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_opi_link}")

# Cortex-A7 (Allwinner H3) with NEON/VFPv4.
# -B points gcc at the Debian multiarch dir so it finds crt1.o/crti.o and the
# libc.so linker script (Bootlin's buildroot triple doesn't search it by default).
# -isystem adds the Debian multiarch include dir (bits/, sys/, gnu/ arch headers).
set(_opi_march "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -B${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf -isystem ${OPI_SYSROOT}/usr/include/arm-linux-gnueabihf")
set(CMAKE_C_FLAGS_INIT   "${_opi_march}")
set(CMAKE_CXX_FLAGS_INIT "${_opi_march}")
