# ATK-MPSoC (A53) Wayland-EGL 构建与运行指南

本文说明如何在一个**新的项目目录**中拉取主仓与子仓，交叉编译出可在 ATK 板 Weston 上运行的 **Wayland-EGL** 程序。

## 仓库地址

| 仓库 | HTTPS | SSH |
|------|-------|-----|
| 主仓 | `https://github.com/ganzhong007/lv_port_linux.git` | `git@github.com:ganzhong007/lv_port_linux.git` |
| 子仓 lvgl | `https://github.com/ganzhong007/lvgl.git` | `git@github.com:ganzhong007/lvgl.git` |

分支：`wsl_wayland`

## 1. 新建目录并拉取主仓 + 子仓

### HTTPS（推荐新环境）

```bash
mkdir -p ~/cursorprj && cd ~/cursorprj

git clone -b wsl_wayland --recurse-submodules \
  https://github.com/ganzhong007/lv_port_linux.git \
  lv_port_debian_wayland

cd lv_port_debian_wayland
```

若克隆时子仓未拉下（`.gitmodules` 中可能仍是 SSH 别名），执行：

```bash
git config -f .gitmodules submodule.lvgl.url \
  https://github.com/ganzhong007/lvgl.git
git submodule sync
git submodule update --init --recursive
```

### SSH

```bash
mkdir -p ~/cursorprj && cd ~/cursorprj

git clone -b wsl_wayland --recurse-submodules \
  git@github.com:ganzhong007/lv_port_linux.git \
  lv_port_debian_wayland

cd lv_port_debian_wayland
```

### 确认

```bash
git branch --show-current          # 应为 wsl_wayland
git -C lvgl rev-parse --short HEAD # 子仓已检出
ls lvgl/src                        # 应有源码
```

## 2. 编译前准备（宿主机）

需要 **Petalinux 2020.2 SDK**，默认路径：

```text
/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
```

若 SDK 安装在其他位置，请修改：

- `user_cross_compile_a53.cmake`
- `scripts/build_a53weston_egl_cross.sh`

中的 `PETALINUX_ROOT` / `SDK_ENV` 路径。

宿主机还需：`cmake`、`python3`、常规构建工具（`make`、`gcc` 等）。

## 3. 交叉编译 A53 Wayland-EGL

在仓库根目录执行：

```bash
./scripts/build_a53weston_egl_cross.sh
```

脚本会：

1. `source` Petalinux SDK 环境
2. 使用配置 `configs/a53weston-egl.defaults`（Wayland + EGL + NanoVG）
3. 使用 `user_cross_compile_a53.cmake` 做 aarch64 交叉编译

产物路径：

```text
build-a53weston-egl/bin/lvglsim
```

验证架构：

```bash
file build-a53weston-egl/bin/lvglsim
# ELF 64-bit LSB executable, ARM aarch64, ...
```

### 等价手动命令

```bash
source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
export PATH="$PWD/scripts:$PATH"
export WAYLAND_SCANNER="$PWD/scripts/wayland-scanner-compat17.sh"

cmake -B build-a53weston-egl -S . \
  -DCONFIG=a53weston-egl \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_a53.cmake \
  -DWAYLAND_SCANNER="$PWD/scripts/wayland-scanner-compat17.sh"

cmake --build build-a53weston-egl -j"$(nproc)"
```

## 4. 部署到 ATK 板并运行

### 板子信息

- **IP**：`192.168.137.190`
- **用户**：`root` / 密码 `123`
- **程序目录**：`/home/root/wayland_run/`

### 部署

```bash
scp build-a53weston-egl/bin/lvglsim \
  root@192.168.137.190:/home/root/wayland_run/lvglsim-egl
```

### 运行（EGL / Mali）

板子上需已有 Weston（socket：`/run/user/0/wayland-0`）：

```bash
ssh root@192.168.137.190

export XDG_RUNTIME_DIR=/run/user/0
export WAYLAND_DISPLAY=wayland-0
export LD_LIBRARY_PATH=/usr/lib/wayland${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

cd /home/root/wayland_run
./lvglsim-egl -m -b wayland
```

后台运行：

```bash
nohup ./lvglsim-egl -m -b wayland >/tmp/lvglsim-egl.log 2>&1 &
```

停止：

```bash
killall lvglsim lvglsim-egl
```

### 运行（SHM 版，可选）

```bash
export XDG_RUNTIME_DIR=/run/user/0
export WAYLAND_DISPLAY=wayland-0
cd /home/root/wayland_run
./lvglsim -m -b wayland
```

SHM 版本由 `./scripts/build_a53weston_cross.sh` 编译，产物为 `build-a53weston/bin/lvglsim`。

## 5. 关键文件对照

| 用途 | 路径 |
|------|------|
| EGL 配置 | `configs/a53weston-egl.defaults` |
| 交叉工具链 | `user_cross_compile_a53.cmake` |
| EGL 编译脚本 | `scripts/build_a53weston_egl_cross.sh` |
| SHM 编译脚本 | `scripts/build_a53weston_cross.sh` |
| 本地产物（EGL） | `build-a53weston-egl/bin/lvglsim` |
| 板端程序（EGL） | `/home/root/wayland_run/lvglsim-egl` |
| 板端程序（SHM） | `/home/root/wayland_run/lvglsim` |
| glmark2 对比 | `boards/glmark2-atk-vs-orangepi.md` |

## 6. GPU 说明

ATK 使用 ARM 闭源 Mali 用户态库：

- `/usr/lib/wayland/libMali.so.9.0`
- `libEGL.so` / `libGLESv2.so` 指向该库
- 内核模块：`mali.ko`，设备节点：`/dev/mali`

运行 EGL 版时必须设置 `LD_LIBRARY_PATH=/usr/lib/wayland`，否则可能找不到正确的 EGL/GLES 实现。
