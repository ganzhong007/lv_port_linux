# lvgl 子模块代码目录说明

> 基于当前 `wsl_wayland_3d` 分支上的 `lvgl` 子模块结构整理。  
> 主仓路径：`lv_port_linux/lvgl/`（git submodule）

## 结论（先说）

**不完全是。** `lvgl/src` 是 LVGL **库实现**的主目录，但子仓里“有效、会被构建或对外使用的代码/头文件”还分布在其它目录。

若指：

- **库核心逻辑** → 主要在 `lvgl/src/`
- **完整可构建产物** → `src/` + `include/lvgl/` +（可选）`demos/`、`examples/` + 构建脚本 `env_support/`、`scripts/`

应用层代码（如 `main.c`、`simple_button.c`、Wayland 封装）在**主仓** `lv_port_linux/src/`，不在子仓内。

---

## 顶层目录一览

```
lvgl/
├── src/           # 库实现（.c / .cpp）— 核心
├── include/lvgl/  # 公共 API 头文件（新结构）
├── demos/         # 官方 demo（stress、widgets 等）
├── examples/      # 官方示例
├── env_support/   # CMake / 各平台移植
├── scripts/       # lv_conf 生成、预处理等
├── tests/         # 单元测试
├── docs/          # LVGL 文档源
├── configs/       # Kconfig / defconfig
└── lv_conf_template.h 等配置模板
```

---

## CMake 构建时编译哪些目录

来源：`lvgl/env_support/cmake/main.cmake`

| 目录 | 内容 | 是否进入 `lvglsim` 链接 |
|------|------|-------------------------|
| **`src/`** | 库实现：core、draw、drivers、widgets、indev、libs（NanoVG、ThorVG 等） | ✅ 始终 → `liblvgl` |
| **`include/lvgl/`** | 对外头文件 | ✅ include 路径（无 `.c`） |
| **`demos/`** | `lv_demo_stress`、`lv_demo_widgets` 等 | ✅ 可选（`LV_BUILD_DEMOS=1`） |
| **`examples/`** | 控件/特性示例 | ✅ 可选（`LV_BUILD_EXAMPLES=1`） |
| **`env_support/`** | CMake 模块、ESP/QNX 等移植 | 构建系统，非 UI 运行时逻辑 |
| **`scripts/`** | Python：生成 `lv_conf.h`、预处理等 | 配置阶段使用 |
| **`tests/`** | 自动化测试 | 仅 `cmake --build` 测试目标时 |

核心规则（摘录）：

```cmake
file(GLOB_RECURSE SOURCES ${LVGL_ROOT_DIR}/src/*.c ...)
file(GLOB_RECURSE EXAMPLE_SOURCES ${LVGL_ROOT_DIR}/examples/*.c)
file(GLOB_RECURSE DEMO_SOURCES ${LVGL_ROOT_DIR}/demos/*.c)
add_library(lvgl ${SOURCES})
```

ThorVG 等 C++ 源码在 `src/libs/thorvg/`，单独编成 `liblvgl_thorvg` 再链接。

---

## `src/` 与 `include/lvgl/` 的关系

当前为**双轨**结构（迁移中）：

| 位置 | 角色 |
|------|------|
| **`src/`** | 函数/模块**实现**（`.c`），以及部分旧头文件（含 deprecated 提示，指向 `include/lvgl/...`） |
| **`include/lvgl/`** | 推荐使用的**公共 API** 头文件 |

示例：

- Wayland 驱动：`src/drivers/wayland/` + 对外声明在 `include/lvgl/drivers/...`
- 绘制：`src/draw/` + `include/lvgl/draw/lv_draw.h`
- 3D 纹理 widget：`src/widgets/3dtexture/` + `include/lvgl/widgets/lv_3dtexture.h`

阅读 API 优先看 **`include/lvgl/`**；追实现进 **`src/`** 对应子目录。

---

## `src/` 下 16 个子目录详解

`lvgl/src/` 根目录除下列 **16 个文件夹** 外，还有若干**根级文件**（不属于子目录，但同属 `src` 实现层）：

| 根级文件 | 作用 |
|----------|------|
| `lv_init.c` / `lv_init.h` | 库初始化入口 `lv_init()`，注册子系统 |
| `lvgl.h` / `lvgl_private.h` / `lvgl_public.h` | 聚合头文件、内部/对外 include 边界 |
| `lv_conf_internal.h` | 由 `lv_conf.h` 展开的配置宏（编译期开关） |
| `lv_api_map_v8.h` … `v9_5.h` | 旧版 API 名到新版的兼容映射 |

