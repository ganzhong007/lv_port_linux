# LVGL on Orange Pi (armv7l) with Weston on Ubuntu 20.04

## Board

- **Platform**: Orange Pi, `armv7l` (armhf)
- **OS**: Ubuntu 20.04 Focal
- **SSH**: `orangepi@192.168.10.140` (password `Even-123`, sudo available)
- **Display (preferred, ATK-style)**: native Weston **`drm-backend`** on `/dev/dri/card0` (`sun4i-drm` / HDMI); GPU via Mesa **lima** (`card1` / `renderD128`)
- **Display (legacy)**: XFCE + Xorg (lightdm) with Weston nested as `x11-backend`

## Build (native on board)

Host PC has no armhf cross toolchain by default; this target is built **natively on the Orange Pi**:

```bash
./scripts/orangepi-weston-build-deploy.sh
```

Installs deps, copies sources, builds `build-orangepi/bin/lvglsim`, deploys to `/home/orangepi/wayland_run/`.

Config: `configs/orangepi-weston.defaults` (Wayland SHM, 32-bit color).

## Run: native DRM Weston (recommended, like ATK)

```bash
./scripts/orangepi-weston-drm-run.sh
```

Stops LightDM/Xorg, starts Weston with `drm-backend.so` (fullscreen on HDMI), then launches `lvglsim`.

**Boot (configured on board):** `weston-drm.service` is `enabled` under `graphical.target`; LightDM/`display-manager` alias removed so Xorg will not start. Reboot should enter native Weston. Socket: `/run/user/0/wayland-0`.

Restore XFCE later:

```bash
./scripts/orangepi-weston-restore-x11.sh
```

## Run: nested X11 Weston (legacy)

```bash
./scripts/orangepi-weston-run.sh
```

Keeps XFCE; Weston is only an X11 window (`x11-backend`).
## Cross compile from host (optional)

Install on Ubuntu host:

```bash
sudo apt install crossbuild-essential-armhf libwayland-dev:armhf libxkbcommon-dev:armhf
```

Then:

```bash
cmake -B build-orangepi -S . \
  -DCONFIG=orangepi-weston \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_armhf.cmake
cmake --build build-orangepi -j$(nproc)
```

Use `scripts/wayland-scanner-compat17.sh` if linking against wayland-client 1.17/1.18.
