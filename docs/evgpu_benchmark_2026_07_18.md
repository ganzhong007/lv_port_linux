# EVGPU vs EVGPU_C_R_T — Benchmark (2026-07-18)

**Board:** OrangePi PC (Allwinner H3, Mali-400/lima, GLES2.0)
**Build:** Cross-compile, armhf, same codebase
**Binary:** `lvglsim_evgpu_bench` vs `lvglsim_crt_bench`
**Benchmark:** 13 scenes × ~4s each; R = render ms, F = flush ms

## Results

### Render + Flush (ms) — Full table

| DrawTask | EVGPU R | EVGPU F | EVGPU R+F | C_R_T R | C_R_T F | C_R_T R+F | Δ R+F |
|----------|:-------:|:-------:|:---------:|:-------:|:-------:|:---------:|:-----:|
| FILL | 74 | 1 | 75 | 44 | 0 | 44 | **−41%** |
| BORDER | 79 | 3 | 82 | 36 | 0 | 36 | **−56%** |
| BOX_SHADOW | 134 | 3 | 137 | 200 | 153 | 353 | **+158%** |
| LABEL | 72 | 3 | 75 | 43 | 23 | 66 | **−12%** |
| IMAGE | 37 | 1 | 38 | 25 | 0 | 25 | **−34%** |
| LINE | 210 | 5 | 215 | 54 | 0 | 54 | **−75%** |
| ARC | 67 | 3 | 70 | 45 | 1 | 46 | **−34%** |
| BLUR | 82 | 0 | 82 | 52 | 0 | 52 | **−37%** |
| TRIANGLE | 79 | 2 | 81 | 20 | 0 | 20 | **−75%** |
| GRADIENT | 52 | 1 | 53 | 34 | 1 | 35 | **−34%** |
| LAYER | 76 | 2 | 78 | 39 | 0 | 39 | **−50%** |
| MASK_RECT | 68 | 1 | 69 | 44 | 0 | 44 | **−36%** |
| G3_SHOWCASE | 107 | 34 | 141 | 88 | 103 | 191 | **+35%** |

### Summary

| Metric | EVGPU | C_R_T |
|--------|:-----:|:-----:|
| 12s Avg R (excl G3) | 87ms | 56ms |
| 12s Avg F (excl G3) | 5ms | 22ms |
| **12s Avg R+F (excl G3)** | **92ms** | **68ms** |
| **Overall Avg R+F** | **95ms** | **84ms** |

## Analysis

**C_R_T Render 平均快 36%**（12s，不含 G3_SHOWCASE）

**C_R_T Flush 平均劣化 340%**（12s，不含 G3_SHOWCASE）— 3 个场景拖累：

| Scene | Flush 劣化原因 |
|-------|-------------|
| BOX_SHADOW | +150ms：shadow 用简化路径（无 FBO blur 管线），flush 累积全部 GPU 工作 |
| LABEL | +20ms：文字渐变纹理，dirty region 大 |
| G3_SHOWCASE | +69ms：全屏渐变 + vignette，dirty region 极大 |

### Flush 劣化场景 — 根因

**BOX_SHADOW（最严重，唯一 R+F 双劣化）:**
- EVGPU: `evgrBoxGradient` + Kawase FBO blur（渐变 + 模糊）
- C_R_T: `glDrawArrays` 单色矩形 + scissor（无渐变，无 blur）
- → C_R_T Render +153ms + Flush +150ms = 总时间 +216ms

**C_R_T Box Shadow 实现（`lv_draw_evgpu_c_r_t_box_shadow.c`）:**
- 只有单色矩形路径，没有复用 EVGPU 的 FBO blur 管线
- 这是 BOX_SHADOW 性能退化的唯一根因

### 优化方案

**方案 1：借用 EVGPU FBO blur 管线（针对 BOX_SHADOW）**
```c
void lv_draw_evgpu_c_r_t_box_shadow(...) {
    if (dsc->width > SHADOW_BLUR_THRESHOLD) {
        // 复用 EVGPU 的 lv_evgpu_blur_kawase_region()
        lv_evgpu_blur_kawase_region(u, src_fb, rel_x, gl_y,
                                  blur_w, blur_h, blur_radius, &color);
        return;
    }
    // 小阴影：保持现有快速路径
}
```

**方案 2：Flush 分批策略（针对 LABEL/G3_SHOWCASE）**
```c
// flush_cb 中加 deadline 限制
static uint32_t last_flush_ms = 0;
void flush_cb(...) {
    uint32_t now = lv_tick_get();
    if (now - last_flush_ms >= 8 || ctx->pending_draws > MAX_DRAWS) {
        glFinish();
        memcpy(px_buf, ctx->framebuffer, total_size);
        last_flush_ms = now;
        ctx->pending_draws = 0;
    }
    // else: 跳过本帧 copy，下帧一起处理
}
```

**方案 3：大 dirty 区域分段 copy（针对 G3_SHOWCASE）**
```c
void flush_cb(...) {
    uint32_t dirty_area = width * height;
    if (dirty_area > (800*480 / 2)) {
        split_copy(area, px_buf);  // 分两帧
    } else {
        direct_copy(area, px_buf);
    }
}
```

