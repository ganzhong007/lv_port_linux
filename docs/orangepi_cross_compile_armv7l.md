# Orange Pi(armv7l / armhf)交叉编译环境

> 目的:在本机 **WSL x86_64** 上交叉编译出 **armv7l 32 位** 的 `lvglsim`,直接部署到 **Orange Pi PC**
> (Allwinner H3,4× Cortex-A7,Mali-400/lima,1GB RAM,Ubuntu 20.04)运行。
>
> 动机:板子上原生编译慢、内存紧张;本机交叉编译**全量 ~15s、增量 ~2s**,配合"插桩→推板→截图"
> 可快速定位显示/GPU 问题(黑屏排查即依赖此环境,见 `docs/orangepi_evgpu_black_screen_fix.md`)。

---

## 1. 组成总览

| 组件 | 路径 | 说明 |
|---|---|---|
| 交叉工具链 | `~/xtools/armv7-eabihf--glibc--stable-2020.08-1` | Bootlin,gcc 9.3.0 / glibc 2.31 |
| 目标 sysroot | `~/xtools/opi-sysroot`(~2.1G) | 从板子 rsync 的 EGL/GLESv2/wayland/xkbcommon/glibc |
| CMake 工具链文件 | `cmake/toolchain-orangepi-armhf.cmake` | 编译器、sysroot、march、pkg-config、链接标志 |
| 板子精简配置 | `configs/orangepi-evgpu.defaults` | EVGPU + wayland-egl,GLES2,关掉重型模块 |
| Wayland 协议兼容文件 | `~/xtools/wl-proto/wayland_xdg_shell.{h,c}` | 用板子 `wayland-scanner 1.18` 生成,规避 API 不兼容 |
| 构建输出 | `build-cross-armhf/bin/lvglsim` | ELF 32-bit ARM,部署为板子 `/home/orangepi/lvglsim_cross` |

**版本对齐是关键**:工具链选 gcc 9.3 / glibc 2.31,与板子 Ubuntu 20.04 完全一致,避免运行期 `GLIBCXX/GLIBC` 版本报错;C++ 运行时再静态链接兜底(见下)。

---

## 2. 一次性搭建步骤

### 2.1 下载并解压 Bootlin 工具链
```bash
mkdir -p ~/xtools && cd ~/xtools
# https://toolchains.bootlin.com/  ->  armv7-eabihf / glibc / stable 2020.08-1
wget https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/armv7-eabihf--glibc--stable-2020.08-1.tar.bz2
tar xf armv7-eabihf--glibc--stable-2020.08-1.tar.bz2
# 交叉前缀:  arm-buildroot-linux-gnueabihf-gcc / -g++ ...
```

### 2.2 从板子拉取 sysroot
只拉目标运行/开发所需目录(EGL/GLESv2/wayland/xkbcommon/glibc + 头文件 + pkgconfig):
```bash
mkdir -p ~/xtools/opi-sysroot
OPI=orangepi@192.168.10.140
rsync -aL --info=progress2 \
  $OPI:/usr/include \
  ~/xtools/opi-sysroot/usr/
rsync -aL $OPI:/usr/lib/arm-linux-gnueabihf ~/xtools/opi-sysroot/usr/lib/
rsync -aL $OPI:/lib/arm-linux-gnueabihf     ~/xtools/opi-sysroot/lib/
# 结构:opi-sysroot/{lib,usr}   usr/lib/arm-linux-gnueabihf/{libEGL.so,libGLESv2.so,libwayland-*.so,pkgconfig/...}
```
> 板子是 merged-`/usr`(`/lib` → `/usr/lib` 软链)、Debian 多架构布局(`arm-linux-gnueabihf`),
> 工具链文件针对这两点做了适配(见 §3 的 `-B` / `-isystem` / pkg-config)。

### 2.3 Wayland 协议 C 代码(1.18 兼容,关键坑)
LVGL 的 `lvgl/env_support/cmake/dependencies/wayland.cmake` 会在 **CMake 配置阶段**用
**宿主机的 `wayland-scanner`** 生成 `xdg_shell` 的 C 代码,且**仅当输出文件不存在时才生成**
(`if(NOT EXISTS wayland_xdg_shell.{h,c})`)。

- 宿主机 `wayland-scanner` 1.24 生成的代码使用 `wl_proxy_marshal_flags` / `WL_MARSHAL_FLAG_DESTROY`
  等 **libwayland ≥1.20** 的 API,而板子是 **1.18** → 交叉链接/运行失败。
- 解决:在板子上用其自带 `wayland-scanner 1.18.0` 生成兼容代码,拷回宿主机,并**预置**到构建目录,
  让 CMake 跳过自动生成。

