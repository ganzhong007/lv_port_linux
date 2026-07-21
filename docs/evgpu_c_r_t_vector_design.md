# EVGPU_C_R_T VECTOR 设计（建议路线 · 已采纳）

> **状态：** 方案 A 已采纳；**P0 已实现**（2026-07-21）  
> **范围：** `LV_DRAW_TASK_TYPE_VECTOR` 在 DrawUnit **EVGPU_C_R_T** 上的认领与绘制  
> **关联：** C_R_T 目录 `lvgl/src/draw/evgpu_c_r_t/`；板端配置 `configs/orangepi-evgpu-crt.defaults`

---

## 1. 动机与约束

| 点 | 说明 |
|----|------|
| 缺口 | C_R_T 原先对 `VECTOR` `evaluate → 0`，仅有声明、无实现 |
| 硬约束 | C_R_T **无 EVGR**，不能复用 `lv_draw_evgpu_vector.c` 的 `path → evgrFill/Stroke` |
| 目标 | 在 **不链回 EVGR** 的前提下，让 C_R_T 独立认领并绘制 VECTOR |
| 硬件 | Orange Pi / Mali-400 / GLES2；优先可控、可调试，而非像素级对齐 EVGPU |

**原则：** 不要为了 VECTOR 再依赖 EVGR——否则失去与 EVGPU 解耦的意义。

---

## 2. 备选方案（决策记录）

### 方案 A（采纳 · 推荐起步）：CPU 三角化 → 现有 GLES2 batch

把 LVGL path 展平为折线 → 填充三角化（自写 ear-clip / 日后可换 libtess2）→ stroke 用粗线段三角带 → 喂给已有 `solid` /（后续）`grad` / `tex` shader。

| 优点 | 缺点 |
|------|------|
| 不引入 EVGR；复用 C_R_T 投影 / scissor / blend | 复杂 path、曲线多时 CPU 重 |
| Mali-400 上可控、易调试 | AA 需自做（边扩 fringe；无 MSAA） |
| 与「手写 GLES2」风格一致 | dash / miter 需单独做 |

### 方案 B：嵌 NanoVG-GL2 子集

path → `nvgFill/Stroke`，stencil + AA。质量好，但体积与状态易和 C_R_T batch 打架，和「去 EVGR 解耦」目标冲突。**不采纳为默认。**

### 方案 C：ThorVG SW → 纹理 blit

语义完整、实现快，但是 CPU 矢量 + 上传带宽，不当性能路径。可用于对拍金图，**不作主路径。**

---

## 3. 建议路线（已落地骨架）

```
认领 VECTOR（pref 70）
    ↓
P0: path flatten + ear-clip fill + stroke strip → solid shader   ← 已实现
    ↓
P1: grad / pattern → 现有 lv_evgpu_c_r_t_grad / image tex
    ↓
P2+: dash / 复杂 join / fringe AA；超复杂 path 可 fallback SW
```

与 EVGPU 分工：

| | **EVGPU** | **EVGPU_C_R_T** |
|--|-----------|-----------------|
| 矢量核心 | EVGR 原生 path/paint | CPU 三角化 + GLES2 batch |
| preference | 80 | **70**（同时开时 VECTOR 优先 C_R_T） |
| 金图预期 | 与 C_R_T **不必像素一致** | 用 `REF_IMG_TOLERANCE` 或独立 `ref_imgs_*` |

---

## 4. 前置条件与配置

1. `evaluate` 认领 `LV_DRAW_TASK_TYPE_VECTOR`（与其它 2D 同档 pref **70**）。
2. `lv_draw_vector.c` 的 `#error` 允许 `LV_USE_DRAW_EVGPU_C_R_T` 作为 VECTOR 后端（**不要求** ThorVG）。
3. 板端：`LV_USE_VECTOR_GRAPHIC 1`，`LV_USE_THORVG_INTERNAL 0`（见 `orangepi-evgpu-crt.defaults`）。
4. 仍需 `LV_USE_DRAW_EVGPU_C_R_T` + `LV_USE_OPENGLES`；矩阵相关已有 `LV_USE_MATRIX` / `LV_USE_FLOAT`。

入口形态与其它后端一致：

```c
lv_vector_for_each_destroy_tasks(dsc->task_list, task_draw_cb, &ctx);
```

每个 path：`MOVE/LINE/QUAD/CUBIC/CLOSE` + fill/stroke dsc（solid / gradient / pattern）。

---

## 5. P0 实现规格（当前代码）

**文件：** `lvgl/src/draw/evgpu_c_r_t/lv_draw_evgpu_c_r_t_vector.c`

| 步骤 | 行为 |
|------|------|
| Flatten | QUAD/CUBIC 按 `path.quality` 细分（LOW 4 / MED 8 / HIGH 16）后变折线；上限约 2048 点 |
| Fill | ear-clip 三角化 → `lv_evgpu_c_r_t_gl` solid 三角 |
| Stroke | 每段按线宽生成矩形条（butt）；闭合 path 连回起点；**忽略** dash / stroke gradient |
| 非 solid fill | P0：gradient 用 **首 stop 色** 近似；pattern 等同 solid 色 |
| Clip | 使用 task / dsc 的 scissor，走现有 GL scissor |

**显式不做（P0）：** even-odd 完整 fill rule、自交稳定 tess、miter/round join、高质量 AA、真实 linear/radial/pattern。

---

## 6. 后续分期

| 阶段 | 内容 |
|------|------|
| **P1** | linear/radial → 复用 `lv_evgpu_c_r_t_grad`；pattern → image tex |
| **P2** | dash；round/miter join；可选 fringe AA |
| **P3** | 复杂/自交 path：换 libtess2 或 fallback SW；板端压测与金图目录 |

---

## 7. 验证建议

| 方式 | 说明 |
|------|------|
| Demo | `lv_demo_vector_graphic()`（CRT build 已编入 demos） |
| 本仓 | 自写 path 用例；`LVGL_GL_DUMP` 像素抽检 |
| LVGL Unity | 日后仿 VG-Lite：独立 `OPTIONS_TEST_EVGPU_C_R_T` + `ref_imgs_evgpu_c_r_t/`（见测试生态讨论） |

构建：`CONFIG=orangepi-evgpu-crt` → `build-orangepi-g3-crt`。

---

## 8. 变更清单（实现落点）

| 位置 | 变更 |
|------|------|
| `lv_draw_evgpu_c_r_t.c` | `evaluate` / `dispatch` 认领并调用 VECTOR |
| `lv_draw_evgpu_c_r_t_vector.c` | **新增** P0 实现 |
| `lv_draw_evgpu_c_r_t_private.h` | 已有 `lv_draw_evgpu_c_r_t_vector` 声明 |
| `lv_draw_vector.c` | `#error` 允许 C_R_T |
| `lv_conf_template.h` | C_R_T 注释标明 VECTOR P0 |
| `configs/orangepi-evgpu-crt.defaults` | `LV_USE_VECTOR_GRAPHIC 1` |
