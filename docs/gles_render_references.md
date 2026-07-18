# GLES 渲染引擎参考

## 矢量 2D 渲染（最相关）
- **Cairo GLES** — `cairo-glesv2` backend，FBO offscreen 合成，gradient/pattern/掩码
- **ThorVG GL backend** — LVGL 已集成，GLES3 后端 (`src/libs/thorvg/src/renderer/gl_engine/`)
- **Rive** — 商业级矢量动画，deferred pipeline + batching
- **NanoVG gl2** — 800 行单文件 GLES2 渲染器，最佳入门参考 (`src/libs/nanovg/`)

## 混合渲染 (SW + GL)
- **Qt Quick 2 / QPainter** — GLES 合成层，SW 画 texture 上传
- **Flutter Impeller** — shader-based 路径填充替代 triangulation
- **WebRender (Servo)** — batch 驱动，instancing，极简 state 切换

## 纯 GLES 参考
- **gl3w / GLEW 示例** — 基础 shader + VBO + VAO
- **Mali GPU SDK 示例** — ARM 官方 GLES 教程
- **PowerVR SDK** — Imagination Tech GLES 教程，tiled-GPU 优化

## 实验性
- **Vello** (Raph Levien) — compute shader 做 2D，不 triangulate
- **Blend2D** — 纯 SW，pipeline 设计可借鉴
