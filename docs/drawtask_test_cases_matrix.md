# DrawTask × 三类测试用例对照表

> **状态：** 已保存（2026-07-21）  
> **说明：** 「第一类」指**本次未给每个 DrawTask 单独加**的 Unity/per-task 用例——实际覆盖来自本仓 `evgpu_benchmark` 场景 + G3 dump，不是 `test_draw_*_crt.c`。  
> **枚举来源：** `lvgl/include/lvgl/draw/lv_draw.h` → `lv_draw_task_type_t`

---

## 三类用例定义

| 类 | 名称 | 机制 | 判定 |
|----|------|------|------|
| **①** | 本仓库（本次） | 建 UI → DrawUnit（EVGPU / C_R_T）执行 → FPS 或 `LVGL_GL_DUMP` | 人眼 / PPM dump / FPS；**无** per-task Unity |
| **②** | LVGL 原有 | Unity + `lv_test_display` → `TEST_ASSERT_EQUAL_SCREENSHOT` vs `ref_imgs/`（默认 SW；VG-Lite → `ref_imgs_vg_lite/`） | 像素金图（典型 800×480，32bpp） |
| **③** | Skia | `gm/*.cpp` 画到 `SkCanvas` → Gold/DM 比参考图；**无** LVGL DrawTask 一一绑定，下表按**语义**对应 | Gold 像素金图 |

路径约定：

| 前缀 | 根路径 |
|------|--------|
| ① | `lv_port_linux/src/`（主仓） |
| ② | `lvgl/tests/src/test_cases/` |
| ③ | Skia 上游 `gm/`（本仓不 vendor；按文件名检索） |

C_R_T 实现目录：`lvgl/src/draw/evgpu_c_r_t/`。VECTOR P0 设计见 [evgpu_c_r_t_vector_design.md](./evgpu_c_r_t_vector_design.md)。

---

## 大表：每个 DrawTask → ① / ② / ③

