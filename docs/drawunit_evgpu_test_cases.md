# DrawUnitEVGPU 实施前验证用例清单

> 在编写 `draw/evgpu/` 代码前，用本清单定义 **验什么、怎么验、通过标准**。  
> 关联：[drawunit_evgpu_design.md](./drawunit_evgpu_design.md) §4.8（GPU 必达）、§8（实施阶段）

---

## 1. 使用说明

| 字段 | 含义 |
|------|------|
| **ID** | 用例编号，实施阶段可勾选 |
| **P** | 优先级：P0 阻塞发布 / P1 必过 / P2 建议 |
| **阶段** | 对应 design §8：G0～G8（G8.0～G8.6） |
| **入口** | demo、脚本或 lvgl 单测 |
| **通过标准** | 客观判定条件 |

**建议执行环境**

```bash
# 板级 EVGPU 配置（示例）
cmake -B build-evgpu -DCONFIG=<soc>-evgpu -DLVGL_APP_DEMO=<demo>
cmake --build build-evgpu -j$(nproc)
./build-evgpu/bin/lvglsim -b wayland -W 800 -H 480 2>run.log
```

**全局日志检查（适用多数用例）**

- [ ] stderr 无 `GL error` / `unsupported style` / `Gradient fill is not supported`
- [ ] sysmon 有 `FPS` 行（`LV_USE_PERF_MONITOR_LOG_MODE=1`）
- [ ] apitrace 主路径无每帧全屏 `glReadPixels`（Canvas 用例除外）

---

## 2. 基础设施（G0）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| G0-01 | P0 | 编译链接 | `cmake --build build-evgpu` | 开启 `LV_USE_DRAW_EVGPU=1`，关 NANOVG/OPENGLES | 零 error；`lvglsim` 可执行 |
| G0-02 | P0 | Unit 注册 | 启动任意 demo | 断点或日志：`lv_draw_evgpu_init()` 在 `lv_opengles_init` 后调用 | EVGPU unit 存在；NanoVG unit 未注册 |
| G0-03 | P0 | GL context | G0-02 | `eglInitialize` / `eglMakeCurrent` 成功 | `evgpu_gl_ready()==true` |
| G0-04 | P1 | 能力查询 | 启动日志 | 打印 `evgpu_caps`：GLES 版本、FBO、max texture | 字段非零且与 `eglinfo` 一致 |
| G0-05 | P1 | FBO 池初始化 | G0-04 | 检查 `evgpu_fbo_pool` 预分配 | `evgpu_caps.fbo_ok==true`；失败时有降级重试日志 |
| G0-06 | P2 | 互斥宏 | 故意同时开 EVGPU+NANOVG 编译 | 应编译失败或 Kconfig 互斥 | 构建系统阻止双 GPU unit |
| G0-07 | P0 | 代码拆分 | 检查 `draw/evgpu/` 与 `draw/nanovg/` | evgpu 文件 `#if LV_USE_DRAW_EVGPU`；符号 `lv_draw_evgpu_*` / `lv_evgpu_*`；nanovg 无 EVGPU 守卫 | 两目录独立编译；无交叉引用 |
| G0-08 | P0 | stress 不崩溃 | `./build-evgpu-stress/bin/lvglsim -b wayland -W 800 -H 480` + `lv_demo_stress` | 运行 ≥45s | 无 SIGSEGV；sysmon 有 FPS 样本；日志 `DrawUnitEVGPU ready` |
| G0-09 | P1 | stress FPS 对照 | 对比 `wayland-egl` vs `wayland-evgpu` | 同分辨率/时长 | evgpu fps_avg 与 NanoVG 基线相当（WSLg ~175） |

---

## 3. GPU 必达 — 渐变（G1 / G5）

