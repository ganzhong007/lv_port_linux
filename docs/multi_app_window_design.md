# 单 LVGL 实例多应用窗口设计

> **文档状态：** 设计记录（尚未落地代码）  
> **背景：** 一个 LVGL 实例承载多个应用的显示；每个应用至少有一个应用窗口；窗口需支持截屏，并形成「正在运行应用」列表  
> **关联：** `lv_snapshot`（见 `src/simple_button_shot.c`）、LVGL 9 `lv_win` / Display Layer API

---

## 1. 结论（选型）

| 方案 | 定位 | 是否作为应用窗口载体 |
|------|------|----------------------|
| **`lv_win`** | 标题栏 + 内容区的 UI 壳 | **否**（可复用为 chrome） |
| **Display Layer**（`top` / `sys` / `bottom`） | 全局壁纸、系统栏、模态、光标 | **否**（层数固定，非每应用一层） |
| **自定义 `lv_app_window`（继承 `lv_obj`）** | 应用窗口语义 + 焦点/z-order/截屏 | **是（主路径）** |

**推荐组合：**

- **核心：** 自定义 `lv_app_window` 容器 + Shell/Window Manager（C 模块）维护运行列表  
- **全局壳：** Display Layer 仅放壁纸 / 全局弹窗 / 光标  
- **可选：** 内部用 `lv_win` 或自写 titlebar 做窗口装饰  

不要用「每个应用一个 `lv_screen` + `lv_screen_load`」来做多应用并行：那是手机式单全屏切换，不适合任务列表与多窗口共存。

---

## 2. 为什么不直接用 `lv_win`

`lv_win` 实现很薄（见 `lvgl/src/widgets/win/lv_win.c`）：

- 固定结构：`header` + `content`（flex column）
- API：`lv_win_add_title` / `lv_win_add_button` / `lv_win_get_header` / `lv_win_get_content`
- 默认尺寸接近父对象 100%，更像全屏面板

**不提供：** 拖拽/缩放、最小化最大化、多窗口 z-order、焦点策略、应用生命周期、任务列表。

**用法：** 可作为 `lv_app_window` 内部的 chrome 组件；「应用窗口」这一层必须自定义。

---

## 3. Layer 两套概念（勿混用）

### 3.1 Display Layer（UI 全局层）

每个 display 固定三层：

| API | 用途建议 |
|-----|----------|
| `lv_display_get_layer_bottom` | 桌面壁纸 |
| `lv_display_get_layer_top` | 全局通知、下拉菜单、模态 |
| `lv_display_get_layer_sys` | 光标、调试 overlay |

输入命中顺序：`sys → top → active screen → bottom`。

**不适合** 把 N 个应用窗口挂在这些层上——层不是「每个应用一层」。

### 3.2 `lv_layer_t`（绘制系统）

Draw unit / 渲染缓冲概念（3D viewport、OpenGL 等），与窗口管理无关。

---

## 4. 推荐架构

```text
Display
├── bottom_layer          # 壁纸
├── lv_screen_active()    # Desktop（唯一主屏）
│   ├── lv_app_window #1
│   │   ├── chrome（标题栏，可选复用 lv_win）
│   │   └── content_root（应用 UI）
│   ├── lv_app_window #2
│   │   ├── chrome
│   │   └── content_root
│   ├── taskbar / launcher（可选）
│   └── …
├── top_layer             # 全局弹窗
└── sys_layer             # 光标 / 调试
```

Shell（普通 C 模块，非 widget）负责：

- 窗口注册表（正在运行应用列表）
- 焦点与 z-order
- 任务栏 / Alt-Tab 式切换的数据源
- 生命周期（create / focus / minimize / close）

---

## 5. `lv_app_window` 设计要点

### 5.1 建议数据结构

```c
typedef struct {
    lv_obj_t obj;
    lv_obj_t * chrome;        /* 标题栏 */
    lv_obj_t * content;       /* 应用 UI 根节点 */
    const char * app_id;
    const char * title;
    lv_draw_buf_t * thumb;    /* 任务列表缩略图缓存 */
    uint8_t state;            /* normal / minimized / maximized */
} lv_app_window_t;
```

### 5.2 关键行为

