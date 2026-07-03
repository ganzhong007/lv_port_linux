# PetaLinux 2020.2 cross toolchain (aarch64-xilinx-linux, Cortex-A53/A72)
#
# Prerequisite (build script does this automatically):
#   source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
#
# Usage:
#   cmake -B build-petalinux-fbdev \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/petalinux-aarch64-xilinx.cmake \
#     -DCONFIG=petalinux-a53-fbdev ...

if(NOT DEFINED ENV{SDKTARGETSYSROOT})
    message(FATAL_ERROR
        "SDKTARGETSYSROOT not set. Source PetaLinux environment first:\n"
        "  source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(PLNX_SYSROOT "$ENV{SDKTARGETSYSROOT}")
set(PLNX_PREFIX "$ENV{OECORE_NATIVE_SYSROOT}/usr/bin/aarch64-xilinx-linux")

set(CMAKE_SYSROOT "${PLNX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${PLNX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_COMPILER   "${PLNX_PREFIX}/aarch64-xilinx-linux-gcc")
set(CMAKE_CXX_COMPILER "${PLNX_PREFIX}/aarch64-xilinx-linux-g++")
set(CMAKE_AR           "${PLNX_PREFIX}/aarch64-xilinx-linux-ar")
set(CMAKE_RANLIB       "${PLNX_PREFIX}/aarch64-xilinx-linux-ranlib")
set(CMAKE_STRIP        "${PLNX_PREFIX}/aarch64-xilinx-linux-strip")
set(CMAKE_LINKER       "${PLNX_PREFIX}/aarch64-xilinx-linux-ld")

# Match environment-setup flags (A53/A72 ZynqMP)
set(PLNX_CPU_FLAGS "-march=armv8-a+crc -mtune=cortex-a72.cortex-a53 -O2 -pipe")
set(CMAKE_C_FLAGS_INIT "${PLNX_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${PLNX_CPU_FLAGS}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

if(DEFINED ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}")
endif()
if(DEFINED ENV{PKG_CONFIG_SYSROOT_DIR})
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "$ENV{PKG_CONFIG_SYSROOT_DIR}")
endif()
