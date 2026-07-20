# ATK 板（A53）simple_button：拉代码 / 编译 / 上传运行

本文记录在本机环境中，从 GitLab 拉取 `wsl_wayland_3d` 分支，交叉编译 A53 版 `lvglsim`（`simple_button`），并上传到正点原子 ATK-MPSoC 板运行的完整步骤。

## 环境与约定

| 项 | 值 |
| --- | --- |
| 主机工程目录 | `/home/gz/cursorprj/LV_PORT_LINUX_ZILI` |
| 远程仓库（主仓） | `ssh://git@gitlab-sw.evensemi.com:2222/gzhuang/lvgl_port_linux_wsl.git` |
| 分支 | `wsl_wayland_3d` |
| lvgl 子模块（代码仓） | `ssh://git@gitlab-sw.evensemi.com:2222/gzhuang/lvgl.git` |
| lvgl 上游（可选） | `git@github.com:ganzhong007/lvgl.git` |
| Demo | `simple_button`（最简单按钮示例） |
| 交叉工具链 | Petalinux 2020.2（`/opt/petalinux/2020.2`） |
| 构建目录 | `build-a53weston/` |
| 产物 | `build-a53weston/bin/lvglsim`（aarch64） |
| 板子 IP | `192.168.137.42` |
| 板子账号 | `root` / `123` |
| 板端路径 | `/root/lvglsim` |
| 板端显示 | Weston + Wayland（`wayland-0`） |

---

## 1. 拉代码

```bash
cd /home/gz/cursorprj/LV_PORT_LINUX_ZILI

# 远程已指向目标仓库时，只拉 wsl_wayland_3d
git fetch origin wsl_wayland_3d
git checkout wsl_wayland_3d
git pull origin wsl_wayland_3d

# 同步 lvgl 子模块（.gitmodules 指向 GitLab 代码仓 gzhuang/lvgl）
git submodule sync --recursive
git submodule update --init --recursive
```

确认子模块 URL 正确：

```bash
git config -f .gitmodules --get submodule.lvgl.url
# 期望：ssh://git@gitlab-sw.evensemi.com:2222/gzhuang/lvgl.git
```

若本地仍残留旧的 GitHub SSH 别名地址，强制改回 GitLab 后再初始化：

```bash
git config -f .gitmodules submodule.lvgl.url \
  ssh://git@gitlab-sw.evensemi.com:2222/gzhuang/lvgl.git
git submodule sync --recursive
git submodule update --init --recursive
```

（可选）在已检出的 `lvgl/` 目录补上游 remote，便于对照 / 同步 GitHub：

```bash
cd lvgl
git remote add upstream git@github.com:ganzhong007/lvgl.git 2>/dev/null || \
  git remote set-url upstream git@github.com:ganzhong007/lvgl.git
git remote -v
# origin   → GitLab 代码仓
# upstream → GitHub
cd ..
```

确认状态：

```bash
git branch -vv
git submodule status
# 期望：主仓在 wsl_wayland_3d，lvgl 检出到对应 commit
```

全新克隆示例：

```bash
git clone -b wsl_wayland_3d \
  ssh://git@gitlab-sw.evensemi.com:2222/gzhuang/lvgl_port_linux_wsl.git \
  LV_PORT_LINUX_ZILI
cd LV_PORT_LINUX_ZILI
git submodule update --init --recursive
```

---

## 2. 编译（A53 + simple_button）

依赖：

- Petalinux SDK：`/opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux`
- 本仓已提供：
  - `user_cross_compile_a53.cmake`
  - `configs/a53weston.defaults`
  - `scripts/build_a53weston_cross.sh`
  - `scripts/wayland-scanner`（兼容板端 wayland-client 1.17）

一键编译：

```bash
cd /home/gz/cursorprj/LV_PORT_LINUX_ZILI
DEMO=simple_button ./scripts/build_a53weston_cross.sh
```

脚本等价于：

```bash
source /opt/petalinux/2020.2/environment-setup-aarch64-xilinx-linux
export PATH="$(pwd)/scripts:$PATH"

cmake -B build-a53weston -S . \
  -DCONFIG=a53weston \
  -DLVGL_APP_DEMO=simple_button \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_a53.cmake

cmake --build build-a53weston -j"$(nproc)" --target lvglsim
```

