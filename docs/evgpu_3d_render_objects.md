# EVGPU 3D 渲染对象说明（基于当前实现）

> **状态：** 实现对照说明（非未落地设计）  
> **代码基准：** `lvgl` 子模块 @ `0804aaa43`（G8.x + DrawUnitEVGPU）  
> **关联设计：** [evgpu_3d_draw_tasks_design.md](./evgpu_3d_draw_tasks_design.md)、[lv_demo_3dscene_design.md](./lv_demo_3dscene_design.md)  
> **原则：** 「3D 渲染对象」分两层——**Draw Task 族**与**Widget / 资源对象**；Pick / Light 不是独立 Draw Task。

---

## 结论

1. **Draw Task 族**：真正进入 DrawUnit 调度的 3D 绘制单元（`LV_USE_3D_DRAW_TASKS`）。
2. **Widget / 资源对象**：应用侧创建，在 `LV_EVENT_DRAW_MAIN` 里把 task 提交进 pass。

补充边界：

| 概念 | 是否 Draw Task | 说明 |
|------|:--------------:|------|
| Pick | 否 | CPU 射线求交 API |
| Light | 否 | 挂在 pass 上的光照状态 |
| `lv_3dscene` widget | — | **本版未实现** |
| `3D_SYNC` task | — | 设计有、**本版未落地** |

---

## 1. Draw Task（3D 功能对象）

开关：`LV_USE_3D_DRAW_TASKS`。执行方：`lvgl/src/draw/evgpu/`。

| Task | 枚举 | 含义 | 关键数据 |
|------|------|------|----------|
| **VIEWPORT** | `LV_DRAW_TASK_TYPE_3D_VIEWPORT` | 开 3D pass：FBO+depth、相机、子 task 完成后 resolve 回父 layer | `pass_layer`、resolve 模式、opa |
| **CLEAR** | `LV_DRAW_TASK_TYPE_3D_CLEAR` | pass 内清 color / depth | 清屏色、是否清深度 |
| **MESH** | `LV_DRAW_TASK_TYPE_3D_MESH` | 单次 mesh 光栅化 | 顶点、索引、model 矩阵、depth/cull/phong |
| **LINE** | `LV_DRAW_TASK_TYPE_3D_LINE` | 3D 线段（网格 / 坐标轴） | 点列、颜色、线宽 |
| **SCENE** | `LV_DRAW_TASK_TYPE_3D_SCENE` | 整场景 batch；当前 `scene_id` = `lv_gltf` 对象，内部调 glTF 渲染后合成 | `scene_id` |
| **CALLBACK** | `LV_DRAW_TASK_TYPE_3D_CALLBACK` | DrawUnit 绑好 pass 后调用户 `render_cb` | 回调 + user_data |
| **BLIT（旧）** | `LV_DRAW_TASK_TYPE_3D` | 把已有 `tex_id` 贴进 layer（兼容 `lv_3dtexture`） | `tex_id`、flip、opa |

### 1.1 从属关系

```text
父 layer（2D）
 └─ 3D_VIEWPORT（BLOCKED，直到子 pass 完成）
      pass_layer 内按序：
        3D_CLEAR
        （lights 写入 pass，不是 task）
        3D_LINE / 3D_MESH* / 3D_SCENE / 3D_CALLBACK
      → resolve → 合成回父 layer
```

- `3D_MESH` / `LINE` / `SCENE` / `CLEAR` / `CALLBACK` **必须**挂在某个 VIEWPORT 的 `pass_layer` 上。
- `LV_DRAW_TASK_TYPE_3D`（BLIT）可单独打在普通 layer 上，不强制进 VP pass。

### 1.2 公共头文件

| Task | Header |
|------|--------|
| VIEWPORT | `include/lvgl/draw/lv_draw_3d_viewport.h` |
| CLEAR | `include/lvgl/draw/lv_draw_3d_clear.h` |
| MESH | `include/lvgl/draw/lv_draw_3d_mesh.h` |
| LINE | `include/lvgl/draw/lv_draw_3d_line.h` |
| SCENE | `include/lvgl/draw/lv_draw_3d_scene.h` |
| CALLBACK | `include/lvgl/draw/lv_draw_3d_callback.h` |
| BLIT | `include/lvgl/draw/lv_draw_3d.h` |

---

## 2. Widget / 应用侧对象

| 对象 | 宏 | 含义 | 提交什么 |
|------|-----|------|----------|
| **`lv_3dviewport`** | `LV_USE_3DVIEWPORT` | 3D 视口容器：相机、清屏、网格、遍历子对象 | `CLEAR` → lights → `LINE`(grid) → `MESH*` → `CALLBACK` → `VIEWPORT` |
| **`lv_3dmesh`** | `LV_USE_3DMESH` | 逻辑 3D 几何节点（不参与 2D 布局），父应为 viewport | 由 viewport `lv_3dmesh_submit_tree` → 多个 `3D_MESH` |
| **`lv_3dlight`** | `LV_USE_3DLIGHT` | 方向光 / 点光节点 | `lv_draw_3d_pass_add_light()`，**无独立 task** |
| **`lv_3dtexture`** | `LV_USE_3DTEXTURE` | 只显示外部 GL 纹理 | **`TYPE_3D`（BLIT）** |
| **`lv_gltf`** | `LV_USE_GLTF` | glTF 查看器；类仍继承 `lv_3dtexture` | EVGPU 路径：`CLEAR` + **`3D_SCENE`** + `VIEWPORT`；旧路径可走纹理 + BLIT |

### 2.1 对象树典型关系