| DrawTask | ① 本仓库用例（位置 + 处理逻辑） | ② LVGL 原有测试（位置 + 处理逻辑） | ③ Skia 语义对应（位置 + 处理逻辑） |
|----------|--------------------------------|-----------------------------------|-----------------------------------|
| **NONE** | — | — | — |
| **FILL** | **位置：** `src/evgpu_benchmark.c` → `fill_scene_create` / `gradient_scene_create`；G3：`demo_g3_showcase.c` → `make_card` / `build_world` 背景。<br>**逻辑：** style `bg` / `bg_grad` → FILL（+grad）；C_R_T：`lv_draw_evgpu_c_r_t_fill` / `lv_evgpu_c_r_t_grad`（SDF / 1D tex）→ FPS 或整帧 dump。 | **位置：** `draw/test_draw_blend.c`、`draw/test_clip_corner.c`、`draw/test_render_to_*.c`；间接：大量 widget 截图。<br>**逻辑：** canvas/obj 刷新 → 默认 SW（或 VG-Lite）画 → PNG 与 `ref_imgs/` 断言。 | **位置：** `gm/roundrects.cpp`、`gm/gradients.cpp`、`gm/convexpaths.cpp`。<br>**逻辑：** `drawRect` / `drawRRect` + `SkPaint::kFill_Style` → GPU/CPU backend → GM 金图。 |
| **BORDER** | **位置：** `border_scene_create`；G3：`make_card` border。<br>**逻辑：** style border → BORDER task；C_R_T：`lv_draw_evgpu_c_r_t_border`（SDF ring）。 | **位置：** `draw/test_clip_corner.c`、widget 截图；无独立 `test_draw_border.c`。<br>**逻辑：** cover / radius 场景连带产生 BORDER → 截图。 | **位置：** `gm/strokerect.cpp`、`gm/roundrects.cpp`（stroke）。<br>**逻辑：** `SkPaint::kStroke_Style` 描矩形/圆角。 |
| **BOX_SHADOW** | **位置：** `box_shadow_scene_create`；G3：bar / hover shadow。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_box_shadow` 分层圆角近似。 | **位置：** `draw/test_draw_dropshadow.c`、`draw/test_clip_corner.c`（shadow style）。<br>**逻辑：** 截图比阴影形状；VG-Lite 部分用例可能跳过。 | **位置：** `gm/shadows.cpp`、`gm/blurcircles.cpp`（近似 drop shadow）。<br>**逻辑：** blur + offset 合成。 |
| **LETTER** | **位置：** 无独立 scene；由 LABEL 内部拆出。<br>**逻辑：** C_R_T 走 label 路径的单字形上传/绘制。 | **位置：** `draw/test_draw_letter.c`。<br>**逻辑：** 直接 `lv_draw_letter` + 截图。 | **位置：** `gm/glyph_pos_*.cpp`、`gm/lcdtext.cpp`。<br>**逻辑：** 单 glyph 位姿 / 描边 / LCD text。 |
| **LABEL** | **位置：** `label_scene_create`；G3：各卡 title。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_label` 纹理字 atlas。 | **位置：** `draw/test_draw_label.c`、`widgets/test_label.c`、`widgets/test_span.c`。<br>**逻辑：** 多字体 / 换行 / span → 截图或功能断言。 | **位置：** `gm/texteffects.cpp`、`gm/stroketext.cpp`、`gm/fontmgr_bounds.cpp`。<br>**逻辑：** `drawString` / `TextBlob`。 |
| **IMAGE** | **位置：** `image_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_image`（解码 → tex → quad）。 | **位置：** `draw/test_image_formats.c`、`draw/test_image_colorkey.c`、`draw/test_bg_image.c`、`draw/test_image_cache.c`、`widgets/test_image.c`。<br>**逻辑：** 多色深 / 压缩 / 解码 / cache → 截图。 | **位置：** `gm/bitmaprect.cpp`、`gm/imagefilterscropped.cpp`、`gm/filterindiabox.cpp`。<br>**逻辑：** `drawImage` / `drawImageRect` + filter。 |
| **LAYER** | **位置：** `layer_scene_create`（transform / opa 子层）。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_layer` blit 子 FBO。 | **位置：** `draw/test_draw_layer.c`、`draw/test_layer_transform.c`。<br>**逻辑：** 子层 scale / rotate / mask → 截图。 | **位置：** `gm/savelayer*.cpp`、`gm/backdrop.cpp`。<br>**逻辑：** `saveLayer` + restore / backdrop。 |
| **LINE** | **位置：** `line_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_line`。 | **位置：** `widgets/test_line.c`（功能为主，截图较少）。<br>**逻辑：** API / 尺寸断言为主。 | **位置：** `gm/hairlines.cpp`、`gm/strokes.cpp`、`gm/line_geometry.cpp`。<br>**逻辑：** `drawLine` / path stroke。 |
| **ARC** | **位置：** `arc_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_arc`。 | **位置：** `widgets/test_arc.c`、`widgets/test_spinner.c`、`widgets/test_arclabel.c`。<br>**逻辑：** 角度 / 模式功能测试偏多，部分截图。 | **位置：** `gm/circulararcs.cpp`、`gm/arcto.cpp`。<br>**逻辑：** `drawArc` / `SkPath::arcTo`。 |
| **TRIANGLE** | **位置：** `triangle_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_triangle`。 | **位置：** `widgets/test_canvas.c`（多处 `lv_draw_triangle`）。<br>**逻辑：** canvas 层直接 draw + 截图。 | **位置：** `gm/convexpaths.cpp`、`gm/triangles.cpp`（若有）/ `drawVertices`。<br>**逻辑：** 填充三角 / vertices。 |
| **MASK_RECTANGLE** | **位置：** `mask_rect_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_mask_rect`。 | **位置：** `draw/test_draw_layer.c`（bitmap/clip 相关）、`draw/test_draw_sw_mask.c`、`draw/test_clip_corner.c`。<br>**逻辑：** SW mask / clip_corner 路径截图。 | **位置：** `gm/mask*.cpp`、`gm/circular_clips.cpp`、clipRRect GMs。<br>**逻辑：** `clipRRect` / `SkMaskFilter`。 |
| **MASK_BITMAP** | **位置：** **无**独立 scene。<br>**逻辑：** EVGPU / C_R_T **均不认领**（`evaluate` → 0）→ 交给 SW 等。 | **位置：** `draw/test_draw_layer.c`（`bitmap_mask_src`）、`draw/test_draw_sw_mask.c`。<br>**逻辑：** SW / 部分 GPU 路径；截图断言。 | **位置：** `gm/maskfilter.cpp`、alpha mask GMs。<br>**逻辑：** mask filter。 |
| **BLUR** | **位置：** `blur_scene_create`。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_blur` + `lv_evgpu_c_r_t_kawase`（需 kawase ready）。 | **位置：** `draw/test_draw_blur.c`；**widgets：** `widgets/test_blur.c`（style `blur_radius` / backdrop → BLUR task + 截图）。<br>**逻辑：** canvas 直调或控件 style → 截图。 | **位置：** `gm/blurs.cpp`、`gm/blurrect.cpp`、`gm/imageblur2.cpp`。<br>**逻辑：** `SkImageFilters::Blur` / mask blur。 |
| **VECTOR** | **位置：** bench **无**独立 vector scene；CRT 配置已开 `LV_USE_VECTOR_GRAPHIC`；可用 `lv_demo_vector_graphic()` / G3 若产生 path。<br>**逻辑：** C_R_T **已认领**（pref 70）：`lv_draw_evgpu_c_r_t_vector` — P0 flatten + ear-clip fill + stroke strip → solid GLES2（见 VECTOR 设计文档）；EVGPU 亦可认领（EVGR，pref 80，同时开时输给 C_R_T）。 | **位置：** `draw/test_draw_vector*.c`、`test_demo_vector_graphic.c`；**widgets：** `widgets/test_vector.c`（canvas fill/stroke + `DRAW_MAIN`）；chart curve 亦间接覆盖。<br>**逻辑：** VECTOR path → 截图。**GPU：** `OPTIONS_TEST_EVGPU` / `C_R_T` 可跑（见 [unity_sw_evgpu_crt_diff_2026_07_21.md](./unity_sw_evgpu_crt_diff_2026_07_21.md)）。 | **位置：** `gm/path*.cpp`、`gm/strokefill.cpp`、`gm/filltypes.cpp`、`gm/beziers.cpp`。<br>**逻辑：** `drawPath` 全套（fill rule、stroke、曲线）。 |
| **3D**（texture blit） | **位置：** demos `3dviewport` / `3dscene`（宏开启时）；bench **未**独立 scene。<br>**逻辑：** C_R_T：`lv_draw_evgpu_c_r_t_3d`（FBO tex blit）。 | **位置：** **无**专用 Unity（`TYPE_3D` blit 未单测）。<br>**逻辑：** — | **位置：** 非 Skia 2D GM；近 Graphite/3D 示例。<br>**逻辑：** **无对等 DrawTask**。 |
| **3D_VIEWPORT** | **位置：** 同上 3D demo。<br>**逻辑：** `lv_draw_evgpu_c_r_t_3d_viewport`：开/关 3D pass、FBO → 屏。 | **位置：** `widgets/test_3dviewport.c`（`test_3dviewport_grid_smoke`）。**仅** `OPTIONS_TEST_EVGPU` / `C_R_T`（宏开）；SW 下 IGNORE。<br>**逻辑：** viewport + grid 冒烟（无 SW 金图）。 | **无** |
| **3D_CLEAR** | **位置：** 随 3D pass。<br>**逻辑：** `…_3d_clear` 清 color/depth。 | 同上 `test_3dviewport_grid_smoke`（clear color） | **无** |
| **3D_LINE** | **位置：** 随 mesh / viewport demo。<br>**逻辑：** `…_3d_line`。 | 同上（grid lines） | 近 `gm/hairlines`（**2D**），非 3D |
| **3D_CALLBACK** | **位置：** 自定义 GL 回调 demo（若有）。<br>**逻辑：** `…_3d_cb`。 | **无**（本轮未加） | **无** |
| **3D_MESH** | **位置：** `LV_USE_DEMO_3DSCENE` / mesh widget。<br>**逻辑：** `…_3d_mesh`（phong 等）。 | **位置：** `widgets/test_3dviewport.c`（`test_3dviewport_mesh_box_smoke`，需 `LV_USE_3DMESH`）。EVGPU/CRT only。 | 非 Skia GM；自有引擎测试 |
| **3D_SCENE** | **位置：** GLTF demo（板端常 `LV_USE_GLTF 0` → 无此 task）。<br>**逻辑：** `…_3d_scene`（需 GLTF）。 | **无**（本轮未加） | **无** |

