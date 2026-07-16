# glmark2-es2-wayland：ATK vs Orange Pi

Date: 2026-07-14  
Tool: **glmark2 2021.02**（两边版本已对齐）  
Resolution: **800×600**  
Backend: Wayland（原生 Weston / drm）

| 项 | ATK-MPSoC (`192.168.137.190`) | Orange Pi (`192.168.10.140`) |
|----|------------------------------|------------------------------|
| SoC / GPU | ZynqMP · Mali-400 MP | H3 · Mali-400 |
| Driver | ARM 闭源 `libMali` (`/usr/lib/wayland`) | Mesa **lima** |
| GL_VENDOR | ARM | lima |
| GL_RENDERER | Mali-400 MP | Mali400 |
| GL_VERSION | OpenGL ES 2.0 `"67dc026"` | OpenGL ES 2.0 Mesa 21.2.6 |
| **总分** | **410** | **363** |

ATK 总分相对 Orange Pi：**+12.9%**。

## 分场景 FPS

| 场景 | ATK | Orange Pi | Δ (ATK−OPi) | 更高 |
|------|-----|-----------|-------------|------|
| build use-vbo=false | 376 | 328 | +48 | ATK |
| build use-vbo=true | 445 | 452 | −7 | OPi |
| texture nearest | 496 | 522 | −26 | OPi |
| texture linear | 471 | 513 | −42 | OPi |
| shading gouraud | 238 | 270 | −32 | OPi |
| shading phong | 197 | 153 | +44 | ATK |
| effect2d | 226 | 356 | −130 | OPi |
| conditionals | 556 | 350 | +206 | ATK |
| function | 552 | 350 | +202 | ATK |
| loop | 551 | 344 | +207 | ATK |

## 命令

ATK：

```bash
export XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0
export LD_LIBRARY_PATH=/usr/lib/wayland${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
glmark2-es2-wayland -s 800x600 \
  --benchmark build:use-vbo=false \
  --benchmark build:use-vbo=true \
  --benchmark texture:texture-filter=nearest \
  --benchmark texture:texture-filter=linear \
  --benchmark shading:shading=gouraud \
  --benchmark shading:shading=phong \
  --benchmark effect2d \
  --benchmark conditionals:fragment-steps=0:vertex-steps=0 \
  --benchmark function:fragment-steps=0:vertex-steps=0 \
  --benchmark loop:fragment-steps=0:vertex-steps=0:fragment-loop=false
```

Orange Pi：

```bash
export XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0
# lima 须已加载（开机 modules-load；勿 blacklist lima）
glmark2-es2-wayland -s 800x600 \
  --benchmark build:use-vbo=false \
  --benchmark build:use-vbo=true \
  --benchmark texture:texture-filter=nearest \
  --benchmark texture:texture-filter=linear \
  --benchmark shading:shading=gouraud \
  --benchmark shading:shading=phong \
  --benchmark effect2d \
  --benchmark conditionals:fragment-steps=0:vertex-steps=0 \
  --benchmark function:fragment-steps=0:vertex-steps=0 \
  --benchmark loop:fragment-steps=0:vertex-steps=0:fragment-loop=false
```

## 备注

- ATK 曾为 glmark2 **2017.07**；已用 Petalinux SDK 交叉编译 **2021.02**，并针对 Mali 闭源 EGL（`eglGetDisplay(NULL)` 崩溃）打补丁后部署。
- Orange Pi 用 Ubuntu 包 `glmark2-es2-wayland` 2021.02；须加载 `lima`，否则会落到 `llvmpipe`。
- 同为 Mali-400 类 GPU，驱动栈不同（闭源 vs lima），分数仅作参考。