---

### 16 个文件夹总表

| # | 目录 | 层级角色 | 主要职责 | 典型内容 / 关键路径 | 与本仓库关系 |
|---|------|----------|----------|---------------------|--------------|
| 1 | **`core/`** | L1 核心 | 对象树、事件、样式、刷新调度 | `lv_obj*.c` 对象模型；`lv_refr.c` 脏区合并与刷新定时器；`lv_group.c` 焦点组 | 所有 widget 与 demo 的基础；点击 → invalidate → refr 均经此层 |
| 2 | **`display/`** | L1 显示抽象 | 屏幕/图层生命周期、分辨率、flush 回调 | `lv_display.c`：`lv_display_create()`、buffer 模式、layer 链表 | 连接 `drivers/wayland` 与上层 draw；`lvglsim -W -H` 改的就是 display 分辨率 |
| 3 | **`indev/`** | L1 输入 | 指针、键盘、encoder、手势 | `lv_indev.c` 轮询与命中测试；`lv_gridnav.c` 网格导航 | Wayland 后端创建的 pointer/keyboard 最终进 `lv_indev_read` |
| 4 | **`draw/`** | L2–L3 绘制 | Draw task 管线、各 draw unit、图像解码 | `lv_draw.c` 任务调度；`sw/` CPU 绘制；`nanovg/` NanoVG；`opengles/`；`lv_draw_3d.c` | **wayland-egl** 走 `draw/nanovg` + `drivers/opengles`；**wayland-shm** 走 `draw/sw` |
| 5 | **`drivers/`** | L5–L6 平台 | 对接 OS/硬件：显示、输入、GPU | `wayland/` SHM/EGL 窗口；`opengles/` EGL 上下文与纹理；`evdev/` 输入 | 本仓库 WSLg 核心：`drivers/wayland` + `drivers/opengles` |
| 6 | **`widgets/`** | L4 控件 | 内置 UI 组件实现 | `button/`、`label/`、`3dtexture/` 等 39 个子目录；每控件 `*_class` + 事件 | `simple_button` 用的 button/label 即在此 |
| 7 | **`layouts/`** | L4 布局 | Flex / Grid 等布局算法 | `flex/`、`grid/`、`lv_layout.c` | 容器内子对象排列；stress demo 里 list/flex 场景会用到 |
| 8 | **`font/`** | L3 资源 | 字体加载与内置字库 | 大量 `lv_font_montserrat_*.c`；`fmt_txt/` 文本格式；`freetype/` 接口 | Label 绘制时的 glyph 来源 |
| 9 | **`themes/`** | L4 外观 | 默认/ mono / simple 主题 | `default/` 等：统一样式、颜色、padding | 未自定义 style 时控件默认外观 |
| 10 | **`misc/`** | 横切工具 | 动画、区域、颜色、链表、矩阵、缓存等 | `lv_anim.c`、`lv_area.c`、`lv_color.c`、`lv_timer.c`、`cache/` | 全库共用基础设施 |
| 11 | **`stdlib/`** | 横切内存 | 内存、字符串、sprintf 抽象 | `lv_mem.c`；`clib/` / `builtin/` 等后端 | `LV_USE_STDLIB_*` 配置决定 malloc 实现 |
| 12 | **`osal/`** | 横切 OS | 线程、互斥、延迟（多 RTOS/OS） | `lv_linux.c`、`lv_freertos.c`、`lv_pthread` 等 | Linux/WSL 构建通常走 `lv_linux` / pthread |
| 13 | **`tick/`** | 横切时间 | 毫秒 tick，`lv_tick_get()` | `lv_tick.c` | `lv_timer_handler()` 与动画时间基准 |
| 14 | **`libs/`** | L3 内嵌库 | 第三方与编解码器源码 | `nanovg/`、`thorvg/`、`gltf/`、`lodepng/`、`freetype/` 等 26 个子目录 | EGL 路径依赖 `libs/nanovg`；`LV_USE_GLTF` 用 `libs/gltf` |
| 15 | **`debugging/`** | 调试/测试 | 运行时诊断与测试辅助 | `sysmon/` FPS/CPU 监控（stress 日志里的 `sysmon:`）；`monkey/` 随机测试 | stress 对比脚本解析的 FPS 即 **sysmon** 输出 |
| 16 | **`others/`** | 扩展功能 | 非核心但可选的「其他」模块 | `file_explorer/`、`fragment/`、`translation/` | 一般 demo 默认不依赖；按 `lv_conf` 开关 |

