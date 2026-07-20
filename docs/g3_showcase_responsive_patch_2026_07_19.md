# G3_SHOWCASE 响应式补丁 (2026-07-19, 17:00)

## TL;DR

把 `SCREEN_W`/`SCREEN_H` 从硬编码 `#define 1200 / 720` 改成由实际显示尺寸初始化，并在 `LV_EVENT_RESOLUTION_CHANGED` 时重新 build。这样不管板子是 800×480 (Orangepi) 还是 1200×720 (PC)，cards 都按真实屏幕尺寸布局，不会再推到屏幕外。

## 1. 改动

`src/demo_g3_showcase.c` 顶部：

```c
/* BEFORE — 硬编码 */
-#define SCREEN_W    1200
-#define SCREEN_H    720

/* AFTER — 运行时初始化，bind 同一变量名给 SCREEN_W/H */
+static int32_t g_screen_w = 1200;
+static int32_t g_screen_h = 720;
+#define SCREEN_W g_screen_w
+#define SCREEN_H g_screen_h
```

新 init 函数：

```c
static bool g_ui_built = false;
static lv_display_t * g_demo_disp = NULL;

static void resolution_changed_cb(lv_event_t * e)
{
    if(g_ui_built) return;
    lv_display_t * disp = lv_event_get_target(e);
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    if(w <= 0 || h <= 0) return;
    SCREEN_W = w;       // 写入 g_screen_w 变量
    SCREEN_H = h;
    g_demo_disp = disp;
    demo_g3_showcase_do_build(lv_screen_active());
}

void demo_g3_showcase_init(void)
{
    lv_display_t * disp = lv_display_get_default();
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);

    if(w > 0 && h > 0) {
        SCREEN_W = w;
        SCREEN_H = h;
        demo_g3_showcase_do_build(lv_screen_active());
    }
    lv_display_add_event_cb(disp, resolution_changed_cb,
                             LV_EVENT_RESOLUTION_CHANGED, NULL);
}
```

`build_ui(scr)` 内容不动；`page` 仍用 `lv_obj_set_size(page, SCREEN_W, SCREEN_H)` —— 但 `SCREEN_W/H` 现在是绑定到 `g_screen_w/h` 的宏，**改一个就全改了**。

## 2. 副作用

* `demo_g3_showcase_init` 现在**幂等**（`g_ui_built` 守卫）。重复调用不会重复 build。
* Wayland 上的早期 `LV_EVENT_RESOLUTION_CHANGED = 0` 触发问题被旁路了：首次 `init` 时若分辨率是 0，就 defer 给 callback；否则 inline build。
* `g_hover_trans` 仍是 static，但 `lv_style_transition_dsc_init` 在 `do_build` 里调用 —— 幂等防护确保只 init 一次。

## 3. 验证（待做 — 板子断了）

⚠️ **2026-07-19 17:00 板子离线** —— ping 100% loss，SSH 连接超时。等板子恢复后需要重新跑：

```bash
# 板子端（恢复网络后）
chmod 666 /run/user/0/wayland-0 /run/user/0/wayland-0.lock
cd /home/orangepi/lvglsim_cross
WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/0 \
  ./lvglsim_evgpu_only_g3 -b wayland -W 800 -H 480
```

预期：屏上能看到
* 顶部 "G3 UX SHOWCASE" 标题
* 4 个 cards：PLAYER（呼吸灯+EQ条）、WORLD（径向色块）、HOVER（3 个悬停 tile）、BOOT（圆按钮）
* card 之间的 ambient glow + world 径向渐变背景
* 黑边 vignette 暗角

如果屏上仍然几乎全黑，问题定位：
1. `LVGL_APP_DEMO` 没切到 `g3_showcase` → 用 `lvglsim_evgpu_only_g3` 不对，改用 `g3_showcase` demo binary
2. board 上的 binary 是这个新构建（要 scp 重传）

## 4. 之前的运行

修复前，板子（800×480）跑这个 binary 时观察到：cards 一个都不渲染，只看到 boot UI（标题/圆环/进度条）。原因是 grid 在 `1200×720` 内的 cell 坐标 ≈ 540×320，但显示只到 800×480，cards 大半部分超出 LVGL 的 `clip_area`。

修复后，grid 在 800×480 内算 → cells ≈ 350×190，全在屏内。

## 5. 文件

* `src/demo_g3_showcase.c` —— 92 行改动（含 `run_boot()` TRANSP fix 4 行 + 响应式 88 行）
* `src/evgpu_benchmark.c` —— `scene_act=12`，timer 禁用（保留早先改动）
* `docs/g3_showcase_root_cause_2026_07_19.md` — root cause（OPAQUE→TRANSP fix）
* `docs/g3_showcase_grid_bug_2026_07_19.md` — 第二个 bug（grid 越界）
* `docs/g3_showcase_responsive_patch_2026_07_19.md` — **本文档**
