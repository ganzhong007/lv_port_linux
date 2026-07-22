# VECTOR SW (ThorVG) vs EVGPU_C_R_T P0 — 对比结果

> **日期：** 2026-07-21  
> **板端：** Orange Pi PC @ 192.168.10.140，Wayland 800×480  
> **Demo：** `lv_demo_vector_graphic_not_buffered()`（`LVGL_APP_DEMO=vector_graphic`）

## 构建

| | SW | CRT |
|--|----|-----|
| Config | `configs/orangepi-sw-vector.defaults` | `configs/orangepi-evgpu-crt.defaults` |
| Build | `build-orangepi-vec-sw` | `build-orangepi-vec-crt` |
| VECTOR 后端 | SW + `LV_USE_THORVG_INTERNAL` | C_R_T P0（无 ThorVG） |
| Dump | `LVGL_BUF_DUMP` → snapshot PPM | `LVGL_GL_DUMP` → `glReadPixels` PPM |

## 运行时确认

- CRT：`execute task type=14`（VECTOR）走 `EVGPU_C_R_T`
- SW：每帧 `FILL` + `OTHER`（日志名；VECTOR 枚举值同为 14）

## 像素对比（同分辨率 800×480）

| 区域 | exact% | MAE | max L1 | 说明 |
|------|--------|-----|--------|------|
| **全屏** | 63.84% | 13.89 | 764 | 背景大面积接近 |
| 红三角区 (20–120) | 31.47% | 14.54 | 510 | solid 几何有差（AA/三角化） |
| pattern 区 | 43.18% | 37.73 | 764 | CRT P0 **无真 pattern**（solid 近似） |
| **grad 区** | **1.59%** | **83.99** | 510 | CRT P0 用**首 stop 色**，与 ThorVG 渐变差最大 |
| center_mid | 36.65% | 32.13 | 509 | 混合路径 |

Delta 分桶（按每像素最大通道差）：

| eq | 1–5 | 6–20 | 21–50 | 51–100 | 101+ |
|----|-----|------|-------|--------|------|
| 245161 | 79887 | 1249 | 5964 | 13209 | 38530 |

## 视觉结论（对照 PNG）

| 能力 | SW (ThorVG) | CRT P0 | 差因 |
|------|-------------|--------|------|
| Solid 三角 / 矩形 / 椭圆 | 有 | 有（几何大致在） | 三角化 / AA |
| 描边（绿线、扇形描边） | 真 dash / 细描边 | 粗实线条带近似 | P0 忽略 dash |
| 线性 / 径向渐变 | 完整 | 首 stop 实色 | P0 近似 |
| Pattern / 图片填充 | 完整（头像 clip） | 无真 pattern | P0 近似 |
| 半透明叠色粉多边形 | 有 | 缺 / 弱 | fill 规则 / 未实现复杂 path |

**一句话：** 通路已打通；观感差主要来自 P0 故意不做的 grad/pattern/dash，而不是「没认领 VECTOR」。
