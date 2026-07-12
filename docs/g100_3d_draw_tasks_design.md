# G100 3D Draw Task 体系设计（已认可方向）

> **状态：** 设计规格（未实现），**方向已认可**  
> **总体计划索引：** [drawunit_g100_design.md §8](./drawunit_g100_design.md#8-实施阶段g100-专项与总体计划)  
> **关联：** [drawunit_g100_design.md](./drawunit_g100_design.md) G6/G8、[opengles2_gpu_integration_guide.md §4](./opengles2_gpu_integration_guide.md#4-3d-路径draw-task-体系与-widget)、[drawunit_g100_test_cases.md](./drawunit_g100_test_cases.md) D3 系列  
> **原则：** 3D 光栅化纳入 Draw Task 管线，与 2D task **同队列、同帧、同 DrawUnit** 调度；不再把 3D 降级为「仅 blit 一张纹理」。

---

## 1. 设计动机

| 现状（旧 `LV_DRAW_TASK_TYPE_3D`） | 新设计 |
|----------------------------------|--------|
| 仅 `tex_id` + area + opa，语义 = 2D 纹理合成 | **8 类 3D task**，覆盖 VP / mesh / scene / blit |
| `glDraw*` 在 Widget `LV_EVENT_DRAW_MAIN` 内偷跑 | **DrawUnitG100 dispatch** 统一执行 |
| 3D 与 2D 组合 = 先 FBO 再 blit | **task 队列直接交错**（2D → 3D VP → 2D → …） |
| 一种 task 对应所有 3D | **每类 3D 对象**映射到明确 task |

---

## 2. 3D Draw Task 类型枚举（规划）

在 `lv_draw_task_type_t` 中新增（建议数值段 `0x40～0x4F`，与 2D 区分）：

| 枚举值 | 代号 | 职责 |
|--------|------|------|
| `LV_DRAW_TASK_TYPE_3D_BLIT` | BLIT | 外部已有 `tex_id` 合成进 layer（**兼容**旧 `3D`） |
| `LV_DRAW_TASK_TYPE_3D_VIEWPORT` | VP | 开/关 3D pass：FBO+depth、camera、resolve |
| `LV_DRAW_TASK_TYPE_3D_CLEAR` | CLR | pass 内清 color/depth |
| `LV_DRAW_TASK_TYPE_3D_MESH` | MESH | 单次 mesh draw（VBO+material+transform） |
| `LV_DRAW_TASK_TYPE_3D_LINE` | LINE3D | 3D 线段/grid/axes |
| `LV_DRAW_TASK_TYPE_3D_SCENE` | SCENE | 整场景 batch（gltf / scene widget） |
| `LV_DRAW_TASK_TYPE_3D_CALLBACK` | CB | DrawUnit 托管 bind 后调用 `render_cb` |
| `LV_DRAW_TASK_TYPE_3D_SYNC` | SYNC | GL 状态屏障（`reinit_state` 等） |

**迁移：**

```c
#if LV_USE_3D_LEGACY_ALIAS
#define LV_DRAW_TASK_TYPE_3D  LV_DRAW_TASK_TYPE_3D_BLIT  /* 一版兼容 */
#endif
```

---

## 3. 与 `LAYER` task 的对比

| 维度 | `LV_DRAW_TASK_TYPE_LAYER` (2D) | `LV_DRAW_TASK_TYPE_3D_VIEWPORT` (3D) |
|------|--------------------------------|--------------------------------------|
| 离屏目标 | 2D layer buffer / FBO | **FBO + depth buffer** |
| 子 task | 2D task 列表 | **3D task 族**（CLR/MESH/LINE/SCENE/CB） |
| 生命周期 | `BLOCKED` 直至子 layer 完成 | 同：`BLOCKED` 直至 VP pass 完成 |
| 合成回父 layer | blend / transform / opa | color 附件 resolve + alpha blend |
| 创建 API | `lv_draw_layer()` / `lv_draw_layer_create()` | `lv_draw_3d_viewport()` + 子 `lv_layer_t` 或内嵌 pass 栈 |
| 典型用途 | 圆角、旋转、blur | 3D 场景、相机、深度 |

**实现选项（G8.0 选 B，G8.3 可评估 A）：**

| 选项 | 做法 |
|------|------|
| **A** | 扩展 `lv_layer_t` 增加 `depth_rb` / `is_3d` 标志，与 2D LAYER 共用子 layer 机制 |
| **B（推荐先做）** | 独立 `3D_VIEWPORT` task + `lv_3d_pass_t` 私有 pass 栈，语义清晰 |

---

## 4. `3D_VIEWPORT` 与 blocked 子队列

### 4.1 状态机

```text
3D_VIEWPORT task 创建
    → state = BLOCKED
    → 分配 lv_3d_pass_t（FBO、depth、camera、子 task 列表）
    → widget / scene 向 pass 子 layer 添加 CLR / MESH / …
    → 子 task 全部 FINISHED
    → G100 resolve（FBO color → 父 layer composite）
    → VP task FINISHED
```

与 `LAYER` 相同：**父 task 在子任务完成前保持 `LV_DRAW_TASK_STATE_BLOCKED`**（见 `lv_draw.h` 注释）。

### 4.2 `lv_draw_3d_viewport_dsc_t`（字段级）

```c
typedef enum {
    LV_3D_RESOLVE_TO_LAYER,     /* 默认：resolve 后 blend 进父 layer */
    LV_3D_RESOLVE_TO_TEXTURE,   /* 产出 tex_id，供 3D_BLIT 或 readback */
    LV_3D_DIRECT_TO_LAYER,      /* 无 FBO，直接画到当前 layer FB（慎用） */
} lv_3d_resolve_mode_t;

typedef struct {
    lv_draw_dsc_base_t base;
    lv_3d_resolve_mode_t resolve_mode;
    lv_color_format_t    color_format;   /* 通常 ARGB8888 / RGBA */
    uint8_t              depth_bits;     /* 0 / 16 / 24 */
    lv_opa_t             opa;            /* resolve 时混合透明度 */
    lv_3d_camera_t       camera;         /* view + proj，见 §5 */
    lv_3d_pass_id_t      pass_id;        /* 运行时填充：pass 句柄 */
} lv_draw_3d_viewport_dsc_t;
```

### 4.3 公共 API

```c
void lv_draw_3d_viewport_dsc_init(lv_draw_3d_viewport_dsc_t * dsc);
void lv_draw_3d_viewport(lv_layer_t * layer, const lv_draw_3d_viewport_dsc_t * dsc,
                         const lv_area_t * coords);

/* 在 VP pass 激活期间，向 pass 子 layer 提交子 task */
lv_layer_t * lv_draw_3d_viewport_get_layer(lv_3d_pass_id_t pass_id);
void lv_draw_3d_viewport_end(lv_layer_t * pass_layer);  /* 可选显式结束 */
```

---

## 5. 相机与数学（共用）

```c
typedef struct {
    lv_point3_t   position;
    lv_point3_t   target;       /* look-at */
    lv_vector3_t  up;
    float         yaw, pitch, distance;  /* 轨道相机；与 gltf 对齐 */
    float         fov_y;        /* 弧度 */
    float         near_z, far_z;
} lv_3d_camera_t;

/* DrawUnit 在 VP begin 时计算 */
void lv_3d_camera_get_view_proj(const lv_3d_camera_t * cam, lv_matrix4x4_t * view,
                                 lv_matrix4x4_t * proj, lv_matrix4x4_t * view_proj);
```

---

## 6. 各 Task 描述符（字段级）

### 6.1 `3D_CLEAR`

```c
typedef struct {
    lv_draw_dsc_base_t base;
    lv_color32_t       color;
    lv_opa_t           opa;
    bool               clear_depth;
    float              depth_value;      /* 通常 1.0 */
} lv_draw_3d_clear_dsc_t;

void lv_draw_3d_clear(lv_layer_t * pass_layer, const lv_draw_3d_clear_dsc_t * dsc);
```

### 6.2 `3D_MESH`

```c
typedef struct {
    uint32_t vbo, ibo;
    uint32_t index_count;
    uint32_t index_type;                 /* GL_UNSIGNED_SHORT / INT */
    lv_3d_material_id_t material_id;
    lv_matrix4x4_t      model_matrix;
    lv_draw_3d_mesh_flags_t flags;       /* DEPTH_TEST, CULL_FACE, … */
} lv_draw_3d_mesh_dsc_t;

typedef struct {
    lv_draw_dsc_base_t base;
    lv_draw_3d_mesh_dsc_t mesh;
} lv_draw_3d_mesh_task_dsc_t;

void lv_draw_3d_mesh(lv_layer_t * pass_layer, const lv_draw_3d_mesh_task_dsc_t * dsc,
                     const lv_area_t * bounds);  /* bounds 用于 dirty/clip */
```

**Material（资源，非独立 task）：**

```c
typedef struct {
    lv_3d_shader_id_t shader_id;
    lv_color32_t      diffuse;
    lv_opa_t          opa;
    uint32_t          texture_id;        /* 0 = none */
    int32_t           loc_mvp, loc_tex;
} lv_3d_material_t;
```

### 6.3 `3D_LINE`

```c
typedef struct {
    lv_draw_dsc_base_t base;
    const lv_point3_t * points;
    uint32_t            point_cnt;
    lv_color32_t        color;
    float               width;
    bool                depth_test;
} lv_draw_3d_line_dsc_t;
```

### 6.4 `3D_SCENE`

```c
typedef struct {
    lv_draw_dsc_base_t base;
    lv_3d_scene_id_t   scene_id;         /* scene graph 句柄 */
    lv_matrix4x4_t     root_transform;
    uint32_t           render_flags;     /* SKIP_TRANSPARENT, WIREFRAME, … */
} lv_draw_3d_scene_dsc_t;
```

`lv_gltf` 修订后：draw 阶段提交 `3D_VIEWPORT` + `3D_SCENE`（`scene_id` 指向 gltf 内部场景），**不再**在 event 里直接 `lv_gltf_view_render()`。

### 6.5 `3D_CALLBACK`

```c
typedef void (*lv_draw_3d_cb_t)(lv_layer_t * pass_layer, const lv_3d_camera_t * cam,
                                 const lv_matrix4x4_t * view_proj, void * user_data);

typedef struct {
    lv_draw_dsc_base_t base;
    lv_draw_3d_cb_t    cb;
    void *             user_data;
} lv_draw_3d_callback_dsc_t;
```

DrawUnit：`VP` 已 bind FBO → 调 `cb` → 恢复约定 GL 状态 → 继续子队列。

### 6.6 `3D_BLIT`（兼容）

```c
/* 与现有 lv_draw_3d_dsc_t 同构，rename API */
typedef lv_draw_3d_dsc_t lv_draw_3d_blit_dsc_t;

void lv_draw_3d_blit(lv_layer_t * layer, const lv_draw_3d_blit_dsc_t * dsc,
                     const lv_area_t * coords);
#define lv_draw_3d  lv_draw_3d_blit  /* deprecated alias */
```

### 6.7 `3D_SYNC`

```c
typedef enum {
    LV_3D_SYNC_REINIT_GLVG = 1 << 0,
    LV_3D_SYNC_FLUSH       = 1 << 1,
} lv_3d_sync_flags_t;

typedef struct {
    lv_draw_dsc_base_t base;
    lv_3d_sync_flags_t flags;
} lv_draw_3d_sync_dsc_t;
```

插入点：3D pass 与 2D task 边界，或 `g100_end_frame` 前后。

---

## 7. 一帧 task 队列示例

```text
父 layer（屏幕）
  [FILL 背景]
  [3D_VIEWPORT area=(10,10,330,250)  BLOCKED]
      pass layer:
        [3D_CLEAR]
        [3D_MESH cube]
        [3D_MESH floor]
        [3D_LINE grid]
      → resolve blend → 父 layer
  [LABEL 标题]
  [3D_VIEWPORT area=(340,10,790,250)  BLOCKED]
      pass layer:
        [3D_SCENE gltf_scene_id]
      → resolve
  [3D_BLIT external_tex_id]
  [FILL + LABEL 按钮]
  [3D_SYNC REINIT_GLVG]
```

**z-order = task 添加顺序 = 对象树 draw 遍历顺序。**

---

## 8. Widget ↔ Task 映射（修订）

| Widget | 提交的 Task | 备注 |
|--------|-------------|------|
| `lv_3dviewport` | `3D_VIEWPORT` + 子 task | 新；替代「仅 FBO widget」 |
| `lv_3dmesh` | `3D_MESH` | 父须在同一 VP pass 内 |
| `lv_3dscene` | `3D_VIEWPORT` + `3D_SCENE` 或展开多个 `3D_MESH` | |
| `lv_3dtexture` | **`3D_BLIT`** | 外部 tex；legacy |
| `lv_gltf` | `3D_VIEWPORT` + **`3D_SCENE`** | 迁移 off event 内 glDraw |
| builtin grid/axes | `3D_LINE` | `LV_USE_3DPRIM` |

**类关系（修订）：**

```text
lv_obj
  └─ lv_3dviewport_class          ← 新基类（VP + camera）
       ├─ lv_3dscene_class
       └─ lv_gltf_class            ← 从「继承 3dtexture」改为「继承 3dviewport」
lv_obj
  └─ lv_3dmesh_class              ← 逻辑节点，不参与 2D draw
lv_obj
  └─ lv_3dtexture_class           ← 保留，仅 BLIT 路径
```

> `lv_gltf` 继承改为 `3dviewport` 为 **G8.3** 迁移项；G8.0～G8.2 可与旧 gltf 并存。

---

## 9. DrawUnitG100 dispatch

| Task | Handler | 关键 GL 操作 |
|------|---------|--------------|
| `3D_VIEWPORT` | `lv_draw_g100_3d_viewport` | create/bind FBO, depth RB, set viewport, push pass |
| `3D_CLEAR` | `lv_draw_g100_3d_clear` | `glClear` |
| `3D_MESH` | `lv_draw_g100_3d_mesh` | program, uniform MVP, `glDrawElements` |
| `3D_LINE` | `lv_draw_g100_3d_line` | line shader |
| `3D_SCENE` | `lv_draw_g100_3d_scene` | 遍历 scene → batch mesh |
| `3D_CALLBACK` | `lv_draw_g100_3d_cb` | 调 cb，检查 GL 状态 |
| `3D_BLIT` | `lv_draw_g100_3d_blit` | 现有 `lv_opengles_render_texture` |
| `3D_SYNC` | `lv_draw_g100_3d_sync` | `lv_opengles_reinit_state()` |

**文件（规划）：**

```text
draw/g100/
├── lv_draw_g100_3d_blit.c      # 自现有 lv_draw_g100_3d.c  rename
├── lv_draw_g100_3d_viewport.c
├── lv_draw_g100_3d_clear.c
├── lv_draw_g100_3d_mesh.c
├── lv_draw_g100_3d_line.c
├── lv_draw_g100_3d_scene.c
├── lv_draw_g100_3d_cb.c
├── lv_draw_g100_3d_sync.c
├── lv_g100_mesh_cache.c
└── lv_g100_3d_pass.c           # pass 栈 / FBO 池
```

**evaluate：** 3D 族在 `!gl_ready` 时返回 0；**无 SW fallback**（与现 3D 一致）。

---

## 10. 配置宏

| 宏 | 默认 | 说明 |
|----|:----:|------|
| `LV_USE_3D_DRAW_TASKS` | 0 | 总开关（VP/MESH/SCENE 族） |
| `LV_USE_3DTEXTURE` | 0 | 含 `3D_BLIT` + `lv_3dtexture` widget |
| `LV_USE_3DVIEWPORT` | 0 | `lv_3dviewport` widget |
| `LV_USE_3DMESH` | 0 | mesh 对象 |
| `LV_USE_3DSCENE` | 0 | scene widget |
| `LV_USE_3DPRIM` | 0 | LINE3D builtin |
| `LV_USE_3D_LEGACY_ALIAS` | 1 | `LV_DRAW_TASK_TYPE_3D` → `3D_BLIT` |
| `LV_USE_3D_LEGACY_BLIT_ONLY` | 0 | 仅 BLIT，关闭 VP 族（兼容旧行为） |

---

## 11. 实施阶段（G6 修订 + G8）

| 阶段 | Draw Task | Widget / 迁移 | 用例 |
|:--:|-----------|---------------|------|
| **G6** | `3D_BLIT` + `3D_SYNC` | `lv_3dtexture`；gltf **暂保留** event 渲染 | D3-01～05 |
| **G8.0** | **`3D_VIEWPORT` + `3D_CLEAR`** | `lv_3dviewport` | D3-06, D3-16 |
| **G8.1** | **`3D_LINE` + `3D_CALLBACK`** | camera、grid | D3-07～08 |
| **G8.2** | **`3D_MESH`** | `lv_3dmesh` | D3-11 |
| **G8.3** | **`3D_SCENE`** | `lv_3dscene`；**gltf → SCENE task** | D3-12, D3-01 回归 |
| **G8.4～G8.6** | material/light/pick/theme | 同前规划 | D3-13～15 |

**依赖：** G8.2+ 需要 G1 `lv_g100_shader` 原生 GLES2（非 NVG 后端）。

---

## 12. 测试用例扩展

| ID | 阶段 | 检查点 |
|----|:--:|--------|
| D3-16 | G8.0 | apitrace：`3D_VIEWPORT` 内 `glBindFramebuffer`，resolve 后 `3D_BLIT` 或 layer blend |
| D3-17 | G8.0 | 同一帧 `FILL → 3D_VP → LABEL` task 顺序 |
| D3-18 | G8.2 | 两 `3D_MESH` 同一 VP 内 depth 正确 |
| D3-19 | G8.3 | gltf 无 widget event 内 `glDraw*`；仅 SCENE task |

---

## 13. 风险与约束

| 项 | 说明 |
|----|------|
| GL 状态 | 每个 VP 结束 `3D_SYNC`；G100 2D 仍用 NVG/GLES 交替 |
| 性能 | 多 VP = 多 FBO resolve；G7 FBO 池复用 |
| 线程 | 同 LVGL 单线程；task 队列非线程安全 |
| GLES2 | 无 compute；灯光数受 uniform 限制 |
| 迁移 | `lv_draw_3d()` alias 保留一版；gltf 迁移分步 |

---

## 14. 总结

| 问题 | 答案 |
|------|------|
| 3D 还是一张纹理吗？ | **否**（默认路径）；仅 **外部 tex** 走 `3D_BLIT` |
| 3D 谁执行 draw call？ | **DrawUnitG100**，不是 Widget 偷跑 |
| 2D/3D 如何同帧组合？ | **同一 layer task 队列**，VP 与 FILL/LABEL 交错 |
| 与 G8 关系？ | G8.0～G8.3 = 3D task 族 + widget **逐步实现** |