验证产物：

```bash
file build-a53weston/bin/lvglsim
# 期望：ELF 64-bit LSB ..., ARM aarch64, ...
```

说明：

- `-DLVGL_APP_DEMO=simple_button`：入口走 `src/simple_button.c`（居中按钮，点击后文字变为 `Clicked!`）
- `-DCONFIG=a53weston`：Wayland SHM 后端，面向板端 Weston

---

## 3. 上传到板子

板子需已开机，主机能 ping 通 `192.168.137.42`，且板端 Weston 已运行。

### 方式 A：scp（需 sshpass 或交互输入密码）

```bash
scp build-a53weston/bin/lvglsim root@192.168.137.42:/root/lvglsim
ssh root@192.168.137.42 'chmod +x /root/lvglsim'
```

### 方式 B：Python + paramiko（本机曾用此方式）

```bash
python3 << 'PY'
import paramiko, os
HOST, USER, PASS = "192.168.137.42", "root", "123"
LOCAL = "build-a53weston/bin/lvglsim"
REMOTE = "/root/lvglsim"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PASS, timeout=15,
          allow_agent=False, look_for_keys=False)
sftp = c.open_sftp()
sftp.put(LOCAL, REMOTE)
sftp.chmod(REMOTE, 0o755)
sftp.close()
c.close()
print("upload ok")
PY
```

---

## 4. 在板端运行

SSH 登录后执行（或远程一条命令）：

```bash
ssh root@192.168.137.42

killall lvglsim 2>/dev/null
export XDG_RUNTIME_DIR=/run/user/0
export WAYLAND_DISPLAY=wayland-0
cd /root
./lvglsim -b wayland -W 800 -H 480
```

后台运行并写日志：

```bash
killall lvglsim 2>/dev/null
export XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0
nohup /root/lvglsim -b wayland -W 800 -H 480 > /tmp/lvglsim.log 2>&1 &
ps | grep '[l]vglsim'
tail -f /tmp/lvglsim.log
```

成功时日志中可见类似：

```text
[LVGL:L6-DRIVER] Wayland SHM flush ...
[LVGL:L3-DRAW] add task LABEL ...
```

屏上应出现居中蓝色按钮，文字 `Hello LVGL`；点击后变为 `Clicked!`。

---

## 5. 常用排障

| 现象 | 处理 |
| --- | --- |
| 子模块 clone 失败 | 确认 `.gitmodules` 为 GitLab URL；`git submodule sync` 后重试；检查对本机 GitLab（`:2222`）的 SSH 权限 |
| 仍指向 `github.com-ganzhong` 等旧别名 | 按第 1 节强制改回 GitLab URL 并 `submodule sync` |
| wayland-scanner / 协议头与 1.17 不兼容 | 确保 `scripts` 在 PATH 最前，使用本仓 `wayland-scanner` 包装脚本 |
| Failed to initialize display backend | 确认 Weston 在跑：`ps \| grep weston`；设置 `XDG_RUNTIME_DIR=/run/user/0`、`WAYLAND_DISPLAY=wayland-0` |
| 找不到 `wayland-0` | `ls /run/user/0/wayland-*`，按实际 socket 名设置 `WAYLAND_DISPLAY` |
| 无显示 / 黑屏 | 确认用 `-b wayland`；分辨率可用 `-W`/`-H` 调整 |

停止程序：

```bash
killall lvglsim
```

---

## 6. 一键回顾命令

```bash
# 拉代码
cd /home/gz/cursorprj/LV_PORT_LINUX_ZILI
git pull origin wsl_wayland_3d
git submodule sync --recursive
git submodule update --init --recursive

# 编译
DEMO=simple_button ./scripts/build_a53weston_cross.sh

# 上传 + 运行（paramiko 示例可拆成独立脚本）
# scp build-a53weston/bin/lvglsim root@192.168.137.42:/root/lvglsim
# ssh root@192.168.137.42 'XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0 /root/lvglsim -b wayland -W 800 -H 480'
```