> 验收：**关闭 `LV_USE_VECTOR_GRAPHIC` 时渐变仍 GPU**（G1-06）；全开时与 SW 截图一致。

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| GR-01 | P0 | 纯色填充 | `lv_demo_render` → FILL | scene `FILL`，opa 100%/50% | 矩形正确；apitrace 有 clear 或 grad/fill draw |
| GR-02 | P0 | 圆角填充 | GR-01 | radius 0/8/全圆 | 圆角无锯齿异常 |
| GR-03 | P0 | 线性渐变 VER | `LINEAR_GRADIENT` scene | `LV_GRAD_DIR_VER`，2 stop | 纵向渐变；**无** VECTOR 宏 warn |
| GR-04 | P0 | 线性渐变 HOR | GR-03 | `LV_GRAD_DIR_HOR` | 横向渐变 GPU |
| GR-05 | P0 | 线性渐变任意角 | GR-03 | `LV_GRAD_DIR_LINEAR` 斜向 | 与 SW 参考图 SSIM/目视一致 |
| GR-06 | P0 | 径向渐变 | `RADIAL_GRADIENT` scene | 多 stop（≥3） | 多 stop shader 生效 |
| GR-07 | P1 | 锥形渐变 | `CONICAL_GRADIENT` scene | `LV_GRAD_DIR_CONICAL` | GPU 绘制；无 fallback warn |
| GR-08 | P0 | extend PAD | 自定义 widget / render 扩展 | `LV_GRAD_EXTEND_PAD` | 端点外钳位同色 |
| GR-09 | P0 | extend REPEAT | 同上 | `LV_GRAD_EXTEND_REPEAT` | 图案重复；§4.8.2 shader `fract` |
| GR-10 | P0 | extend REFLECT | 同上 | `LV_GRAD_EXTEND_REFLECT` | 镜像重复 |
| GR-11 | P1 | 渐变 + 圆角 | `FILL` + grad + radius>0 | 组合 | 圆角内渐变连续 |
| GR-12 | P1 | 无 VECTOR 宏 | config 设 `LV_USE_VECTOR_GRAPHIC=0` | 仅跑 GR-03～06 | **仍 GPU 渐变**；日志无 gradient warn |
| GR-13 | P2 | 三角渐变 | `TRIANGLE` scene + grad | 三角形填充 | GPU；与 SW 一致 |

---

## 4. GPU 必达 — 文字（G2）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| TX-01 | P0 | 静态标签 | `simple_button` / `TEXT` scene | `text_static=1` 固定串 | 文字清晰；EVGPU dispatch LABEL |
| TX-02 | P0 | 动态标签 | 自建：每 100ms `lv_label_set_text` | 非 static，计数递增 | **GPU 绘制**；apitrace **无** ReadPixels 文字路径 |
| TX-03 | P0 | 内容 hash 缓存 | TX-02 + 日志 | 连续相同文本 N 帧 | 缓存 hit 日志；改一字 hash miss 重绘 |
| TX-04 | P0 | 禁止指针 key | TX-02：`realloc` 同内容新缓冲区 | 指针变、内容同 | 显示正确；不误用旧缓存 |
| TX-05 | P1 | 多字体字号 | `lv_demo_widgets` | montserrat 12/16/24 | 字形 LRU 正常 |
| TX-06 | P1 | 装饰线 | `TEXT` scene | underline / strikethrough | GPU letter 合成 |
| TX-07 | P1 | 旋转缩放 | label `transform_angle` / `zoom` | 变换文字 | matrix + glyph quad |
| TX-08 | P1 | 中文/UTF-8 | 动态设置 CJK 串 | 常用字若干 | 无 tofu；glyph 缓存命中 |
| TX-09 | P2 | 长文本换行 | 窄宽 label 多行 | 自动换行 | 行距正常；无 SW label task |
| TX-10 | P2 | 显存压力 | 快速滚动长列表含文 | 列表 50+ 项 | 无 crash；LRU 淘汰有日志 |

---

## 5. GPU 必达 — 矢量（G3）

> config **必须** `LV_USE_VECTOR_GRAPHIC=1`。

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| VC-01 | P0 | VECTOR task 接管 | `lv_demo_vector_graphic_buffered` | 启动 demo | 无 `unsupported style`；preferred unit = EVGPU |
| VC-02 | P0 | SOLID 填充 | `test_draw_vector`（lvgl tests） | 基础路径 | GPU；截图与参考一致 |
| VC-03 | P0 | GRADIENT 填充 | vector demo 渐变路径 | 矢量渐变 | evgpu_grad 或 vector grad shader |
| VC-04 | P0 | PATTERN 填充 | 带 pattern 的 vector 场景 | `LV_VECTOR_DRAW_STYLE_PATTERN` | **GPU 纹理平铺**；非 SW |
| VC-05 | P1 | 描边 stroke | vector 描边 | width / join | GPU stroke |
| VC-06 | P1 | 虚线 dash | SVG dash 属性用例 | `test_draw_svg` 相关 | dash 细分 GPU 可见 |
| VC-07 | P1 | Lottie | `LV_USE_LOTTIE` demo | 动画播放 | VECTOR 每帧 GPU |
| VC-08 | P1 | SVG 文件 | `test_draw_svg.c` 抽 3～5 个场景 | skew/arc/path | 与 SW 参考容差内一致 |
| VC-09 | P2 | 嵌套 clip | vector 多层 scissor | 复杂合成 | FBO ping-pong；无花屏 |
| VC-10 | P2 | not_buffered 模式 | `lv_demo_vector_graphic_not_buffered` | 直接绘制 | 与 buffered 视觉一致 |

