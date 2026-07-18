# GLES 渲染引擎对比分析

## 1. NanoVG gl2 — 最简可行 GLES2 矢量渲染

**文件位置:** `lvgl/src/libs/nanovg/nanovg_gl.h` (~800 行)

### 架构
```
nvgBeginFrame → 路径构建 → nvgFill/Stroke → nvgEndFrame
                                         ↓
                ┌────────── CPU ─────────┬──── GPU ──────┐
                │ tessellate path        │               │
                │ → triangle fans + AA   │  stencil fill  │
                │ → record call/vert     │  → GL_TRIANGLES│
                │ → pack uniforms        │  cover quad    │
                │                        │  VBO double buf│
                └────────────────────────┴───────────────┘
```

### 关键设计
- **Stencil-then-Cover** 路径填充：先用 stencil INCR/DECR 累加 winding，再画全屏 quad 做 NOTEQUAL 测试
- **4 种着色器**：渐变填充、纹理填充、stencil 写入、纹理三角形
- **双缓冲 VBO**：交替上传，避免 CPU-GPU 同步
- **状态缓存**：boundShader/boundTexture/blendFunc 防冗余 gl 调用
- **片元统一数组**：12 个 vec4 打包成一次 glUniform4fv
- **GLES2 限制处理**：NPOT 纹理禁用 repeat、GL_LUMINANCE 代替 GL_R8、手动行扫描纹理上传

### 与 evgpuganesh 的对比
- NanoVG 用 stencil path fill → 需要 stencil buffer（evgpuganesh 没有使用）
- NanoVG 不做 per-call batching，只做 per-frame 累积
- evgpuganesh 的 VBO 用法更简单（单个 VBO，无双缓冲）
- 两者的 shader 数量相近

---

## 2. ThorVG GL Engine — 完整的企业级 GLES3 矢量渲染

**位置:** 上游 ThorVG `src/renderer/gpu_engine/gl/` (~30 个文件)  
**注意:** LVGL 当前只集成 ThorVG SW 后端，GL 引擎代码不在本地

### 架构
```
prepare() → preRender() → renderShape() → postRender() → sync()
    ↓            ↓             ↓               ↓            ↓
 tessellate  编译shader    记录任务         结束pass    执行GPU
 path →       +            到               +   FBO    任务图
 GlShape     initShaders  渲染Pass         交换          +VBO上传
```

### 关键设计 (与 evgpuganesh 最相关)
- **Stage buffer**: 所有顶点/索引/uniform 先缓存在 CPU，`sync()` 时一次性 `glBufferData` 上传到 GPU，大幅减少 GL 调用
- **双 batch 系统**:
  - `GlSolidBatch`: 合并纯色填充到单个 draw call
  - `GlStencilCoverBatch`: 批处理 stencil-then-cover 模式
- **RenderPass + FBO 栈**: 管理离屏合成/混合/掩码
- **UBO** 传渐变数据（std140 layout）
- **85 种混合着色器**: 17 混合模式 × 5 源类型

### 对 evgpuganesh 的启发
- **Stage buffer 模式** 可以解决当前 VBO 每帧上传效率问题
- **SolidBatch** 减少纯色块的 draw call 数
- **按深度排序** 实现 painter's algorithm 时用 `GL_GREATER` depth test

---

## 3. Rive — Pixel Local Storage 激进方案

**不在 LVGL 代码库中**
**目标:** 单 pass、无 CPU 曲面细分、order-independent 矢量渲染

### 架构 (三阶段 PLS)
```
Phase 1: 资源预渲染 (GPU)
  ├─ ColorRampPipeline → 渐变纹理
  ├─ TessellatePipeline → Bezier → span → 纹理
  └─ AtlasPipeline → 羽化边缘

Phase 2: 主渲染 (Pixel Local Storage)
  ├─ 激活 PLS: Color / Coverage / Clip / Scratch 4 planes
  ├─ glDrawElementsInstanced 一次调用画所有路径
  └─ Coverage 累加 (CW+, CCW−)

Phase 3: Resolve → 最终帧缓冲
```

### 关键突破
- **GPU 端曲面细分**: 用 Wang's formula 在 shader 里细分 Bezier，无 CPU tessellation
- **单实例 Draw Call**: `glDrawElementsInstanced` 画所有路径
- **PLS 实现 OIT**: 不需要 stencil pass，没有多 pass
- **GLES 降级路径**: native PLS → Fragment Shader Interlock → R/W texture → MSAA fallback