---

## 三类差异（读表时记住）

| | ① 本仓库 | ② LVGL | ③ Skia |
|--|----------|--------|--------|
| **粒度** | 场景级（一 scene 多 task） | 有的 per API，多数 widget/截图间接 | per GM 文件，极细 |
| **判定** | 人眼 / PPM dump / FPS | 像素金图 | Gold 像素金图 |
| **DrawUnit** | 真 GLES2 EVGPU / C_R_T | 默认 SW；可选 VG-Lite；**现已加** `OPTIONS_TEST_EVGPU` / `OPTIONS_TEST_EVGPU_C_R_T`（headless EGL，首轮仍比 `ref_imgs/` SW 金图 + MAE） | Skia backend（GL / Vulkan / …） |
| **缺口** | **没有** `test_draw_*_crt.c`；LETTER / MASK_BITMAP / 3D\* 场景级仍弱 | GPU OPTIONS 尚无独立 `ref_imgs_evgpu*`；3D widgets 冒烟仅 EVGPU/CRT（见 `widgets/test_3dviewport.c`） | **没有** LVGL task 概念；3D 不对齐 |

---

## ① 本仓库场景索引

| 索引 | 创建函数（`src/evgpu_benchmark.c`） | 主推 DrawTask |
|------|-----------------------------------|---------------|
| 0 | `fill_scene_create` | FILL |
| 1 | `border_scene_create` | BORDER |
| 2 | `box_shadow_scene_create` | BOX_SHADOW |
| 3 | `label_scene_create` | LABEL（含 LETTER） |
| 4 | `image_scene_create` | IMAGE |
| 5 | `line_scene_create` | LINE |
| 6 | `arc_scene_create` | ARC |
| 7 | `blur_scene_create` | BLUR |
| 8 | `triangle_scene_create` | TRIANGLE |
| 9 | `gradient_scene_create` | FILL（gradient） |
| 10 | `layer_scene_create` | LAYER |
| 11 | `mask_rect_scene_create` | MASK_RECTANGLE |
| 12 | `g3_showcase_scene_create` → `demo_g3_showcase.c` | FILL / BORDER / SHADOW / LABEL / IMAGE / … 混合 |
| dump | `LVGL_GL_DUMP` + C_R_T / EVGPU end_frame 路径 | 整帧像素 |

若要把「①」补成真正的 **每个 DrawTask 一条 Unity + C_R_T OPTIONS**，需扩展测试矩阵（已有 `OPTIONS_TEST_EVGPU` / `OPTIONS_TEST_EVGPU_C_R_T` + 离屏 GLES 回读；首轮仍用 SW `ref_imgs/` + MAE，独立 `ref_imgs_evgpu*` 待 MAE 稳定后再切）。VECTOR 首轮结果见 [unity_sw_evgpu_crt_diff_2026_07_21.md](./unity_sw_evgpu_crt_diff_2026_07_21.md)。