---

## 6. GPU 必达 — BLUR（G4）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| BL-01 | P0 | 小半径 blur | `test_draw_blur` / widgets 毛玻璃 | radius 8～32 | 模糊可见；EVGPU BLUR task |
| BL-02 | P0 | 圆角 blur | BL-01 | `corner_radius>0` | 圆角边缘柔和 |
| BL-03 | P0 | 大半径 256+ | 自建 radius=300～400 | 全屏局部区域 | **GPU Kawase**；无 NanoVG 256 skip |
| BL-04 | P0 | 超大半径 512+ | radius=512，降采样链 | 性能可接受 | FPS > 阈值（板级定义，如 ≥15@800×480） |
| BL-05 | P0 | 脏区局部 FBO | 小控件 blur 非全屏 | 仅脏区 | apitrace FBO 尺寸 ≈ 控件+padding |
| BL-06 | P1 | box_shadow 联动 | `BOX_SHADOW` / `test_draw_dropshadow` | 阴影=blur+offset | 与 blur 管线共用 |
| BL-07 | P1 | FBO 池复用 | stress 多 blur 对象 | `lv_demo_stress` | 无每帧 `glGenFramebuffer` 暴增 |
| BL-08 | P2 | FBO 降级 | 模拟 RGBA8888 失败（测试 hook） | 4444/565 降级 | blur 仍可用或该帧 skip+log，**非 SW** |
| BL-09 | P2 | 半透明叠层 | blur 上叠半透明 rect | `test_draw_blur` 后半段 | alpha 正确 |

---

## 7. 常规 2D GPU（G1 / G5）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| D2-01 | P0 | 边框 | `BORDER` scene | 四边/partial/radius | GPU BORDER |
| D2-02 | P0 | 盒阴影 | `BOX_SHADOW` scene | spread/offset/opa | GPU shadow |
| D2-03 | P0 | 图片 RGB565 | `IMAGE_NORMAL_*` | RGB565 资源 | 纹理上传正确色 |
| D2-04 | P0 | 图片 ARGB8888 | IMAGE + 透明 | alpha blend | 透明边缘无黑边 |
| D2-05 | P0 | 图片 recolor | `IMAGE_RECOLOR_*` | recolor 动画 | uniform 更新；无每帧 CPU recolor |
| D2-06 | P0 | 图片旋转缩放 | IMAGE transform | rotate 900/scale | matrix 正确 |
| D2-07 | P1 | 图片 tile POT | 64×64 tile 背景 | repeat | GPU repeat 采样 |
| D2-08 | P1 | 图片 tile NPOT | 100×100 tile | §5.2 策略 | 多 quad 模拟或文档预期行为 |
| D2-09 | P0 | Layer 合成 | `LAYER_NORMAL` scene | 子 layer alpha | FBO blend |
| D2-10 | P1 | Layer transform | `test_layer_transform` | scale/rotate layer | GPU matrix |
| D2-11 | P0 | 线段 | `LINE` scene | 宽线/端点 | GPU LINE |
| D2-12 | P0 | 圆弧 | `ARC_NORMAL` / `ARC_IMAGE` | 弧+图 | GPU ARC |
| D2-13 | P0 | 三角形 | `TRIANGLE` scene | 纯色三角 | GPU |
| D2-14 | P1 | 矩形 mask | `MASK_RECTANGLE` widget | 圆角 clip | scissor/stencil |
| D2-15 | P1 | blend mode | `BLEND_MODE` scene | 多种混合 | 与参考一致 |

---

