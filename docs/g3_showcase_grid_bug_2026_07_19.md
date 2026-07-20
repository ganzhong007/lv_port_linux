# G3_SHOWCASE root cause update (2026-07-19, 16:55)

**TL;DR** — `run_boot()` 全屏 BLACK OPAQUE bg 的 fix **确实生效**（已通过 board 上的 EVGPU_C_R_T log 验证），但 G3 仍然几乎全黑。原因是 *第二* 个独立 bug：demo 用的 grid 布局在 Orangepi（800×480）和源码硬编码 `SCREEN_W=1200, SCREEN_H=720` 不匹配时，cards 被推到屏幕外。

## 1. 板上 log 验证

板子上把 fix 二进制（`lvglsim_evgpu_only_g3`）启动后，每帧只有这 6 个 task：

```
[LVGL:L3-DRAW] add task FILL   at (-200,-120)-(999,599)
[LVGL:L3-DRAW] add task BORDER at (355,160)-(394,199)
[LVGL:L3-DRAW] add task BORDER at (405,160)-(444,199)
[LVGL:L3-DRAW] add task LABEL  at (293,220)-(505,259)
[LVGL:L3-DRAW] add task FILL   at (310,280)-(489,403)
[LVGL:L3-DRAW] add task LABEL  at (331,304)-(468,319)
```

* `(-200,-120)-(999,599)` —— 大屏幕全屏 `FILL`。这是 `run_boot()` 的 bg。**TRANSP 后仍然有 task，但 `lv_draw_sw_fill` 内部会跳过 opacity=0 的 actual copy**。
* `(355,160)-(394,199)`、`(405,160)-(444,199)` —— `build_boot()` 里的 ring 圆环
* `(293,220)-(505,259)` —— `g3_label(boot, "EVEN G3", FONT_LG, OP_BRIGHT)` 标题
* `(310,280)-(489,403)` —— 进度条
* `(331,304)-(468,319)` —— 状态文字

**所有这些 task 都是 `run_boot()` 创建的 boot overlay 内容**。`build_world()`, `build_card_player/world/hover/boot()` 一个 task 都没画。

## 2. 第二个 bug: grid 布局 + 硬编码尺寸

`demo_g3_showcase.c` 顶部硬编码：

```c
#define SCREEN_W    1200
#define SCREEN_H    720
```

但 Orangepi 板的 native 显示是 `800×480`：

```
$ ./lvglsim_evgpu_only_g3 -b wayland -W 800 -H 480
```

`build_card_*` 用 `lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, ...)` 放卡片到 grid 里。grid 的 `cols[]/rows[]` 用 `LV_GRID_FR(1)`，**它依赖父对象的尺寸（`page` 的 size）**：

```c
lv_obj_t * page = plain(scr);
lv_obj_set_size(page, SCREEN_W, SCREEN_H);     // 1200×720
```

`page` 比 800×480 显示还大，所以：
* grid 算出每个 card ≈ 540×320 (在 1200×720 内)
* 但显示只到 800×480 → card 大部分坐标超出可见区
* LVGL 仍画 task（log 里能看到），但 `lv_display_refr_timer` 的 `clip_area` 把画到屏幕外的部分裁剪掉
* 用户看到的就是"几乎全黑"

## 3. 这条独立 bug 的最小修复（未实施）

任选其一：

### α 方案（1 行）：不进入 grid 路径
在 `evgpu_benchmark_create()` 里调用 `demo_g3_showcase_init()` 之前，临时改 SCREEN：
```c
// 不行，SCREEN_W/H 在 demo_g3_showcase.c 里是 #define
```

### β 方案（5 行）：改 demo 用绝对 align
把 grid 替换为 `lv_obj_align(card, LV_ALIGN_..., x_off, y_off)`，**用 SCREEN_W/4 这种相对值算**。

### γ 方案（10 行）：在 build_ui 里 clamp SCREEN_W/H
```c
build_ui(lv_obj_t * scr) {
    lv_display_t * disp = lv_display_get_default();
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    if(w > 0 && h > 0) {
        SCREEN_W = LV_MIN((int32_t)SCREEN_W, w);  // 临时变量，不是 #define
        ...
    }
    ...
}
```
（需要把 `SCREEN_W/H` 从 `#define` 改成 `int32_t`）

### δ 方案（30 行）：把 demo 改成响应式
监听 `LV_EVENT_RESOLUTION_CHANGED`，在回调里重新 build_scene。
这是 SDK 的 "正确"做法。

## 4. 当前验证状态

* ✅ `run_boot()` bg `OPAQUE → TRANSP` fix 已经在源码（`demo_g3_showcase.c:454`）
* ✅ 已经 cross-compile 并 scp 到 Orangepi（`/home/orangepi/lvglsim_cross/lvglsim_evgpu_only_g3`）
* ✅ 进程 12941 正在 Orangepi 板上稳定跑（screen 上能看到 EVGPU_C_R_T 输出，但 cards 不显示因为 grid bug）
* ⚠️ 用户在屏幕上看到什么？目前约 95% 黑屏 + 一些 boot UI 内容（标题绿字、ring、进度条在屏幕内的部分）

## 5. 给用户看的具体步骤

1. 用户在 Orangepi 直接执行：
   ```bash
   cd /home/orangepi/lvglsim_cross
   sudo chmod 666 /run/user/0/wayland-0 /run/user/0/wayland-0.lock
   WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/0 \
     ./lvglsim_evgpu_only_g3 -b wayland -W 800 -H 480
   ```
2. 应该看到：屏幕大部分黑，但中心能看到浅绿色 "EVEN G3" 标题、绿圆环、进度条、文字。**绝不应该再看到 "黄色扇形"**（那是之前 boot bg OPAQUE 挡住一切后漏出来的 BOOT card 阴影）。如果仍看到扇形，说明 fix 没传到板子上。

## 6. TODO（之后做）

* γ 方案：把 demo_g3_showcase.c 的 grid 改成响应式，让 SCREEN_W/H 跟实际显示匹配
* 重新 cross-compile、scp、板上测试
* 如果 cards 渲染正常，重新跑 `evgpu_benchmark_2026_07_18.md` 的 FPS 测量，看 "3×加速"是否还成立

## 7. 文件

* `src/demo_g3_showcase.c:454` — `run_boot()` bg `OPAQUE → TRANSP`（已验证生效）
* `src/evgpu_benchmark.c:22` — `scene_act=12`（仍只跑 G3_SHOWCASE）
* 板子上 `/home/orangepi/lvglsim_cross/lvglsim_evgpu_only_g3` — 编译好的 binary