| 行为 | 实现要点 |
|------|----------|
| 浮动定位 | 创建时加 `LV_OBJ_FLAG_FLOATING`，脱离父 layout，`lv_obj_set_pos/size` |
| 置顶 / 焦点 | 点击 chrome → `lv_obj_move_foreground()` + Shell 记录焦点 |
| 最小化 | `lv_obj_add_flag(win, LV_OBJ_FLAG_HIDDEN)`，仍留在运行列表 |
| 内容隔离 | 应用只往 `content` 挂子对象，不直接操作 desktop |
| 关闭 | 从 registry 移除 → `lv_obj_delete` → 释放 `thumb` |

### 5.3 运行列表（Shell registry）

```c
typedef struct {
    lv_app_window_t * win;
    void (*on_close)(void *);
    void * app_ctx;
} running_app_t;

/* 全局链表/数组：add / remove / focus / enumerate */
```

任务栏、最近应用、Dock 遍历 registry，**不要**靠遍历整棵 obj 树推断「正在运行」。

---

## 6. 截屏与任务列表缩略图

与容器选型无关：用 `lv_snapshot`（需 `LV_USE_SNAPSHOT`）。

仓库已有整屏示例：`src/simple_button_shot.c` 对 `lv_screen_active()` 做 `lv_snapshot_take`。

| 用途 | 截取对象 | 注意 |
|------|----------|------|
| 任务列表缩略图 | `app_window->content`（或整个 `app_window`） | 只含该应用，不含兄弟窗口 |
| 整屏截图 | `lv_screen_active()` / desktop | 含所有可见窗口 |
| 后台缩略图 | `HIDDEN` 窗口仍可 snapshot | 成本高，按需/降频 |

建议：

- 复用 buffer：`lv_snapshot_create_draw_buf` + `lv_snapshot_take_to_draw_buf`
- 触发时机：失焦、内容变更、最小化等；**不要每帧截**
- 缩略图可降分辨率（如 1/4）以控内存与 CPU

---

## 7. 输入焦点与其它设计点

1. **输入焦点：** 每应用一个 `lv_group`；切窗口时切换 default group（或显式把 indev 绑到目标 group）  
2. **布局模式：** 层叠窗口用 FLOATING + 手动几何；平铺可另做 layout 策略，仍由 Shell 驱动  
3. **性能：** 窗口数与 snapshot 频率是主要成本；thumb 宜缓存、按需刷新  
4. **Screen 策略：** 保持单一 desktop screen；多应用 = 多浮动窗口，而非多 screen 切换  

---

## 8. 推荐模块边界（落地时）

```text
Shell (C 模块)
├── desktop = lv_screen_active()
├── window registry
├── taskbar / launcher UI
└── focus & z-order 策略

lv_app_window (自定义 widget, base = lv_obj_class)
├── chrome
├── content
├── app_window_snapshot_thumb()
└── events: CLOSE / MINIMIZE / FOCUS

Display layers（仅全局）
├── bottom_layer: 壁纸
├── top_layer: 全局 dialog
└── sys_layer: 光标
```

---

## 9. 明确不做 / 误用清单

| 误用 | 原因 |
|------|------|
| 每个应用挂在 `top_layer` | 层固定、非窗口管理器 |
| 直接用 `lv_win` 当应用窗口 | 无生命周期、无任务列表、无焦点策略 |
| 用 `lv_layer_t` 管应用窗口 | 那是绘制层，不是 UI 容器 |
| 每应用一个 `lv_screen` 来「多开」 | 无法自然并存与任务切换 |

---

## 10. 后续可落地项（未实现）

- [ ] `lv_app_window` 最小骨架：create / focus / minimize / close / snapshot  
- [ ] Shell registry + 简单任务列表 UI  
- [ ] 标题栏拖拽与 z-order  
- [ ] 缩略图缓存策略与降分辨率  

---

## 参考源码

| 内容 | 路径 |
|------|------|
| `lv_win` 实现 | `lvgl/src/widgets/win/lv_win.c` |
| `lv_win` API | `lvgl/include/lvgl/widgets/lv_win.h` |
| Display Layer getter | `lvgl/src/display/lv_display.c`（`lv_display_get_layer_*`） |
| Snapshot API | `lvgl/include/lvgl/draw/lv_snapshot.h` |
| 现有截屏示例 | `src/simple_button_shot.c` |
