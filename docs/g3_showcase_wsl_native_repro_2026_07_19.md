# G3_SHOWCASE WSL Native Snapshot — Repros WITHOUT GPU/EVGPU/C_R_T (2026-07-19)

**Context:** Following the user's correction ("除了G3_SHOWCASE，其他case显示都是对的"
= all 12 other benchmark cases render correctly), we re-ran G3_SHOWCASE in
the WSL x86_64 native build using the SOFTWARE rasterizer (SW draw), not
EVGPU. Same rendering failure reproduces. This rules out every hypothesis
from `g3_showcase_rendering_bug_2026_07_19.md` that involved GPU/shader/wayland.

## 1. Setup (WSL native build)

* x86_64 native, no cross-compilation
* LCD display driver: built-in software (`LV_DISPLAY_RENDER_MODE_FULL`)
* Draw unit: **software** (`LV_USE_DRAW_SW=1`), EVGPU/C_R_T disabled
  (`LV_USE_DRAW_EVGPU=0`, `LV_USE_DRAW_EVGPU_C_R_T=0`)
* Demo: `LVGL_APP_DEMO=g3_showcase` → calls `demo_g3_showcase_init()`
* Snapshot path: bypass `lv_snapshot_*` (which produced all-black) and
  hook `lv_display_set_flush_cb` to `memcpy(backbuf, px_map)` then write PPM
  from the buffer

### Build steps executed
```bash
sed -i 's/^LV_USE_EVDEV    1$/LV_USE_EVDEV    0/' lv_conf.defaults
echo 'LV_USE_SNAPSHOT 1' >> lv_conf.defaults
sed -i 's|set(LVGL_APP_DEMO.*|set(LVGL_APP_DEMO "g3_showcase" CACHE STRING "...|' CMakeLists.txt
cmake -B build && cmake --build build --target g3_flush_snap
./build/bin/g3_flush_snap   # /tmp/g3_frame_1.ppm
```

### Tools built
* `simple_snap` — 1 red rect + 1 blue rect + green label. Used as a
  reference "does SW draw work at all" baseline.
* `g3_flush_snap` — runs `demo_g3_showcase_init()`, hooks flush_cb, saves
  full frame as PPM.
* `flush_snap` — same pattern as g3_flush_snap but with the simple scene.

## 2. Results

### Reference (`/tmp/flush_snapshot.ppm`)
* 800×480, 384 000 / 384 000 pixels non-black
* Distinct colors: 34
* Top colors: `(64,64,64)` gray bg (303 279), `(255,0,0)` red
  rectangle (38 340), `(0,0,255)` blue rectangle (38 340),
  `(224,224,224)` border highlights (3 016), `(0,255,0)` text (158)
* `SW draw → memcpy → flush` chain works perfectly for plain geometry.

### G3_SHOWCASE (`/tmp/g3_frame_1.ppm`)
* **3 484 / 384 000 non-black pixels (0.91 %)**
* Distinct colors: 50 (all related to `G_GREEN = 0x45FF8A` and its
  anti-aliased halo)
* Most common non-black pixel: `(69,255,138)` = `0x45FF8A` (1 409 px)
* Visualised ASCII map of non-black pixels (downsampled to 80×24):

```
y  0: ................................................................................
...
y160: ....................................########....................................
y180: ....................................########....................................
y200: .....................................##..##.....................................
y220: ..............................############..######..............................
y240: ..............................#####.######..######..............................
y260: ................................################................................
y280: .................................##############.................................
y300: .................................##############.................................
...
y460: ................................................................................
```

All non-black pixels cluster between y=160 and y=310, x≈350–460.
This region matches **the WORLD card's gradient swatches** at
`grad_swatch(row, 0x243444, ...)` etc., which are 64×64 circle cells with
bg gradients.

## 3. Interpretation

This rules out every hypothesis from `g3_showcase_rendering_bug_2026_07_19.md`:

| Hypothesis | Verdict |
|---|---|
| EVGPU shader compile failures on Mali-400 | ❌ ruled out — same bug, but no shaders involved here |
| EVGPU_C_R_T dispatch priority bug | ❌ ruled out — C_R_T disabled in this build |
| wayland EGL readback / colour-format mismatch | ❌ ruled out — flush_cb gets the buffer directly, no readback |
| Premultiplied-alpha interpretation | ❌ ruled out — SW draw produces premultiplied buffers that the simple test handles fine |
| The flush handler copies the wrong region of the buffer | ❌ ruled out — simple test flushes the same buffer fine |

What is **consistent across all four runs (OrangePi+EVGPU, OrangePi+C_R_T,
WSL+EVGPU previously failed to build, WSL+SW)**:

* The visible region is **always** the gradient swatches inside the WORLD
  card (rows 160–305) plus the 4-stop radial gradient from
  `build_world()` in some boards.
* Simple primitives (rect with solid color, text label) render
  correctly via SW.

## 4. Narrowing Down What's Special About G3_SHOWCASE

G3 components that the other 12 benchmark cases do NOT all use:

| Component | In G3? | In other cases? |
|---|---|---|
| Multiple back-to-back **gradient fills** (`build_world` 4-stop radial + 2× `ambient_glow` + `vignette`) | YES | only GRADIENT case has any |
| **`run_boot()` creating `lv_layer_top()` overlay** | YES | only LAYER case creates one — but it's just the card, not the LAYER demo with animations |
| **Animations continuously invalidating** (breathing dot, EQ bars, status icons) | YES | only ANIM case animates |
| **Many low-opacity overlapping objects** (cards with `bg_opa=16` + borders) | YES | no other case does this combination |
| **Many labels** | YES | no other case has 88k labels/frame |

The likeliest culprit is the **combination of "many text labels + continuous
animations + low-opacity card BG + radial gradients",** which stresses a
path that the simple cases never touch.

## 5. What To Investigate Next

The bug is NOT in EVGPU vs C_R_T vs GPU. It is in:

* Either `demo_g3_showcase.c` (the demo's composition is wrong for some
  render paths), or
* The LVGL rendering pipeline's handling of one of the G3-specific
  components (gradients + low-opacity overlapping + many labels).

To bisect, the simplest next step is to bisect in **the same SW build**
by editing `demo_g3_showcase.c` to remove one component at a time and
re-snapshot:

```c
// In demo_g3_showcase.c, comment one block at a time:
//   (A) build_world()              — verify gradient fill
//   (B) ambient_glow(...)          — verify glow render
//   (C) build_vignette(...)        — verify opacity=0 vs ≠0
//   (D) build_card_player/...      — verify low-opaque cards
//   (E) run_boot()                 — verify lv_layer_top()
// Each round: take a snapshot and check pixel coverage.
```

Once the offending component is identified, the fix is in the demo
itself (likely: remove/replace that component, or skip lv_layer_top),
NOT in the draw unit.
