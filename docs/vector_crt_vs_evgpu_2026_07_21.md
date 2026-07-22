# VECTOR CRT vs EVGPU 像素对比（2026-07-21）

> **板端：** Orange Pi @ 192.168.10.140，Wayland 800×480  
> **Demo：** `lv_demo_vector_graphic_not_buffered()`（同图）  
> **参考：** EVGPU（EVGR）；**被测：** EVGPU_C_R_T（CPU tess → GLES2）

## 构建

| | EVGPU | CRT |
|--|-------|-----|
| Config | `configs/orangepi-evgpu-vector.defaults` | `configs/orangepi-evgpu-crt.defaults` |
| Build | `build-orangepi-vec-evgpu` | `build-orangepi-vec-crt` |
| VECTOR | EVGR `lv_draw_evgpu_vector` | `lv_draw_evgpu_c_r_t_vector` |
| Dump | `LVGL_GL_DUMP` | `LVGL_GL_DUMP` |

运行时：CRT 日志 `type=14` → `EVGPU_C_R_T`；EVGPU 日志 `OTHER`（VECTOR 枚举无独立名字）+ `evgrBeginFrame`。

## 指标（CRT vs EVGPU，exact / MAE）

| 区域 | exact% | MAE | max L1 |
|------|--------|-----|--------|
| **全屏** | **86.59** | **11.65** | 765 |
| red_tri | 94.71 | 4.57 | 599 |
| dash | 97.21 | 2.10 | 599 |
| stroke_grad | 84.15 | 9.36 | 500 |
| pattern | 79.83 | 26.64 | 705 |
| fill_grad | 66.23 | 20.75 | 482 |
| center_mid | 56.43 | 23.51 | 718 |

Delta 分桶（每像素 max 通道差）：eq / 1–5 / 6–20 / 21–50 / 51–100 / 101+  
→ `332513 / 1079 / 3413 / 6914 / 9277 / 30804`

## 与 CRT vs SW 对照

| 对比 | 全屏 exact% | 全屏 MAE | fill_grad MAE | stroke_grad MAE |
|------|-------------|----------|---------------|-----------------|
| CRT vs **SW** (P3.2) | 63.55 | 11.41 | **42.57** | 21.80 |
| CRT vs **EVGPU**（本次） | **86.59** | 11.65 | **20.75** | **9.36** |

全屏比 SW 对拍更近（exact 高约 23pt），主要因 solid 几何 / dash / pattern 大块接近。

**渐变抽样（重要）：**

| ROI | EVGPU | CRT |
|-----|-------|-----|
| fill 区主色 | 近实色橄榄 `(128,128,0)` + 蓝块 | 红→橙→绿 **线性渐变** 色带 |
| stroke 区 | 近实色紫 `(128,64,128)` | 红→蓝 **线性渐变** |

EVGPU 侧 fill/stroke 渐变在本 demo 更像「两 stop 均值实色」，CRT 反而更接近 demo 意图；ROI MAE 被背景像素稀释，**不能**解读成「CRT 渐变已对齐 EVGPU」。差还来自 AA / tess 边界、pattern 边缘（见 absdiff）。

## 产物

`benchmark_logs/vector_crt_vs_evgpu_2026_07_21/`：`vec_evgpu.{ppm,png}`、`vec_crt.{ppm,png}`、`vec_absdiff_x3.{ppm,png}`
