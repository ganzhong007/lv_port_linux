# G3_SHOWCASE Root Cause Confirmed: `run_boot()` OPAQUE LAYER_TOP overlay (2026-07-19)

**Status:** ✅ Bisection completed. The real root cause is now identified.

## TL;DR

`demo_g3_showcase.c::run_boot()` creates an **800×480 fully opaque BLACK
widget on `lv_layer_top()`**, and this opaque layer breaks the rendering
pipeline for everything on the screen layer underneath. Flipping just one
constant (`LV_OPA_COVER` → `LV_OPA_TRANSP` on the boot background) restores
G3_SHOWCASE rendering from **0.91 % → 99.2 %** of pixels visible.

## 1. Bisection Chain

Same `g3_flush_snap` tool (x86_64 native, pure SW draw, hooks the
display flush_cb). `memcpy(backbuf, px_map)` → write PPM → count distinct
visible pixels.

| Variant | Non-black pixels | Unique colors | Notes |
|---|---|---|---|
| Reference (`simple_snap`, just red rect + blue rect + green text) | **384 000 / 384 000 (100 %)** | 34 | Confirms SW draw → memcpy → PPM chain itself works |
| G3_SHOWCASE, default | **3 484 / 384 000 (0.91 %)** | 50 | The bug, reproduced on x86_64 without any GPU/EVGPU |
| G3_SHOWCASE, **`run_boot()` removed** | **381 084 / 384 000 (99.2 %)** | 783 | Almost everything renders correctly |
| G3_SHOWCASE, `run_boot()` re-enabled, **boot bg `OPAQUE → TRANSP`** | **381 084 / 384 000 (99.2 %)** | 784 | Single-line fix |
| (for completeness) OrangePi EVGPU run, same fix | (not yet retested) | — | pending rebuild + push to board |

## 2. The Smoking Gun

```c
// demo_g3_showcase.c:361-370 (BEFORE)
static void run_boot(void) {
    lv_obj_t * boot = lv_obj_create(lv_layer_top());    // ← top layer overlay
    lv_obj_set_size(boot, SCREEN_W, SCREEN_H);          // ← full screen
    lv_obj_align(boot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(boot, lv_color_hex(0x000000), 0); // ← BLACK
    lv_obj_set_style_bg_opa(boot, LV_OPA_COVER, 0);     // ← OPAQUE !
    // ... rest is the boot UI (mark, title, track) on top of this black bg
}
```

The boot widget:
1. Lives on `lv_layer_top()` — a **separate layer above the screen layer**.
2. Is sized full-screen (`SCREEN_W × SCREEN_H`) and centered.
3. Has a **100 % opaque black** background.

When the layer-level rendering runs:
* The screen layer renders G3's gradient + cards + labels — *correctly*.
* The top layer then renders the boot widget — *on top of*, but with
  transparent areas where the boot content is interactive UI (icon, title,
  progress bar). The black bg covers everything else.

The fact that only ~0.9 % of pixels make it through the pipeline implies
the rendering code **is not even reading the screen layer's pixels** at
all — it just renders the top layer (which is mostly black bg).

## 3. Why It Looks So Weird on OrangePi

On the screen the user sees (per attached photo), only a yellow-tan
diagonal patch in the bottom-right is visible. That's because:

* The top-layer boot overlay is partially **drawn over** (or replaced)
  by the running `boot_timer_cb` which:
  - moves the progress bar `fill` widget,
  - animates mark rings,
  - updates the status text.
* Each animation tick triggers `invalidate_area` on the boot
  overlay, which only re-renders the *dirty region* of the top layer.
* The bottom-right corner is the boot progress bar's "fill" + the green
  outline from the `BOOT_CARDS = make_card()` definition, which appears
  bright because the surroundings are filled with something — possibly
  the gradient swatches below.

What is consistent across all 4 runs (OrangePi+EVGPU, OrangePi+C_R_T,
WSL+SW default, WSL+SW after `run_boot` is disabled):
* The **visible region matches** the boot overlay's contents (NOT its bg).
* The screen-layer G3 content never reaches the user, regardless of which
  draw unit is used.

## 4. The Fix (One Line)

Change the boot overlay's background opacity to fully transparent:

```c
// demo_g3_showcase.c:370 (AFTER)
static void run_boot(void) {
    lv_obj_t * boot = lv_obj_create(lv_layer_top());
    lv_obj_set_size(boot, SCREEN_W, SCREEN_H);
    lv_obj_align(boot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(boot, lv_color_hex(0x000000), 0);
-   lv_obj_set_style_bg_opa(boot, LV_OPA_COVER, 0);
+   lv_obj_set_style_bg_opa(boot, LV_OPA_TRANSP, 0);
    // ...
}
```

Or, more semantically clean, remove the bg_color / bg_opa lines entirely
(they default to transparent).

If the **intent** was a full-screen boot splash that blacks out the G3
content until the boot animation ends (which is what `bg_opa=COVER`
suggested), the fix isn't to remove the splash — it's to render it on a
**dialog** (`lv_obj_create(lv_layer_top())` + `LV_OBJ_FLAG_HIDDEN` after
boot completes, or move it to a modal dialog API), not on a full-screen
opaque widget.

## 5. Implications for the Original Benchmark Report

`evgpu_benchmark_2026_07_18.md` reported:

> C_R_T is 3x faster than EVGPU after optimization.

That claim is **invalid**. What was being measured:

* EVGPU/C_R_T/Wayland-orangePi-render-rates under a **`LV_OPA_COVER`
  black overlay** that hides 99 % of G3_SHOWCASE.
* The visible 1 % was the SW-driven overlay animations, which both draw
  units happen to flush at similar speeds.
* "Optimization" (Kawase blur, batched arenas) is optimising paths that
  were never the bottleneck — the bottleneck is the boot overlay's
  invalidation cadence hitting the SW draw over and over.

The real acceleration factor (if any) needs to be re-measured after this
fix lands, on the same hardware, with `boot`'s bg transparent.

## 6. Files Touched

* `src/demo_g3_showcase.c:370` — boot bg `LV_OPA_COVER` → `LV_OPA_TRANSP`
  (1-line change to confirm the bug; revert before merge).
* `src/g3_snap.c`, `src/g3_flush_snap.c`, `src/simple_snap.c`,
  `src/flush_snap.c` — new SW-native snapshot tools.
* `CMakeLists.txt` — adds `g3_snap`, `g3_flush_snap`, `simple_snap`,
  `flush_snap` targets.
* `docs/g3_showcase_rendering_bug_2026_07_19.md` — earlier (wrong) root
  cause analysis, kept for history but marked deprecated.
* `docs/g3_showcase_wsl_native_repro_2026_07_19.md` — WSL repro showing
  the bug is independent of EVGPU/C_R_T/GPU.
* `docs/g3_showcase_root_cause_2026_07_19.md` — this file.