## 8. 3D 合成（G6）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| D3-01 | P0 | glTF demo | `LVGL_APP_DEMO=gltf`（需 main 路由） | 加载模型旋转 | 3D+2D 同屏 |
| D3-02 | P0 | 3D task composite | apitrace | 单帧 | `lv_gltf_view_render` 后 `lv_opengles_render_texture` |
| D3-03 | P1 | 透明背景 3D | glTF alpha | 与 UI 叠加 | `lv_opengles_render` 混合模式正确 |
| D3-04 | P1 | h/v flip | 3dtexture flip 属性 | 翻转显示 | 纹理方向正确 |
| D3-05 | P2 | GL 状态恢复 | 3D 后画 2D 圆角 | 连续帧 | 无 GL 状态污染花屏 |
| D3-06 | P1 | `lv_3dview` 空白视口 | `LV_USE_3DVIEW=1` + `lv_demo_3dview`（规划） | 创建视口 clear color | FBO 纹理经 `lv_draw_3d` 合成；与 2D 同屏 |
| D3-07 | P1 | `render_cb` 自定义 mesh | `lv_3dview_set_render_cb` | 回调内 `glDraw*` | 每帧 FBO 更新；EVGPU `lv_draw_evgpu_3d` composite |
| D3-08 | P1 | 轨道相机 | yaw/pitch/distance 手势或 API | 拖动旋转 | 矩阵变化；`ON_DEMAND` 仅脏时重渲 |
| D3-09 | P2 | 透明背景 | `clear_color` alpha=0 | 与半透明 2D 叠加 | blend 正确；无黑边 |
| D3-10 | P2 | resize FBO | 动态改 widget 尺寸 | resize 后 | renwin 重建；无花屏/leak |
| D3-16 | P0 | **3D_VIEWPORT pass** | G8.0 build | apitrace 单帧 | FBO bind → 子 task → resolve composite |
| D3-17 | P1 | **2D/3D task 交错** | FILL + VP + LABEL 同帧 | 截图/apitrace | task 顺序正确 |
| D3-18 | P1 | **depth 遮挡** | 两 MESH 同 VP | 旋转视角 | 近处 mesh 遮挡远处 |
| D3-19 | P0 | **gltf SCENE task** | G8.3 build | apitrace | 无 `LV_EVENT_DRAW` 内 `glDraw*` |

> **3D Draw Task 全族规格：** [evgpu_3d_draw_tasks_design.md](./evgpu_3d_draw_tasks_design.md)。D3-06～10 为 widget 功能；D3-16～19 为 task 管线验收。

---

## 9. SW 兜底验证（须确认仍走 SW）

> 这些用例 **预期 EVGPU evaluate=0**，由 `draw/sw` 完成，结果正确即可。

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| SW-01 | P0 | MASK_BITMAP | `test_draw_layer` bitmap_mask | `bitmap_mask_src` 样式 | 蒙版形状正确；EVGPU 未接 task |
| SW-02 | P0 | Canvas CPU | `lv_canvas` + `lv_canvas_init_layer` | 画 fill/label | 全程 SW；apitrace 无 EVGPU 主屏 bind |
| SW-03 | P1 | 私有像素格式 | NEMA_TSC 等（若有资源） | draw image | SW 或 decoder 拒绝 |
| SW-04 | P1 | I1/L8 格式 | `test_render_to_i1` / `l8` | 绘制 | SW 路径 |
| SW-05 | P1 | image skew | `transform_skew` on image | skew≠0 | SW（与现网一致） |
| SW-06 | P1 | bitmap_mask+A8 | layer + A8 mask 图 | 组合 | SW `apply_mask` |
| SW-07 | P2 | Snapshot | `lv_snapshot` API | 截图到 buffer | CPU buffer 内容正确 |

---

## 10. 性能与稳定性（G7）

| ID | P | 用例名 | 入口 | 步骤 | 通过标准 |
|----|---|--------|------|------|----------|
| PF-01 | P0 | stress 短跑 | `scripts/benchmark_stress_shm_vs_egl.sh` 60s | EVGPU build | 无 crash；sysmon FPS 稳定 |
| PF-02 | P0 | benchmark | `LVGL_APP_DEMO=benchmark` | 全场景 | CSV render/flush 有数据；EVGPU ≥ SW 或 CPU↓ |
| PF-03 | P1 | 长时间 soak | stress 30min+ | 循环 | 无 GL leak、显存涨 |
| PF-04 | P1 | 4× 分辨率 | stress 3200×1920 | 高压 | FPS 记录入 `benchmark_logs/` |
| PF-05 | P1 | 切屏 | `lv_screen_load` 往返 | 多屏 | 无残影；layer readback 正确 |
| PF-06 | P2 | 多窗口 resize | Wayland resize | 动态改 W/H | FBO/display 纹理重建 OK |
| PF-07 | P2 | draw_mult | `LV_DEMO_STRESS_DRAW_MULT=10` | 加压 | 相对 mult=1 可对比 |

---

## 11. apitrace / 路径验收（专项）

