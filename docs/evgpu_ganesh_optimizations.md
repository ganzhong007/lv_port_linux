# EVGPU Ganesh-风格渲染优化分析

## 目标
在不破坏 EVGPU 原有架构的前提下，对特定 DrawTask 采用 Skia Ganesh 的直接 GL 渲染方式替换 evgr 路径渲染，以降低开销。

## 对比分析矩阵

| DrawTask | EVGPU 原始实现 | Skia Ganesh 实现 | 结论 |
|----------|---------------|-----------------|------|
| **FILL** | `lv_evgpu_solid_fill_rect` 直接 GL quad | 直接 GL quad | 等价，保留 |
| **BORDER** | `evgrBeginPath` + `evgrRoundedRect` outer + `evgrPathWinding` subtract + `evgrFill` (路径布尔运算) | 对直边+无圆角：4 次 `setBlendMode + drawQuad` edge quads；对圆角：路径填充 | **改为 Ganesh 风格** |
| **LINE** | `evgrBeginPath` + 多个 `evgrLineTo` + `evgrStroke` (路径解析 + stroking) | `GL_TRIANGLE_STRIP` 4 顶点法线偏移 + 圆端 cap 用 `drawQuad` | **改为 Ganesh 风格** |
| **LABEL / LETTER** | `evgrImagePattern` + `evgrBeginPath` + `evgrRect` + `evgrFill` (路径蒙版纹理填充) | 直接纹理 quad 用 `u_recolor_opa=1.0` tint A8→RGB | **改为 Ganesh 风格** |
| **IMAGE** | `evgrCreateImage` + `evgrImagePattern` + `evgrBeginPath` + `evgrRect` + `evgrFill` | `glTexImage2D` + `glGenerateMipmap` | **补充 mipmap** |
| **ARC** | 路径 arc 扇形 | Ganesh 也用 tessellated path | 等价，保留 EVGPU |
| **BLUR** | shader 双 pass | Ganesh 也用 shader | 等价，保留 EVGPU |
| **BOX_SHADOW** | evgr 路径 | Ganesh 也用 path | 等价，保留 EVGPU |
| **VECTOR / TRIANGLE** | evgr path/tri | — | EVGPU 领先 |
| **GRADIENT** | evgr 3-stop / 纹理 | Ganesh 也纹理 | 等价，保留 EVGPU |
| **TGA / BORDER_IMAGE / ROCKET** | N/A 或未实现 | — | 保留 |

## 修改文件清单

### 1. `src/draw/evgpu/lv_draw_evgpu_border.c`
- **新增** `border_native_gl()`：对 `LV_BORDER_SIDE_FULL` + `radius == 0` 情况
  - 绘制 4 条 edge quad（top/bottom/left/right），每条 4 顶点通过 `lv_evgpu_solid_fill_rect` 渲染
  - 直接调用 GL solid shader，无需 evgr 路径
- **重命名原实现**为 `border_evgr_path()` 作为 fallback（partial-side / 圆角时使用）
- 在 `border_draw_cb` 中根据条件选择 fast path 或 fallback

### 2. `src/draw/evgpu/lv_draw_evgpu_line.c`
- **重写** line draw task
- 计算线的 4 个顶点（沿法线方向偏移线宽的一半）
- 使用 `GL_TRIANGLE_STRIP` 通过 solid shader 直接绘制
- 对圆端 cap：在两端用 `lv_evgpu_solid_fill_rect` 绘制圆形 cap（传入 radius+center）
- 保留虚线支持：对每个 dash segment 独立调用上述逻辑
- 消除 evgr path/stroke 的所有调用

### 3. `src/draw/evgpu/lv_draw_evgpu_label.c`
- 在 `draw_letter_bitmap` 中
  - 构建临时的 `lv_draw_image_dsc_t`，设 `recolor = glyph_color`、`recolor_opa = LV_OPA_COVER`
  - 对 A8 bitmap 调用 `lv_evgpu_tex_draw_image()`（直接 GL 纹理 quad）
  - 利用 tex shader 的 `u_recolor_opa = 1.0`，将 `GL_ALPHA` 纹理中 (0,0,0,A) → 覆盖 tint color
  - 消除 evgr `ImagePattern` + path rect + fill 的所有调用

### 4. `src/draw/evgpu/lv_evgpu_image_cache.c`
- 在 `evgrCreateImage` 绑完纹理后，对 `w > 64 || h > 64` 的纹理：
  - `glGenerateMipmap(GL_TEXTURE_2D)` 生成 mipmap 链
  - 设置 `GL_LINEAR_MIPMAP_LINEAR` 缩小过滤器
  - 改善大图缩放时的锯齿问题

## 未修改（保留 EVGPU 原始实现）
- ARC：EVGPU 的 evgr arc path 实现已足够高效
- BLUR：shader 双 pass，无需改变
- BOX_SHADOW：路径填充实现，与 Ganesh 无本质区别
- VECTOR / TRIANGLE：EVGPU 的 evgr 路径领先于 Ganesh
- GRADIENT：3-stop 硬件线性渐变优于 Ganesh 纹理
- FILL：原有直接 GL quad 已是最优