---

### 按数据流串联（便于记忆）

```
indev/ ──事件──► core/ ──invalidate──► draw/ ──像素──► display/ ──flush──► drivers/
                ▲                           ▲
         widgets/ + layouts/          libs/ + font/
                ▲
           themes/ + misc/ + tick/ + stdlib/ + osal/
```

- **输入**：`drivers/` 采集 → `indev/` 分发 → `core/` 命中与事件
- **输出**：`widgets/` 改状态 → `core/` refr → `draw/` 生成 task → `display/` flush → `drivers/` 送 Wayland/WSLg
- **debugging/sysmon** 挂在刷新链路上报 FPS（stress 测试数据来源）

---

### 各目录子模块速查（二级）

| 目录 | 重要子目录 / 文件 |
|------|-------------------|
| `core/` | `lv_obj*.c`、`lv_refr.c`、`lv_event.c`、`lv_group.c` |
| `draw/` | `sw/`（CPU）、`nanovg/`、`opengles/`、`lv_draw_rect/label/image*.c`、`lv_image_decoder.c` |
| `drivers/` | `wayland/`、`opengles/`、`evdev/`、`sdl/`、`drm/`、`x11/` |
| `widgets/` | 每控件一目录：`button/`、`label/`、`3dtexture/`、`chart/` … |
| `libs/` | `nanovg/`、`thorvg/`、`gltf/`、`freetype/`、`lodepng/`、`libwebp/` |
| `debugging/` | `sysmon/`（性能监控）、`test/`（内部测试桩） |
| `font/` | 内嵌 Montserrat 等 `.c` 字库 + `fmt_txt/` |
| `layouts/` | `flex/`、`grid/` |
| `themes/` | `default/`、`mono/`、`simple/` |

---

## `src/` 内主要子目录（库本体）— 简表

| 子目录 | 说明 |
|--------|------|
| `core/` | 对象模型、事件、刷新 `lv_refr` |
| `draw/` | 2D/3D 绘制任务、NanoVG/OpenGLES/SW 等 draw unit |
| `display/` | `lv_display_t` 显示抽象 |
| `indev/` | 输入设备 |
| `drivers/` | Wayland、DRM、SDL、OpenGLES 等平台驱动 |
| `widgets/` | 内置控件实现 |
| `libs/` | 内嵌第三方：NanoVG、ThorVG、GLTF 等 |
| `font/`、`layouts/`、`misc/`、`osal/` | 字体、布局、工具、OS 抽象 |
| `lv_init.c` | 库初始化入口 |

---

## 与本仓库 `lv_port_linux` 的分工

| 仓库路径 | 职责 |
|----------|------|
| **`lvgl/`（子模块）** | LVGL 库、demo、examples、驱动实现 |
| **`lv_port_linux/src/`** | 应用入口 `main.c`、simple button demo、backend 选择封装 |
| **`lv_port_linux/configs/`** | wayland / wayland-egl 等**主仓**构建配置 |
| **`lv_port_linux/scripts/`** | WSL Wayland 构建、stress 对比脚本等 |

构建 `lvglsim` 时：主仓 CMake 拉子模块，按 `-DCONFIG=wayland` 等生成 `lv_conf.h`，再编译子模块 `lvgl` + 主仓 `src/`。

---

## 常见误解

1. **“所有代码都在 `src/`”** — 错。公共头在 `include/lvgl/`；stress 等 demo 在 `demos/`。
2. **“子仓就是整个可执行程序”** — 错。可执行文件由主仓 `src/main.c` 等与子模块 `liblvgl` 链接而成。
3. **“`src/widgets` 与 `include/lvgl/widgets` 重复”** — 前者是实现，后者是 API；二者成对出现。

---

## 快速定位建议

| 想查… | 去看… |
|--------|--------|
| 某 API 怎么用 | `include/lvgl/` |
| 某功能怎么实现 | `src/` 同名模块 |
| stress / widgets demo | `demos/` |
| Wayland SHM / EGL 后端 | `src/drivers/wayland/` |
| NanoVG 绘制 | `src/draw/nanovg/` |
| 构建选项从哪来 | 主仓 `configs/*.defaults` → 生成 `lv_conf.h` |