板子上生成(注意 1.18 需要**普通可 seek 文件**,不能用管道):
```bash
# 板子端
cp /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml ~/xdg.xml
wayland-scanner client-header    ./xdg.xml ./xdg.h     # -> wayland_xdg_shell.h
wayland-scanner private-code     ./xdg.xml ./xdg.c     # -> wayland_xdg_shell.c
```
拷回并备份到 `~/xtools/wl-proto/wayland_xdg_shell.{h,c}`,构建前放入:
```bash
mkdir -p build-cross-armhf/lvgl/wayland-protocols
cp ~/xtools/wl-proto/wayland_xdg_shell.{h,c} build-cross-armhf/lvgl/wayland-protocols/
```

---

## 3. CMake 工具链文件(`cmake/toolchain-orangepi-armhf.cmake`)

要点逐条:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 工具链 & sysroot(可用 -DOPI_TOOLCHAIN_ROOT / -DOPI_SYSROOT 覆盖)
set(OPI_TOOLCHAIN_ROOT "$ENV{HOME}/xtools/armv7-eabihf--glibc--stable-2020.08-1")
set(OPI_SYSROOT        "$ENV{HOME}/xtools/opi-sysroot")
set(_tc_prefix "${OPI_TOOLCHAIN_ROOT}/bin/arm-buildroot-linux-gnueabihf-")
set(CMAKE_C_COMPILER   "${_tc_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_tc_prefix}g++")

# 用板子 sysroot 作为查找根,库/头/包只在 sysroot 找,程序用宿主
set(CMAKE_SYSROOT "${OPI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${OPI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config 从 sysroot 解析 .pc 并重写前缀(找 wayland/egl/xkbcommon 等)
set(ENV{PKG_CONFIG_LIBDIR}
    "${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${OPI_SYSROOT}/usr/lib/pkgconfig:${OPI_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${OPI_SYSROOT}")

# 链接:补 sysroot 库路径 + rpath-link 解析间接 NEEDED;静态拉 gcc9.3 的 libstdc++/libgcc
set(_opi_link
  "-L${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf -L${OPI_SYSROOT}/lib/arm-linux-gnueabihf \
   -Wl,-rpath-link,${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf \
   -Wl,-rpath-link,${OPI_SYSROOT}/lib/arm-linux-gnueabihf \
   -static-libstdc++ -static-libgcc")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_opi_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_opi_link}")