```text
lv_obj（2D UI）
└─ lv_3dviewport
     ├─ lv_3dlight*     ← 写入 pass 灯光表
     └─ lv_3dmesh*      ← 提交 3D_MESH

lv_3dtexture            ← 独立 BLIT，不进 VP pass

lv_gltf                 ← 自己建 pass，scene_id=自己，走 3D_SCENE
```

### 2.2 `lv_3dviewport` 提交顺序（实现）

源码：`lvgl/src/widgets/3dviewport/lv_3dviewport.c` → `draw_3dviewport()`：

1. 创建 / 更新 `pass_layer`
2. `lv_draw_3d_pass_set_camera`
3. `lv_draw_3d_clear`
4. `lv_3dlight_submit_tree`（若启用）
5. grid → `lv_draw_3d_line`（若显示）
6. `lv_3dmesh_submit_tree`（若启用）
7. 可选 `lv_draw_3d_callback`
8. `lv_draw_3d_viewport` + `lv_draw_3d_viewport_end`

### 2.3 `lv_gltf` 与 SCENE（实现）

- 类关系：**仍** `base_class = &lv_3dtexture_class`（设计文档中「改为继承 viewport」尚未做）。
- EVGPU 路径（`LV_USE_3D_DRAW_TASKS && LV_USE_DRAW_EVGPU`）：`draw_gltf_viewport()` 提交 `CLEAR` + `3D_SCENE`（`scene_id = gltf obj`）+ `VIEWPORT`。
- `lv_draw_evgpu_3d_scene()`：用 `scene_id` 取 viewer，调用 `lv_gltf_render_scene()`，再把结果纹理合成进 pass。

---

## 3. 支撑概念（非独立「渲染对象」类）

| 概念 | Header / 位置 | 含义 | 关系 |
|------|---------------|------|------|
| **`lv_3d_camera_t`** | `lv_draw_3d_camera.h` | 轨道相机（yaw/pitch/distance/fov） | 挂在 viewport；写入 pass，供 MESH/LINE/CB/MVP |
| **`pass_layer`** | `lv_draw_3d_viewport.h` | VIEWPORT 私有子 layer | 承载 3D 子 task + camera + lights |
| **Pick** | `lv_draw_3d_pick.h` | 屏幕点 → 射线 → mesh 三角形求交 | `lv_3dviewport_pick_at` / `lv_3dmesh_pick_at_tree`；**不进 draw 队列** |
| **Mesh 材质字段** | `lv_draw_3d_mesh_dsc_t` | color + phong 标志 + shininess | 跟在 `3D_MESH` dsc 内，非独立 widget |
| **`lv_3d_light_dsc_t`** | `lv_draw_3d_light.h` | pass 灯光表（最多 `LV_3D_PASS_MAX_LIGHTS`） | 由 `lv_3dlight` 写入，MESH phong 读取 |

---

## 4. Widget ↔ Task 映射（本版）

| Widget | 提交的 Task / 行为 | 备注 |
|--------|-------------------|------|
| `lv_3dviewport` | `VIEWPORT` + 子 task | 3D 容器基类 |
| `lv_3dmesh` | `MESH` | 须在同一 VP pass 内 |
| `lv_3dlight` | 无 task，写 pass lights | 供 phong mesh |
| `lv_3dtexture` | `TYPE_3D`（BLIT） | 外部 tex；legacy 路径 |
| `lv_gltf` | `VIEWPORT` + `SCENE`（EVGPU）或 BLIT（旧） | 仍继承 `3dtexture` |
| viewport 内置 grid | `LINE` | `set_grid_visible` |
| Pick | — | API，非 task |

---

## 5. 应用选型速查

| 需求 | 创建什么 |
|------|----------|
| 自建简单 3D 场景（盒体 / OBJ / 网格 / 光照 / 拾取） | `lv_3dviewport` + `lv_3dmesh`（+ 可选 `lv_3dlight`） |
| 加载完整 glTF / PBR / IBL | `lv_gltf` |
| 只把已有 GL 纹理贴进 UI | `lv_3dtexture` |

---

## 6. 与旧理解 / 设计文档的差异

1. **不是**「3D = 一张纹理 blit」。主路径是 VIEWPORT pass 内多种 task 同帧调度。
2. **scene / mesh / line / clear / callback / viewport** 各有独立 `DrawTaskType`；**pick 没有**。
3. **`lv_3dscene` widget 不存在**；「scene」= `3D_SCENE` task + `lv_gltf`。
4. **`lv_gltf` 尚未改为继承 `lv_3dviewport`**，仍继承 `lv_3dtexture`，但 EVGPU 下已走 SCENE+VP。
5. **Light** 是 pass 状态，不是 task；设计中的 **`3D_SYNC`** 本版未作为独立枚举出现。

---

## 7. 关键源码路径

```text
lvgl/include/lvgl/draw/lv_draw.h              # task 枚举
lvgl/include/lvgl/draw/lv_draw_3d_*.h         # 各 3D dsc / API
lvgl/include/lvgl/widgets/lv_3dviewport.h
lvgl/include/lvgl/widgets/lv_3dmesh.h
lvgl/include/lvgl/widgets/lv_3dlight.h
lvgl/include/lvgl/widgets/lv_3dtexture.h
lvgl/include/lvgl/widgets/lv_gltf.h
lvgl/src/widgets/3dviewport/lv_3dviewport.c
lvgl/src/widgets/3dmesh/
lvgl/src/widgets/3dlight/
lvgl/src/draw/evgpu/lv_draw_evgpu.c           # dispatch
lvgl/src/draw/evgpu/lv_draw_evgpu_3d_*.c      # 各 task handler
lvgl/src/libs/gltf/gltf_view/lv_gltf_view.cpp # SCENE 路径
```