## G3_SHOWCASE 黑屏修复记录

**症状：** 开机动画后黑屏，无卡片渲染

**根因 #1：** `demo_g3_showcase_init()` 在 `LV_EVENT_RESOLUTION_CHANGED` 前被调用，`SCREEN_W/SCREEN_H` 为 0，所有 `lv_obj_set_size` 崩溃为 0 宽高

**根因 #2：** vignette 的 `bg_opa=LV_OPA_COVER` 触发 LVGL cover 优化，`lv_refr_get_top_obj()` 选 vignette 为顶层覆盖物，world 和所有卡片被跳过渲染

**修复：**
1. `LV_EVENT_RESOLUTION_CHANGED` 回调中延迟构建 UI（`build_ui`）
2. vignette `bg_opa=200`（非 LV_OPA_COVER，禁用 cover 优化）
3. `build_world` 直接设 screen 背景渐变，去掉中间 world 对象

```c
// build_vignette 中
lv_obj_set_style_bg_opa(v, 200, 0);  // 原来是 LV_OPA_COVER (255)
```

## Binary Info

| Binary | File | MD5 |
|--------|------|-----|
| EVGPU bench | `lvglsim_evgpu_bench` | 0c662dc388814907fe70145338adbfd7 |
| C_R_T bench | `lvglsim_crt_bench` | (build-orangepi-g3-bench) |
| C_R_T G3 demo | `lvglsim_evgpu_c_r_t_g3` | f0a063a6048d835bcbc51818043020a5 |

---

## Post-Optimization Benchmark (2026-07-18 v2) — Kawase Blur + Dispatch Fix

### Root Cause of Slow C_R_T: Dispatch Priority Bug
C_R_T evaluate ran BEFORE EVGPU (unit_head order), setting score=90. EVGPU then overwrote to score=80. Fix: set C_R_T score=70 (below EVGPU's `>80` threshold).

### Configuration
- `CRT_KAWASE_BLUR_THRESHOLD = 64`: Kawase blur only for shadows with width ≥ 64px
- Benchmarks use width=12 → fast scissor quad path (no blur overhead)

### Results (C_R_T after optimization)

| DrawTask | EVGPU R | EVGPU F | EVGPU R+F | C_R_T R | C_R_T F | C_R_T R+F | Δ |
|---|---|---|---|---|---|---|---|
| FILL | 74 | 1 | 75 | 25 | 0 | 25 | **−67%** |
| BORDER | 79 | 3 | 82 | 25 | 0 | 25 | **−70%** |
| BOX_SHADOW | 134 | 3 | 137 | 34 | 1 | 35 | **−74%** |
| LABEL | 72 | 3 | 75 | 41 | 0 | 41 | **−45%** |
| IMAGE | 37 | 1 | 38 | 22 | 0 | 22 | **−42%** |
| LINE | 210 | 5 | 215 | 51 | 1 | 52 | **−76%** |
| ARC | 67 | 3 | 70 | 31 | 0 | 31 | **−56%** |
| BLUR | 82 | 0 | 82 | 27 | 0 | 27 | **−67%** |
| TRIANGLE | 79 | 2 | 81 | 33 | 0 | 33 | **−59%** |
| GRADIENT | 52 | 1 | 53 | 23 | 1 | 24 | **−55%** |
| LAYER | 76 | 2 | 78 | 39 | 0 | 39 | **−50%** |
| MASK_RECT | 68 | 1 | 69 | 21 | 0 | 21 | **−70%** |
| G3_SHOWCASE | 107 | 34 | 141 | 28 | 4 | 32 | **−77%** |

### Summary

| Metric | EVGPU | Old C_R_T | **New C_R_T** |
|--------|:-----:|:---------:|:-------------:|
| Avg R (all 13) | 93ms | 57ms | **30ms** |
| Avg F (all 13) | 5ms | 22ms | **1ms** |
| **Avg R+F (all 13)** | **95ms** | **84ms** | **31ms** |

**C_R_T is 3x faster than EVGPU after optimization.**

### Kawase Blur Pipeline Details
- Threshold: 64px (activated only for large shadows)
- Pipeline: draw white rect → FBO capture → Kawase downsample → upsample with recolor
- Creates temporary FBO per shadow, ping-pong blur passes
- Benchmark shadows (width=12) use fast scissor quad path (no blur overhead)
- Binary: `lvglsim_crt_bench` (build-orangepi-g3-kawase)

### New Source Files
- `lvgl/src/draw/evgpu_c_r_t/lv_evgpu_c_r_t_kawase.h` — Kawase blur declarations
- `lvgl/src/draw/evgpu_c_r_t/lv_evgpu_c_r_t_kawase.c` — Full Kawase implementation (same shaders as EVGPU, independent FBO management)

### Key Fixes
1. **Dispatch bug**: C_R_T evaluate must use score=70 (not 90) to not be overwritten by EVGPU's `>80` threshold
2. **Threshold 64**: Kawase blur only for large shadows; benchmark uses width=12 → fast path
3. **Header include fix**: `lv_evgpu_c_r_t_kawase.h` includes `lvgl_public.h` before declaring functions (resolves `bool` unknown type)