# Cortex-A7 + NEON/VFPv4;-B 让 gcc 找到多架构目录里的 crt1.o/crti.o 与 libc.so 链接脚本,
# -isystem 加入多架构头目录(bits/ sys/ gnu/ 架构相关头)
set(_opi_march "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
  -B${OPI_SYSROOT}/usr/lib/arm-linux-gnueabihf \
  -isystem ${OPI_SYSROOT}/usr/include/arm-linux-gnueabihf")
set(CMAKE_C_FLAGS_INIT   "${_opi_march}")
set(CMAKE_CXX_FLAGS_INIT "${_opi_march}")
```

设计意图:
- **`-static-libstdc++ -static-libgcc`**:二进制不再依赖板子精确的 C++ 运行时版本,换板/换镜像更稳。
- **`-B .../arm-linux-gnueabihf`**:Bootlin 的 buildroot triple 默认不搜 Debian 多架构目录,
  否则报 `cannot find crt1.o/crti.o`。
- **`-isystem .../include/arm-linux-gnueabihf`**:否则报 `bits/libc-header-start.h: No such file`。
- **`PKG_CONFIG_SYSROOT_DIR`**:让 `.pc` 里的 `-I/usr/...`、`-L/usr/...` 自动加上 sysroot 前缀。

---

## 4. 板子精简配置(`configs/orangepi-evgpu.defaults`)

为 1GB / Cortex-A7 / Mali-400(仅 GLES2)裁剪。关键项:
```
LV_COLOR_DEPTH 32
LV_DEF_REFR_PERIOD 16                # 弱 SoC 不要 REFR_PERIOD=1 顶满 CPU

LV_USE_WAYLAND 1 / LV_WAYLAND_USE_EGL 1
LV_USE_OPENGLES 1 / LV_USE_DRAW_EVGPU 1
LV_USE_DRAW_NANOVG 0 / LV_USE_DRAW_OPENGLES 0 / LV_USE_NANOVG 0
LV_EVGR_BACKEND = LV_EVGR_BACKEND_GLES2   # evgr 用 #version 100,兼容 Mali-400

# 3dscene 所需 3D 部件
LV_USE_3DTEXTURE / LV_USE_3D_DRAW_TASKS / LV_USE_3DVIEWPORT / LV_USE_3DMESH = 1
LV_USE_3DLIGHT 1     # 必须开:lv_style_3d.c 无条件引用 lv_3dlight
LV_USE_GLTF 0

LV_USE_EVDEV 0       # wayland 客户端输入由合成器给,不需 evdev(否则 CMake 报缺 libevdev)
LV_BUILD_DEMOS 1 / LV_USE_DEMO_3DSCENE 1 / (其余重型 demo 关)

# 关掉大件 C++ 编译
LV_USE_VECTOR_GRAPHIC 0 / LV_USE_THORVG_INTERNAL 0 / LV_USE_LOTTIE 0

# profiler -> systrace(Perfetto),日志 WARN 保持 stdout 干净
LV_USE_PROFILER 1 / LV_USE_PROFILER_BUILTIN(_POSIX) 1
LV_PROFILER_INCLUDE "lv_profiler_builtin.h"
LV_LOG_LEVEL LV_LOG_LEVEL_WARN
```
> 踩坑记录:`LV_USE_3DLIGHT` 必须为 1(`lv_style_3d.c` 无条件引用 `lv_3dlight_class`);
> `LV_USE_EVDEV` 置 0 以免 CMake 因缺 `libevdev` 失败(wayland 客户端不需要)。

---

## 5. 构建 & 部署

### 5.1 配置(首次)
```bash
cd /home/gz/opencodeprj/lv_port_linux

# 预置 1.18 兼容的 wayland 协议代码(见 §2.3),避免 CMake 用宿主 scanner 生成
mkdir -p build-cross-armhf/lvgl/wayland-protocols
cp ~/xtools/wl-proto/wayland_xdg_shell.{h,c} build-cross-armhf/lvgl/wayland-protocols/

cmake -S . -B build-cross-armhf \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-orangepi-armhf.cmake \
  -DCONFIG=orangepi-evgpu \
  -DLVGL_APP_DEMO=3dscene          # 可换 simple_button / benchmark / 3dviewport ...
```

### 5.2 编译
```bash
cmake --build build-cross-armhf -j"$(nproc)"     # 全量 ~15s / 增量 ~2s
file build-cross-armhf/bin/lvglsim
#  -> ELF 32-bit LSB executable, ARM, EABI5, dynamically linked, interpreter /lib/ld-linux-armhf.so.3
```

### 5.3 部署 & 运行(板子 weston 以 root 跑,socket 在 /run/user/0)
```bash
scp build-cross-armhf/bin/lvglsim orangepi@192.168.10.140:/home/orangepi/lvglsim_cross

ssh orangepi@192.168.10.140 \
  "sudo XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0 \
   /home/orangepi/lvglsim_cross -b wayland -W 800 -H 480"   # 加 -f 全屏
```

---

## 6. 踩坑清单(按出现顺序)

| 报错 / 现象 | 原因 | 处理 |
|---|---|---|
| `CMakeTestCCompiler` 失败,`cannot find crt1.o/crti.o` | buildroot triple 不搜多架构目录 | 工具链加 `-B${SYSROOT}/usr/lib/arm-linux-gnueabihf` |
| `bits/libc-header-start.h: No such file` | 多架构头目录未加入搜索 | 加 `-isystem ${SYSROOT}/usr/include/arm-linux-gnueabihf` |
| `WL_MARSHAL_FLAG_DESTROY undeclared` / `wl_proxy_marshal_flags` | 宿主 scanner 1.24 生成新 API,板子 libwayland 1.18 | 用板子 scanner 1.18 生成协议代码并预置(§2.3) |
| `wayland-scanner 1.18` 输出空 / `Failed to reset fd` | 1.18 需普通可 seek 文件,不支持管道 | 用普通文件参数:`wayland-scanner client-header ./xdg.xml ./xdg.h` |
| CMake `libevdev not found` | wayland 客户端误启 evdev | 配置 `LV_USE_EVDEV 0` |
| `lv_3dlight_class undeclared` | `lv_style_3d.c` 无条件引用 | 配置 `LV_USE_3DLIGHT 1` |
| 运行黑屏 | (另见专文)MSAA config + alpha=0 | 见 `docs/orangepi_evgpu_black_screen_fix.md` |

---

## 7. 相关文件

- `cmake/toolchain-orangepi-armhf.cmake` — 交叉工具链定义
- `configs/orangepi-evgpu.defaults` — 板子精简 LVGL 配置
- `~/xtools/armv7-eabihf--glibc--stable-2020.08-1/` — Bootlin 工具链(本机,未入库)
- `~/xtools/opi-sysroot/` — 板子 sysroot(本机,未入库)
- `~/xtools/wl-proto/wayland_xdg_shell.{h,c}` — 1.18 兼容协议代码备份(本机,未入库)
- `build-cross-armhf/` — 交叉构建目录(本机,未入库)

> 说明:工具链、sysroot、构建目录体积大,不入 git;仓库内仅保留 `toolchain-*.cmake` 与 `*.defaults`
> 两份可移植文件,重建 sysroot 按 §2 步骤即可。
