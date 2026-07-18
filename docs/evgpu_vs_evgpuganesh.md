# EVGPU vs EVGPUGANESH 代码差异

## 架构层面

| 维度 | EVGPU (evgr 路径) | EVGPUGANESH (直接 GL) |
|------|------------------|----------------------|
| GL 抽象层 | evgr 库（`evgrBeginPath`/`evgrFill`/`evgrStroke`） | 无中间层，直接 `glDrawArrays` |
| 路径渲染 | 由 evgr 库内部处理（GPU 路径填充） | CPU tessellate → 三角形/条带 |
| Shader | evgr 内部管理，不暴露 | 自管 4 个 GL program（solid/tex/blur/grad） |
| VBO | evgr 内部 | 1 个 STREAM_DRAW VBO，每帧重填 |
| FBO | `lv_evgpu_fbo_cache`（evgr 包装） | `lv_evgpuganesh_fbo`（glGenFramebuffers 直调） |
| 3D 任务 | 自己处理 | 透传回 EVGPU（evaluate 返回 0） |

## 每个 DrawTask 差异

| 任务 | EVGPU | EVGPUGANESH |
|------|-------|-------------|
| **FILL** | `evgrRect`+`evgrFill` 或 `lv_evgpu_solid_fill_rect` | `glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)` |
| **BORDER** | `evgrRoundedRect` outer + `evgrPathWinding` subtract + `evgrFill` | 4 条 edge quad，各 4 顶点 `draw_quad_solid` |
| **BOX_SHADOW** | `evgr` path 阴影填充 | `draw_quad_solid` 扩展矩形 |
| **LABEL** | `evgrImagePattern` + path rect + fill | A8 纹理上传 + `draw_quad_tex`（recolor tint） |
| **IMAGE** | `evgrCreateImage` + pattern + fill | `glTexImage2D` + `draw_quad_tex` |
| **LINE** | `evgrLineTo` + `evgrStroke` | 法线偏移 4 顶点 → `GL_TRIANGLE_STRIP` |
| **ARC** | `evgr` arc path + fill | CPU 72 段 tessellation → triangle strip |
| **TRIANGLE** | `evgrCreateImage` + `evgrFill` | 直接 3 顶点 `glDrawArrays(GL_TRIANGLES, 0, 3)` |
| **MASK_RECT** | evgr scissor 包装 | `glScissor` 交集裁剪 |
| **LAYER** | `evgrluBindFramebuffer` + readback | `glBindFramebuffer` + `glReadPixels` |
| **BLUR** | Kawase 双 pass evgr blur | 占位半透明 overlay（简化） |
| **GRADIENT** | evgr 3-stop 梯度填充 | 纯色 fallback（简化） |

## 核心差异

EVGPU 依赖 evgr 库做 GPU 路径填充和 blend，EVGPUGANESH 全部自己管 GL 状态机。
