# EVGPU_C_R_T VECTOR 设计（方案 A · 已落地）

> **状态：** 方案 A 已采纳；**P0–P3.2 + pattern UV 修正已实现**（2026-07-21）  
> **范围：** `LV_DRAW_TASK_TYPE_VECTOR` 在 DrawUnit **EVGPU_C_R_T** 上的认领与绘制  
> **实现：** `lvgl/src/draw/evgpu_c_r_t/lv_draw_evgpu_c_r_t_vector.c`  
> **板端 CRT：** `configs/orangepi-evgpu-crt.defaults`  
> **板端 EVGPU 对拍：** `configs/orangepi-evgpu-vector.defaults`

---

## 1. 动机与约束

| 点 | 说明 |
|----|------|
| 缺口 | C_R_T 原先对 `VECTOR` `evaluate → 0`，仅有声明、无实现 |
| 硬约束 | C_R_T **无 EVGR**，不能复用 `lv_draw_evgpu_vector.c` 的 `path → evgrFill/Stroke` |
| 目标 | 在 **不链回 EVGR** 的前提下，让 C_R_T 独立认领并绘制 VECTOR |
| 硬件 | Orange Pi / Mali-400 / GLES2；优先可控、可调试，而非像素级对齐 EVGPU/SW |

**原则：** 不要为了 VECTOR 再依赖 EVGR——否则失去与 EVGPU 解耦的意义。

---

## 2. 备选方案（决策记录）

### 方案 A（采纳）：CPU 三角化 → GLES2 batch

path flatten → ear-clip / 扇形回退 → stroke 三角带 → `solid` / `grad` / `tex` shader。

| 优点 | 缺点 |
|------|------|
| 不引入 EVGR；复用 C_R_T scissor / blend | 复杂 path、曲线多时 CPU 重 |
| Mali-400 上可控、易调试 | AA 自做 fringe；无 MSAA |
| 与「手写 GLES2」风格一致 | even-odd / 带洞需另做 |

### 方案 B：嵌 NanoVG-GL2 — **不采纳**（状态易与 C_R_T batch 打架）

### 方案 C：ThorVG SW blit — **仅作对拍金图**，不作主路径

---

## 3. 分期路线（当前）

```
认领 VECTOR（pref 70；同时开 EVGPU 时 C_R_T 胜出：score 越低越优先）
    ↓
P0  flatten + ear-clip fill + stroke strip → solid          ✅
P1  fill linear/radial + image pattern（1D/2D tex）         ✅
P2  dash / caps / joins / fringe AA                         ✅
P3.1 stroke linear/radial gradient                          ✅
P3.2 tess：双向 ear-clip + 质心扇回退                         ✅
P3.3a pattern UV：OBJECT_BOUNDING_BOX 原点（对齐 EVGPU/SW）   ✅
P3.3b 可选：libtess2 / even-odd；Unity 独立金图矩阵
```

与 EVGPU 分工：

| | **EVGPU** | **EVGPU_C_R_T** |
|--|-----------|-----------------|
| 矢量核心 | EVGR 原生 path/paint | CPU 三角化 + GLES2 |
| preference | 80 | **70** |
| 金图 | 不必与 C_R_T 像素一致 | 自有 `ref_imgs_*` 或容差 |

---

## 4. 配置与入口

1. `evaluate` 认领 `LV_DRAW_TASK_TYPE_VECTOR`（pref **70**）。
2. `lv_draw_vector.c` 允许 `LV_USE_DRAW_EVGPU_C_R_T` 作 VECTOR 后端（**不要求** ThorVG）。
3. CRT：`LV_USE_VECTOR_GRAPHIC 1`，`LV_USE_THORVG_INTERNAL 0`，`LV_USE_DEMO_VECTOR_GRAPHIC 1`。
4. 需 `LV_USE_DRAW_EVGPU_C_R_T` + `LV_USE_OPENGLES` + `LV_USE_MATRIX` / `LV_USE_FLOAT`。

```c
lv_vector_for_each_destroy_tasks(dsc->task_list, task_draw_cb, &ctx);
```

交叉构建示例：

```bash
# CRT VECTOR demo
cmake -B build-orangepi-vec-crt -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-orangepi-armhf.cmake \
  -DCONFIG=orangepi-evgpu-crt -DLVGL_APP_DEMO=vector_graphic

# EVGPU-only VECTOR（对拍用）
cmake -B build-orangepi-vec-evgpu -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-orangepi-armhf.cmake \
  -DCONFIG=orangepi-evgpu-vector -DLVGL_APP_DEMO=vector_graphic
```

Dump：CRT/EVGPU 用 `LVGL_GL_DUMP=/tmp/out.ppm`；SW 用 `LVGL_BUF_DUMP`（需 snapshot）。

---

## 5. 实现规格

**主文件：** `lv_draw_evgpu_c_r_t_vector.c`  
**GL：** `lv_evgpu_c_r_t_gl_create_grad_tex_stops`、`lv_evgpu_c_r_t_gl_draw_triangles_tex`

### 5.0 公共

| 步骤 | 行为 |
|------|------|
| Flatten | QUAD/CUBIC 按 quality 细分（LOW 4 / **MED 12** / HIGH 16）；上限 ~2048 点；经 `path.matrix` 进世界坐标 |
| Clip | task / dsc scissor → GL scissor |

### 5.1 P0 — solid

| | 行为 |
|--|------|
| Fill | ear-clip → solid 三角 |
| Stroke | 线宽矩形条（初版 butt）；闭合连回起点 |

### 5.2 P1 — fill 渐变 / pattern

