# Cross-compile toolchain for ATK-MPSoC (Cortex-A53 / aarch64) with Petalinux 2020.2.
#
# Usage (see docs/atk_a53_simple_button.md / scripts/build_a53weston_cross.sh):
#   source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
#   cmake -B build-a53weston -S . \
#         -DCONFIG=a53weston \
#         -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_a53.cmake
#
# Override with -DA53_SDK_ROOT=... if Petalinux is installed elsewhere.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED A53_SDK_ROOT)
  if(DEFINED ENV{PETALINUX} AND EXISTS "$ENV{PETALINUX}/sysroots/aarch64-xilinx-linux")
    set(A53_SDK_ROOT "$ENV{PETALINUX}")
  else()
    set(A53_SDK_ROOT "/opt/petalinux/2020.2")
  endif()
endif()

if(DEFINED ENV{SDKTARGETSYSROOT} AND EXISTS "$ENV{SDKTARGETSYSROOT}")
  set(A53_SYSROOT "$ENV{SDKTARGETSYSROOT}")
else()
  set(A53_SYSROOT "${A53_SDK_ROOT}/sysroots/aarch64-xilinx-linux")
endif()

if(DEFINED ENV{OECORE_NATIVE_SYSROOT} AND EXISTS "$ENV{OECORE_NATIVE_SYSROOT}")
  set(A53_NATIVE_SYSROOT "$ENV{OECORE_NATIVE_SYSROOT}")
else()
  set(A53_NATIVE_SYSROOT "${A53_SDK_ROOT}/sysroots/x86_64-petalinux-linux")
endif()

set(_a53_tc_bin "${A53_NATIVE_SYSROOT}/usr/bin/aarch64-xilinx-linux")
set(CMAKE_C_COMPILER   "${_a53_tc_bin}/aarch64-xilinx-linux-gcc"   CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_a53_tc_bin}/aarch64-xilinx-linux-g++"   CACHE FILEPATH "" FORCE)
set(CMAKE_AR           "${_a53_tc_bin}/aarch64-xilinx-linux-ar"    CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${_a53_tc_bin}/aarch64-xilinx-linux-ranlib" CACHE FILEPATH "" FORCE)
set(CMAKE_STRIP        "${_a53_tc_bin}/aarch64-xilinx-linux-strip" CACHE FILEPATH "" FORCE)

if(NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR
    "A53 cross compiler not found: ${CMAKE_C_COMPILER}\n"
    "Install/source Petalinux 2020.2 SDK, or pass -DA53_SDK_ROOT=...")
endif()

set(CMAKE_SYSROOT "${A53_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${A53_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Match Petalinux environment-setup defaults (A53/A72 big.LITTLE tuning).
set(_a53_cpu "-march=armv8-a+crc -mtune=cortex-a72.cortex-a53")
set(CMAKE_C_FLAGS_INIT   "${_a53_cpu}")
set(CMAKE_CXX_FLAGS_INIT "${_a53_cpu}")

# pkg-config must resolve target .pc files (wayland 1.17, xkbcommon, ...).
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${A53_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${A53_SYSROOT}/usr/lib/pkgconfig:${A53_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH}
    "${A53_SYSROOT}/usr/lib/pkgconfig:${A53_SYSROOT}/usr/share/pkgconfig")

# Help the linker resolve NEEDED deps inside the sysroot.
# Petalinux 2020.2 sysroot ships shared libstdc++ only (no .a), so do not use
# -static-libstdc++ here — link against the board's matching shared runtime.
set(_a53_link
    "-Wl,-rpath-link,${A53_SYSROOT}/usr/lib -Wl,-rpath-link,${A53_SYSROOT}/lib")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_a53_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_a53_link}")

# Ensure protocol XML comes from the target sysroot (wayland.cmake).
set(ENV{SDKTARGETSYSROOT} "${A53_SYSROOT}")