| ID | P | 检查项 | 方法 | 通过标准 |
|----|---|--------|------|----------|
| AP-01 | P0 | 主屏无 ReadPixels | apitrace `simple_button` 60 帧 | 无全屏 readback |
| AP-02 | P0 | 主屏无错误 TexImage | 同上 | 无每帧 `glTexImage2D` 整屏 fb |
| AP-03 | P0 | flush 路径 | wayland-egl | `eglSwapBuffers` 或 `render_display_texture` |
| AP-04 | P1 | 渐变 shader | `lv_demo_render` LINEAR | `glUseProgram` 命中 evgpu_grad |
| AP-05 | P1 | blur FBO | BL-03 | `glBindFramebuffer` ping-pong |
| AP-06 | P1 | 动态文字无 SW | TX-02 + trace | 无 SW blend 到 ARGB 再上传 |
| AP-07 | P2 | 3D 路径 | gltf demo | render 在 composite 之前 |
| AP-08 | P2 | `lv_3dview` FBO | `lv_demo_3dview`（规划） | FBO bind → `render_cb` → composite；无全屏 ReadPixels |

---

## 12. 阶段门禁（实施勾选表）

| 阶段 | 最少通过用例 | 门禁 |
|------|-------------|------|
| **G0** | G0-01～03, G0-07～08 | 可启动；`draw/evgpu/` 独立；stress 不崩溃 |
| **G1** | GR-01～06, GR-12, D2-01, TX-01, AP-01～03 | 主屏+渐变无 VECTOR 依赖 |
| **G2** | TX-02～04, AP-06 | 动态文字 GPU |
| **G3** | VC-01～04 | 矢量+PATTERN GPU |
| **G4** | BL-01～05 | 大半径 blur GPU |
| **G5** | D2-03～13, GR-08～10, SW-01～02 | 2D 完整+兜底 |
| **G6** | D3-01～05（3D_BLIT） | 3D 纹理合成 |
| **G8** | D3-06～08, D3-11, D3-16～19（按 G8.x 子阶段） | 3D Draw Task 族 |
| **G7** | PF-01～02 | 性能基线 |

---

## 13. 与现有 lvgl tests 映射

| lvgl test 文件 | 覆盖 EVGPU 用例 |
|----------------|----------------|
| `test_draw_blur.c` | BL-01, BL-02, BL-09 |
| `test_draw_vector.c` / `test_draw_vector_detail.c` | VC-02, VC-05 |
| `test_draw_svg.c` | VC-08 |
| `test_draw_layer.c` | D2-09, SW-01 |
| `test_draw_label.c` / `test_draw_letter.c` | TX-01, TX-06 |
| `test_draw_dropshadow.c` | BL-06, D2-02 |
| `test_layer_transform.c` | D2-10 |
| `test_render_to_*.c` | SW-04（CPU canvas，非 EVGPU 主路径） |
| `test_demo_render.c`（若有） | GR-* / D2-* 全场景 |

**说明**：lvgl 单测默认 **SW/离屏 canvas**，多数字面验证 **逻辑正确性**；EVGPU 专项须在 **`lvglsim` + EGL** 下跑 §3～11 中带 apitrace/sysmon 的用例。

---

## 14. 建议新增（实施时）

| 项 | 说明 |
|----|------|
| `LVGL_APP_DEMO=render` | main.c 路由：循环 `lv_demo_render` 各 scene |
| `LVGL_APP_DEMO=evgpu_verify` | 专用验证屏：TX-02/BL-03/GR-09 单页切换 |
| `LVGL_APP_DEMO=3dview`（规划） | `lv_demo_3dview`：D3-06～10 |
| `widgets/3dview/`（规划） | `LV_USE_3DVIEW`；见 integration guide §4.3 |
| `scripts/verify_evgpu.sh` | 按 **CP-XX** 跑 build+demo+门禁 grep；见 [design §8.8](./drawunit_evgpu_design.md#88-checkpoint-步步为营验证与提交) |
| `docs/evgpu_test_results.md` | **Checkpoint 勾选**结果表（每 CP 一行，随 push 更新） |

---

## 15. 最小发布集（P0 共 35 项）

若时间紧，以下 **必须全过** 方可称 EVGPU MVP：

```
G0-01, G0-02, G0-03
GR-01, GR-03, GR-06, GR-08, GR-09, GR-10, GR-12
TX-01, TX-02, TX-03
VC-01, VC-02, VC-04
BL-01, BL-03, BL-05
D2-01, D2-03, D2-04, D2-09, D2-11, D2-12
SW-01, SW-02
PF-01, PF-02
AP-01, AP-02, AP-03
```

其余 P1/P2 在 G7 前补齐。