| | 行为 |
|--|------|
| Linear / radial fill | 多 stop → 256×1 ramp；顶点算 `t` → UV.x |
| Pattern | decode → GL tex；三角 + UV（见 5.6） |

### 5.3 P2 — 描边样式

| | 行为 |
|--|------|
| Dash | 沿折线 on/off；on 段描边（默认升 round cap） |
| Caps | butt / square / round |
| Joins | bevel / miter（超限→bevel） / round |
| Fringe | solid 描边：外扩 `width+1.25`、alpha×0.35，再画芯 |

### 5.4 P3.1 — stroke 渐变

| | 行为 |
|--|------|
| Linear / radial | 同 fill 1D ramp；描边顶点按世界坐标算 UV |
| 矩阵 | `stroke.matrix` × `path.matrix` 变换控制点 |
| Fringe | 渐变描边暂不做 fringe |

### 5.5 P3.2 — tess

| | 行为 |
|--|------|
| 主路径 | ear-clip，CCW/CW 各试；放宽凸耳阈值 |
| 回退 | 质心三角扇 |
| 未做 | libtess2、严格 even-odd、带洞 |

### 5.6 P3.3a — pattern UV（对齐 EVGPU/SW）

默认 `fill_units = OBJECT_BOUNDING_BOX`（`lv_zalloc` → 枚举 0）。

EVGPU：`evgrImagePattern(ox, oy, w, h, …)`，`ox/oy` = path **局部**包围盒左上。  
SW：`imx = path_matrix`；bbox 平移后再乘 `fill.matrix`。

CRT UV：

```
local = inv(path_matrix) · world
local -= path_local_bbox_min     # 仅 OBJECT_BOUNDING_BOX
img_pt = inv(fill_matrix) · local
u = img_pt.x / img_w
v = img_pt.y / img_h
```

纹理 wrap：`GL_CLAMP_TO_EDGE`。

---

## 6. Demo 与对拍 ROI

`lv_demo_vector_graphic`：

- 原有 solid / dash / pattern 头像 / fill 渐变等  
- **新增** `draw_stroke_gradient()`：~(500,380)→(760,430)，宽 14，红→蓝线性描边  

| ROI 名 | 建议范围 | 用途 |
|--------|----------|------|
| avatar | ~(200,200)–(320,310) | pattern 头像（勿用旧 x&lt;220 框） |
| fill_grad | ~(420,80)–(620,360) | fill 线性渐变 |
| stroke_grad | ~(490,360)–(770,450) | stroke 线性渐变 |
| dash | ~(80,40)–(280,200) | 绿虚线 |

**MAE：** ROI 内每像素 `(|ΔR|+|ΔG|+|ΔB|)/3` 再平均（0–255）。须配 exact% 与 ROI；全屏会被灰底稀释。

---

## 7. 板端结论摘要（2026-07-21）

同 demo 800×480，三方：SW(ThorVG) / CRT / EVGPU(EVGR)。

| 对比 | 全屏 exact% | 全屏 MAE | 备注 |
|------|-------------|----------|------|
| CRT vs SW（pattern 修后） | ~63.6 | **~9.8** | |
| EVGPU vs SW | ~63.0 | **~8.0** | pattern 强 |
| CRT vs EVGPU | **~88** | ~9.7 | solid/dash 极近 |

| 能力 | SW | CRT | EVGPU |
|------|----|-----|-------|
| solid / dash | ✓ | ✓ | ✓ |
| pattern 头像 | ✓ | ✓（UV 修后 ≈ EVGPU；avatar MAE CRT↔EV ~1.6） | ✓ |
| fill 线性渐变 | ✓ | ✓（更近 SW） | ✗ 近均值实色橄榄 |
| stroke 线性渐变 | ✗ ROI 无笔画 | ✓ 红→蓝 | ✗ 近均值实色紫 |

详细报告：

- `docs/vector_sw_vs_crt_*.md`（P0/P1/P2/P3.2）  
- `docs/vector_crt_vs_evgpu_2026_07_21.md`  
- `docs/vector_sw_crt_evgpu_2026_07_21.md`  
- `docs/vector_sw_crt_evgpu_patfix_2026_07_21.md`  

产物目录：`benchmark_logs/vector_*_2026_07_21/`

---

## 8. 后续（可选）

| 项 | 内容 |
|----|------|
| tess | 真 libtess2 / even-odd / 带洞 |
| EVGPU | 排查 fill/stroke 渐变退化为 stop 均值 |
| SW | 排查 stroke 线性渐变未落像素 |
| 测试 | Unity `OPTIONS_TEST_EVGPU_C_R_T` + `ref_imgs_evgpu_c_r_t/` |
| 性能 | pattern 纹理缓存（现每 path decode+upload） |

---

## 9. 变更落点

| 位置 | 内容 |
|------|------|
| `lv_draw_evgpu_c_r_t.c` | evaluate / dispatch VECTOR |
| `lv_draw_evgpu_c_r_t_vector.c` | P0–P3.2 + pattern UV |
| `lv_evgpu_c_r_t_gl.*` | grad tex / triangles tex |
| `lv_draw_vector.c` | `#error` 允许 C_R_T |
| `lv_demo_vector_graphic.c` | `draw_stroke_gradient` |
| `configs/orangepi-evgpu-crt.defaults` | VECTOR + demo |
| `configs/orangepi-evgpu-vector.defaults` | EVGPU-only VECTOR 对拍 |
| `src/main.c` | `LVGL_GL_DUMP` / `LVGL_BUF_DUMP` kick |
