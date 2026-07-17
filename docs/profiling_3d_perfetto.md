# 3D Demo 性能剖析：LVGL Profiler → Perfetto

> **目标**：用 LVGL 内置 profiler 抓取 3D demo 运行 5 秒的 trace，转成 systrace 导入
> [Perfetto](https://ui.perfetto.dev) 查看，并定位性能瓶颈。
> **环境**：WSL2 + WSLg（`WAYLAND_DISPLAY=wayland-0`），`configs/wayland-evgpu-profile`，`LVGL_APP_DEMO=3dscene`

---

## 1. 原理

LVGL 内置 trace（profiler）在关键函数插桩（refr / draw / layout / event / timer …），
按 Android **systrace**（`tracing_mark_write: B|E`）格式输出，可直接导入 Perfetto。

关键点（源码 `lvgl/src/misc/lv_profiler_builtin*.c`）：

- profiler 在 `lv_init()` 中**自动初始化**（需 `LV_USE_PROFILER=1`）。
- 开 `LV_USE_PROFILER_BUILTIN_POSIX=1` 时：
  - 时间戳用 `clock_gettime(CLOCK_MONOTONIC)`，**纳秒精度**（否则 `lv_tick_get()` 仅 1ms）；
  - trace 通过 `printf` 输出到 **stdout**；
- 环形缓冲写满时自动 flush；`LV_PROFILER_BUILTIN_BUF_SIZE` 越小 flush 越频繁（数据更全、抖动略大）。

---

## 2. 配置

新增 `configs/wayland-evgpu-profile.defaults`（基于 `wayland-evgpu` + 打开 profiler）。核心开关：

```
LV_USE_PROFILER 1
LV_USE_PROFILER_BUILTIN 1
LV_USE_PROFILER_BUILTIN_POSIX 1
LV_PROFILER_INCLUDE "lv_profiler_builtin.h"   # 见下方“坑”
LV_PROFILER_BUILTIN_BUF_SIZE (16 * 1024)
LV_LOG_LEVEL LV_LOG_LEVEL_WARN                 # 降噪，保持 stdout 干净
```

> **坑：`LV_PROFILER_INCLUDE`**
> 模板默认值是 `"lvgl/src/misc/lv_profiler_builtin.h"`，依赖“仓库根目录在 include 路径上”。
> 但子目标（如 thorvg、lvgl_linux）没有该 `-I`，会报
> `fatal error: ... lv_profiler_builtin.h: No such file or directory`。
> 改成**裸文件名** `"lv_profiler_builtin.h"` 即可：`#include LV_PROFILER_INCLUDE` 位于
> `lv_profiler.h`，裸名会相对该文件所在目录解析，公共头与 `src/misc` 两处都放着同名头，
> 所有目标都能找到。

---

## 3. 一键复现

```bash
./scripts/profile_3d_perfetto.sh 3dscene 5 800 480
#                                  demo   sec  W   H
# demo: 3dscene（默认）| 3dviewport | 3dview | gltf
```

脚本 `scripts/profile_3d_perfetto.sh` 做了：

1. `cmake -DCONFIG=wayland-evgpu-profile -DLVGL_APP_DEMO=<demo>` 并构建到 `build-evgpu-3dscene-profile/`；
2. `timeout <sec>` 运行，**stdout→trace**（`benchmark_logs/<demo>_trace.txt`），
   **stderr→sysmon/日志**（`benchmark_logs/<demo>_sysmon.log`）分离；
3. `lvgl/scripts/trace_filter.py` 过滤成 `benchmark_logs/<demo>_trace.systrace`；
4. `scripts/analyze_trace.py` 打印瓶颈汇总（按 total / self 时间排序）。

### 导入 Perfetto

打开 https://ui.perfetto.dev → 拖入 `benchmark_logs/3dscene_trace.systrace`。
UI 里 `A`/`D` 平移，`W`/`S` 缩放，点函数块看耗时。

---

## 4. 本次结果（3dscene，5 秒，800×480，WSLg）

抓取 **378,481** 个事件，trace 跨度 **4930 ms**；`lv_display_refr_timer` 触发 3633 次，
但真正有失效区、走到 flush 的只有 **139 次 → 有效 ~28 FPS**。

### Top by SELF time（真正的热点，不含子调用）

| 函数 | 调用 | self | 说明 |
|------|------|------|------|
| `call_flush_cb` | 139 | **319.6 ms** | 帧缓冲提交到 Wayland/EGL，单次 ~2.3ms，占比最高 |
| `wait_for_flushing` | 140 | **96.2 ms** | 等待上一帧 flush 完成（显示同步/背压） |
| `glevgr__convexFill` | 6291 | 58.9 ms | EVGPU 凸多边形填充（矢量/2D 元素） |
| `timer_cb` | 21926 | 57.4 ms | 定时器回调总开销（动画等） |
| `lv_opengles_render_draw` | 139 | 42.4 ms | GL 渲染提交 |
| `lv_draw_evgpu_fill` | 280 | 28.2 ms | EVGPU 填充 |
| `lv_draw_unit_draw_letter` | 7420 | 18.0 ms | 文字绘制 |
| `lv_draw_evgpu_3d_clear` | 140 | 17.0 ms | 3D clear pass |
| `lv_draw_evgpu_3d_mesh` | 280 | 12.2 ms | 3D mesh 光栅化 |
| `lv_draw_evgpu_3d_line` | 140 | 12.1 ms | 3D 网格线 |

### 结论

1. **瓶颈在显示 flush，而非 3D 绘制本身**：`call_flush_cb` + `wait_for_flushing`
   合计 **~416 ms self**（占 trace 跨度约 8.4%，且是每帧串行的关键路径），
   远超所有 3D draw task 之和（clear+mesh+line ≈ 41ms）。
2. 3D 绘制成本可控：单帧 3D（clear/mesh/line）约 `(17+12+12)/140 ≈ 0.3ms`。
3. 2D/矢量填充（`glevgr__convexFill` 58.9ms / 6291 次）与文字（18ms）是 CPU 侧次要热点。

> ⚠️ **环境注意**：本机 WSLg 的 EGL 初始化报 `MESA: ZINK: failed to choose pdev`
> （见 `benchmark_logs/3dscene_sysmon.log`），最终 swap 走了回退路径，
> 因此 flush 偏重、`SW` draw unit 也有参与。真实 GPU 环境下 flush 占比应显著下降。
> 修复 GPU 驱动（zink/dri）后重跑可得到更贴近硬件的画像。

---

## 4b. 真机结果（Orange Pi PC / Allwinner H3 / Mali-400 / GLES2）

在真板子上用 **wayland-egl + EVGPU** 重新构建并运行 3dscene（`configs/orangepi-evgpu.defaults`，
board 原生 gcc 编译）。这是真实 GPU 路径，不是 WSLg 的 zink 回退。

**GPU 确认**：`GL vendor: lima / GL renderer: Mali400 / OpenGL ES 2.0 (GLSL ES 1.0)`。
探针确认支持 `GL_OES_depth_texture`、`GL_OES_depth24`、`GL_OES_packed_depth_stencil`、
`GL_OES_texture_npot`、`GL_OES_element_index_uint` 等 —— EVGPU 的深度纹理 FBO 可用。

**3dscene 是否超 GLES2？否。** 运行 8 秒：viewport/clear/line 各 208、mesh 416（floor+cube×208 帧），
无 GL 错误、无 FBO incomplete、无 swrast/llvmpipe 回退 → 在 Mali-400 上 EVGPU 正常渲染，
**有效 ~26 FPS**。因此无需单独的 GLES2-only 场景。

### Mali-400 上的真实热点（self time）

| 函数 | self | 说明 |
|------|------|------|
| `glevgr__convexFill` | 409 ms | EVGPU 2D 凸填充（面板/背景） |
| `lv_draw_unit_draw_letter` | 332 ms | **文字字形 CPU 光栅化** |
| `wait_for_flushing` | 282 ms | 等 GPU/合成器 flush（背压） |
| `lv_draw_evgpu_fill` | 163 ms | EVGPU 填充 |
| `call_flush_cb` | 154 ms | 帧缓冲提交 |
| `draw_letter_bitmap` | 142 ms | 字形位图 blit |
| `glevgr__renderFlush` | 119 ms | GL 命令 flush |
| **`lv_draw_evgpu_3d_mesh`** | **87 ms** | **真正的 3D mesh 渲染（很小）** |

**结论（与 x86/WSLg 完全不同）**：在 Mali-400 + Cortex-A7 上，瓶颈是 **2D 填充 + 文字 CPU 光栅化**，
**3D 本身很便宜**。这正是“在目标硬件上 profiling”才能得到的真实画像 —— 弱 CPU 让 2D/文字成为主导，
而不是 3D 或 flush。

> 复现：`configs/orangepi-evgpu.defaults`（EVGPU+wayland-egl，GLTF/ThorVG 关，profiler 开），
> 板上 `cmake -B build-evgpu -DCONFIG=orangepi-evgpu -DLVGL_APP_DEMO=3dscene && cmake --build build-evgpu -j3`，
> 在 root weston 下 `sudo env XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0 timeout 8 ./bin/lvglsim -b wayland`。
> 产物 systrace：`benchmark_logs/board_3dscene.systrace`。

---

## 5. 分析脚本

`scripts/analyze_trace.py <file.systrace> [top_n]`：解析 B/E 事件配对，按线程栈计算每个
函数的 **调用次数 / total（含子）/ self（不含子）/ 平均**，并估算 refr 帧数与跨度。
不依赖 Perfetto，可在 CI/终端快速看瓶颈。

---

## 6. 常见问题

| 现象 | 处理 |
|------|------|
| Perfetto 解析失败/不全 | trace 被其它日志插入污染 → 已用 stdout/stderr 分离；确保只喂 `.systrace` |
| 函数耗时显示 0s | 时间戳精度不足 → 已用 POSIX 纳秒时钟 |
| trace 事件很少 | 缓冲没写满且未手动 flush → 调小 `LV_PROFILER_BUILTIN_BUF_SIZE`，或延长运行时间 |
| 尾部数据缺失 | `timeout` 杀进程时最后一段缓冲未 flush（可接受）；需要完整可加退出时 `lv_profiler_builtin_flush()` |

---

## 参考

- LVGL profiler 文档：`lvgl/docs/src/debugging/profiler.mdx`
- 过滤脚本：`lvgl/scripts/trace_filter.py`
- 本项目脚本：`scripts/profile_3d_perfetto.sh`、`scripts/analyze_trace.py`
- 配置：`configs/wayland-evgpu-profile.defaults`
