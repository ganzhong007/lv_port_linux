# lv_draw_opengles 架构分析

> 位置: `lvgl/src/draw/opengles/lv_draw_opengles.c` (799 行)

## 核心设计：SW 渲染 → 纹理缓存 → 合成

```
                               均匀着色器填色 (
                               radius=0, 无渐变)
         ┌─ glClear + scissor  ──────────────────────┐
         │                                            │
DrawTask ─┤                                            ├─ 最终 FBO
         │                                            │
         └─ 其他 (含 FILL 复杂情况) ────────────────────┘
               ↓
         1. 创建临时 CPU 层 (dest_layer)
         2. lv_draw_rect/label/...  →  SW 渲染到 CPU 缓冲区
         3. create_texture() → glTexImage2D 上传
         4. lv_opengles_render_texture() → 合成到 FBO
```

## 两种路径

| 条件 | 路径 | 开销 |
|------|------|------|
| `FILL + radius=0 + 无渐变` | 直写 `glClear()` + scissor | **最快**（1 draw call） |
| 其他所有（BORDER/LABEL/ARC/LINE/IMAGE...） | CPU-SW 渲染→纹理上传→纹理 quad | **最慢**（每帧 CPU render + memcpy + GPU upload） |

## 关键优化：纹理缓存

`draw_from_cached_texture()` 是核心：

```c
cache_data_t data_to_find;
data_to_find.draw_dsc = (lv_draw_dsc_base_t *)t->draw_dsc;
// memcmp(dsc) 做 cache key，坐标先归零再比较
lv_cache_entry_t * entry_cached = lv_cache_acquire_or_create(
    u->texture_cache, &data_to_find, u);
```

如果同一个 BORDER 画两次，第二次直接复用之前上传的纹理，**跳过 CPU 渲染和纹理上传**。

## 与 evgpuganesh 对比

| 维度 | `lv_draw_opengles` | `evgpuganesh` |
|------|-------------------|---------------|
| **绘制方式** | SW 渲染→纹理上传 | 直接 GL 绘制 |
| **Fill 最快路径** | `glClear` (1 指令) | `glDrawArrays` (4 顶点) |
| **纹理缓存** | ✅ 全类型 DSC 缓存 | ❌ 无 |
| **shader 管理** | 依赖公共 `lv_opengles_render_*` | 自管 4 个 program |
| **FBO** | 全局共享 1 个 FBO | 每层独立 FBO |
| **BORDER** | SW `lv_draw_rect` → 纹理 | 4×quad |
| **ARC** | SW `lv_draw_arc` → 纹理 | 72 段 tessellation |
| **LABEL** | SW `lv_draw_label` → 纹理 | 直接 A8 纹理 quad |
| **IMAGE** | SW `lv_draw_image` → 纹理 | 直接纹理 quad |

## 最大启发

1. **`glClear` 作为 solid fill** — Mali-400 上最快的填色方式。evgpuganesh 可直接借鉴替代 4 顶点 quad。
2. **纹理缓存机制** — 对静态可复用绘制（BORDER/ARC/LINE）可避免每帧重复 tessellation 和顶点上传。