### 对 evgpuganesh 的启发
- PLS 需要 GLES3.0+ `EXT_shader_pixel_local_storage`，Mali-400 GLES2 不支持
- 但可以用 **fragment shader interlock** 回退（也不支持 GLES2）
- Rive 思路不适合当前硬件，但 **texture-based tessellation** 值得关注

---

## 4. Cairo GLES — CPU 辅助的 GPU 渲染（已废弃）

**状态:** Cairo 2023 年移除 GL 后端，上游不再维护。community fork 约 4500 行。

### 架构
```
cairo_fill()/stroke()
       ↓
Span Compositor (CPU 扫描转换)
       ↓
路径 → 水平 spans → GL_LINES/GL_QUADS
       ↓
GPU 填充/混合
```

### 关键设计
- **CPU 扫描转换** + GPU 填充（路径不走 stencil）
- **4 种着色器**: solid/texture/gradient/glyph
- **单共享 VBO（~64KB）**，每次写 mapped buffer range
- **渐变纹理**: linear 1024×1, radial 1024×2，CPU 端烘焙
- **无帧级 batching**，immediate-mode API

### 局限性
- CPU 端路径扫描转换是瓶颈
- 每帧重新细分/重新上传
- 已从 Cairo 主仓库移除
- 不适用纯 GPU 加速场景

---

## 横向对比

| 特性 | NanoVG | ThorVG GL | Rive | Cairo GL |
|------|--------|-----------|------|----------|
| GLES 版本 | 2.0 | **3.0+** | 3.0+ (PLS) | 2.0 |
| 路径填充 | stencil-then-cover | stencil + batch | PLS + instancing | CPU spans → GL_LINES |
| 抗锯齿 | 几何扩展 | MSAA / CPU | PLS coverage | CPU coverage / MSAA |
| Batching | per-frame | **SolidBatch + StageBuffer** | **单 instanced call** | per-call |
| VBO 策略 | 双缓冲 | Stage buffer → 一次上传 | instancing | 单 VBO subdata |
| Shader 数 | 4 | 30+ (含85 blend) | 4-5 pipelines | 4-5 |
| Stencil | 需要 | 需要 | **不依赖** | 不需要 |
| FBO | 可选(utils) | **核心(RenderPass)** | FBO for PLS resolve | 需要(offscreen) |
| 代码量 | ~800行(gl) | ~30文件 | ~50文件 | ~4500行 |
| 维护状态 | 停滞 | **活跃** | **活跃(商业)** | **已废弃** |

---

## 对 evgpuganesh 的建议

按优先级从高到低：

### P0 — 立即采纳
1. **Stage buffer 模式** (ThorVG): 所有顶点数据先聚在 CPU buffer，sync 时一次 `glBufferData`。减少 gl 调用次数 → **直接解决当前每个 draw call 都上传 VBO 的性能问题**
2. **状态缓存** (NanoVG): cached shader/texture/blend func → 我们当前在 `lv_evgpuganesh_gl.c` 里每帧都设置状态

### P1 — 短期优化
3. **纯色 batching** (ThorVG SolidBatch): 同色的矩形/三角形合并到一次 draw call
4. **Shader 简化**: 我们的 5 个 program 可以合并 fragment shader（NanoVG 用 #define SHADER_TYPE 编译 4 个变体）

### P2 — 中期
5. **双缓冲 VBO** (NanoVG): 交替 2 个 VBO 消除 pipeline stall
6. **Depth sorting** (ThorVG): 用 `GL_GREATER` depth test 实现 painter's algorithm，减少 overdraw
7. **统一 UBO** (ThorVG): 把 projection matrix 等统一数据放 UBO（GLES2 不支持 UBO，但可以用 uniform array 模拟——见 NanoVG 的 `frag[12]` 技巧）

### 不适合当前硬件的方案
- Stencil-then-Cover (NanoVG/ThorVG) → 增加 draw call，不适合我们的简化 2D 场景
- PLS (Rive) → 需要 GLES3.0+
- CPU 扫描转换 (Cairo GL) → 和我们 GPU 加速目标相反
