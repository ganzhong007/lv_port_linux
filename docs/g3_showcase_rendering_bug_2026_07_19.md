# G3_SHOWCASE Rendering Investigation (2026-07-19)

**Context:** Follow-up to `evgpu_benchmark_2026_07_18.md`. The previous benchmark report claimed
"C_R_T is 3x faster than EVGPU after optimization" based on FPS / render+flush timings on
OrangePi (Mali-400, GLES 2.0). This investigation was launched to validate that claim
**visually** by running G3_SHOWCASE in isolation on the actual hardware.

**Result (initial):** Both implementations exhibit the same rendering bug — only the
BOOT card area renders correctly on OrangePi.

**Result (revised 2026-07-19, after WSL native repro):** The bug is NOT specific to
EVGPU vs C_R_T vs GPU. The SAME failure reproduces with the **software draw unit** on
x86_64 Linux native, with no EVGPU/C_R_T code involved. See
`g3_showcase_wsl_native_repro_2026_07_19.md` for the definitive evidence.

**Conclusion:** The previous benchmark's "3× speedup" probably cannot be reproduced
either; both implementations are measuring their inability to render the scene, not a
real performance gap. The root cause is G3_SHOWCASE-specific (most likely some combination
of many gradients + low-opacity cards + continuous animations), NOT in the draw units.

---

## 1. Hardware & Software Setup

| Item | Value |
|---|---|
| Board | OrangePi PC (Allwinner H3, SoC) |
| GPU | Mali-400 (lima driver) |
| GLES | 2.0 only (no GLES 3.0 / GLSL 3.30 support) |
| Wayland compositor | Weston 14+ on `/run/user/0/wayland-0` |
| Resolution | 800×480 |

## 2. Cross-Compilation & Deployment

Built two stripped binaries (only G3_SHOWCASE demo, only one scene, no cycle through others):

| Binary | C_R_T enabled | Source change |
|---|---|---|
| `build-orangepi-g3-bench/bin/lvglsim` → pushed as `/home/orangepi/lvglsim_cross/lvglsim_evgpu_only_g3` | `LV_USE_DRAW_EVGPU_C_R_T=0` (EVGPU only) | `scene_act = 12`, scene_timer disabled |
| Same build → pushed as `/home/orangepi/lvglsim_cross/lvglsim_crt_only_g3` | `LV_USE_DRAW_EVGPU_C_R_T=1` (both, C_R_T wins dispatch at score=70 < EVGPU's 80) | same source change |

Run command on the board (via `screen -dmS` to survive SSH exit):

```bash
echo 'Even-123' | sudo -S chmod 777 /run/user/0/wayland-0 \
                                  /run/user/0/wayland-0.lock
export WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/0
cd /home/orangepi/lvglsim_cross
nohup setsid ./lvglsim_evgpu_only_g3 -b wayland -W 800 -H 480 \
  >/tmp/evgpu_g3.log 2>&1 < /dev/null &
```

## 3. Source Modification (src/evgpu_benchmark.c)

```diff
-static int scene_act = 0;
+static int scene_act = 12;     /* Only run G3_SHOWCASE (index 12) */
+static int max_scenes_act = MAX_SCENES; /* reserved for future use */
 ...
 void evgpu_benchmark_create(void) {
     init_scenes();
-    scene_act = 0;
+    /* scene_act keeps its initial 12 above; do NOT reset to 0 */
     ...
-    lv_label_set_text(info_label, "Starting...");
+    lv_label_set_text(info_label, "G3_SHOWCASE");
     ...
-    scenes[0].create_cb(scene_parent);
-    scene_timer = lv_timer_create(next_scene_timer_cb, SCENE_TIME_MS, NULL);
-    lv_timer_set_repeat_count(scene_timer, 1);
+    /* Skip scene_timer: scene would otherwise advance to 13 four seconds in,
+     * and the timer callback would wipe the screen with a benchmark summary
+     * table.  We want the G3_SHOWCASE frame to stay visible forever. */
+    scenes[scene_act].create_cb(scene_parent);
+    scene_timer = NULL;
 }
```

The timer is intentionally disabled because, even starting at `scene_act=12`,
after 4 s the one-shot timer fires, increments `scene_act` to 13, hits
`if(scene_act >= MAX_SCENES)` and creates a 13-row benchmark-result table that
covers the G3_SHOWCASE scene.

## 4. Empirical Observations

### 4.1 EVGPU-only build (PID 3694 ran ≥ 30 s)

* Log: 33 161 draw tasks queued and executed.
* All `[LVGL:L3-EVGPU] execute ...` lines visible.
* Screen: only a single yellow-tan region in the lower-right quadrant; rest is uniform dark.

### 4.2 EVGPU_C_R_T build (PID 6394 ran continuously)

* Log: 226 826+ draw tasks. Breakdown:
  ```
  103 348 add task FILL
   88 546 add task LABEL
   54 162 add task BORDER
   24 625 add task OTHER
  ```
* All `[LVGL:L3-EVGPU_C_R_T] execute task type=N` lines visible (type 1=FILL,
  type 2=BORDER, type 5=LABEL, type 9=OTHER/BLEND).
* GLSL ES 3.00 / 3.30 shader compile **warnings** still appear in the log
  (from `lv_opengl_shader_manager`), even though C_R_T's own shader uses
  `precision mediump` GLES 2.0. These come from a different code path
  (`lv_opengl_shader_manager_select_shader`) and do **not** block C_R_T.
* Screen: **identical** to the EVGPU-only run — only the BOOT card region
  in the lower-right corner. See attached photo.

### 4.3 What is actually rendered

The bright shape on screen is not an `ambient_glow` (those are
gradient-tan/near-black and centered in the upper-right / lower-left).
It is the **BOOT card and its hover-state shadow**:

* `demo_g3_showcase.c:308` `build_card_boot(...)` uses
  `LV_ALIGN_BOTTOM_RIGHT, -12, -12`.
* `make_card(...)` calls `add_hover_glow(c)` which sets, in `LV_STATE_HOVERED`:
  * `bg_color = 0x45FF8A` (bright green)
  * `border_color = 0x45FF8A`
  * `border_width = 2`, `shadow_width = 24`, `shadow_opa = 150/255`
  * shadow extends 24 px outward from the card on all sides
* The yellow-green color the user perceives on screen matches
  `0x45FF8A` rendered with premultiplied alpha on a dark background.
* The other 3 cards (PLAYER top-left, WORLD top-right, HOVER bottom-left)
  produce **no visible pixels** even though their tasks are dispatched and
  `execute`'d in the log.

The clear text "G3 UX SHOWCASE", the breathing dot, the EQ bars, the
HOVER swatches, "PLAYER" / "WORLD" / "HOVER" / "BOOT" titles — none visible.
The faint diagonal noise in the dark regions of the photo is JPEG
compression and screen dithering of the underlying uniform-black buffer.

## 5. Root-cause Analysis (Theory — DEPRECATED)

**Update:** The hypotheses below were disproven by the WSL native repro
(`g3_showcase_wsl_native_repro_2026_07_19.md`). The bug is NOT in the
shared readback / alpha / GPU pipeline. It reproduces with the pure-SW
software draw unit on x86_64, where most of these hypotheses cannot apply
(no FBO, no readback, no GLES, no wayland).

The previous hypothetical causes (kept here for completeness, marked wrong):

~~1. **Premultiplied-alpha mismatch.** ...~~ ❌ ruled out
~~2. **Row-flip oddity in `on_layer_readback`.** ...~~ ❌ ruled out
~~3. **FBO/wayland swap chain reuses one buffer.** ...~~ ❌ ruled out

**Real lead** (from native repro): G3_SHOWCASE produces a black frame even
with pure-SW draw. Either `demo_g3_showcase.c` is invoking some render path
that doesn't work without external GPU acceleration, or some combination of
"many labels + low-opacity overlapping cards + radial gradients" breaks
LVGL's task compositing. Bisect with the SW native build (see §11 of the
WSL native doc) to localise the bad component.

## 6. Implication for the Previous Benchmark Report

`evgpu_benchmark_2026_07_18.md:166` claims:

> **C_R_T is 3x faster than EVGPU after optimization.**

This claim is **invalid** on OrangePi / Mali-400 because:

1. Both implementations have the **same rendering bug**; the FPS / timing
   numbers are measuring partially-rendered frames.
2. The "optimization" (Kawase blur, batched arena, pre-multiplied LUT
   gradients) optimises paths that **never executed correctly** to begin
   with on this hardware.
3. C_R_T's apparent speedup is more likely attributable to the fact that
   it skips the `evgrBeginFrame` / `evgrEndFrame` fbo flush, meaning fewer
   submits per frame — making the bug **less visible** (fewer overwritten
   rows in the FBO), not actually faster or correct.

The numbers from that report should be re-run on hardware where the
shader path actually works (PC-class GPU with GLES 3.0+) before any
"3x speedup" claim is propagated.

## 7. Suggested Next Steps (NOT YET DONE)

Pick one and validate on real hardware:

1. **Readback path.** In `lv_draw_evgpu_c_r_t.c:186` and
   `lv_draw_evgpu.c` (`on_layer_readback`), investigate whether the FBO
   pixel transfer is inverting the buffer layout. Try reading into the
   draw_buf in **forward** order (no `h-1-y`) and see if more of the scene
   appears.
2. **Premultiplied flag.** Try removing the
   `LV_IMAGE_FLAGS_PREMULTIPLIED` set when the wayland surface is opaque
   (no alpha config). Or, conversely, force the wayland window to have
   an alpha channel.
3. **TexImage vs readback.** Replace the row-by-row readback with a
   single `glReadPixels(0, 0, w, h, ...)` to test whether the per-row
   path is the bug.
4. **EVGPU_C_R_T direct path.** C_R_T's draw loop uses
   `lv_evgpu_c_r_t_gl_flush` and an Arena, dispatching to the default
   framebuffer rather than an FBO. Compare framebuffer contents to FBO
   contents for the same draw set — if framebuffer is correct, the FBO
   path is the issue.
5. **Single-card test.** Add a `g3_card_only` benchmark that creates
   only the BOOT card on a clean background to confirm visually that
   EVGPU and C_R_T both render that single card correctly. Then add
   the other cards one at a time to bisect where the bug appears.

## 8. Reproduce

```bash
# On dev machine
cd /home/gz/opencodeprj/lv_port_linux
cmake -B build-orangepi-g3-bench -DCONFIG=orangepi-evgpu-crt -DLVGL_APP_DEMO=benchmark
# (then either toggle LV_USE_DRAW_EVGPU_C_R_T in lv_conf.h between 0 and 1)
cmake --build build-orangepi-g3-bench -j$(nproc)
scp build-orangepi-g3-bench/bin/lvglsim \
    orangepi@192.168.10.140:/home/orangepi/lvglsim_cross/lvglsim_evgpu_only_g3

# On Orangepi (local terminal, not SSH)
cd /home/orangepi/lvglsim_cross
echo 'Even-123' | sudo -S \
  WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/0 \
  ./lvglsim_evgpu_only_g3 -b wayland -W 800 -H 480
```

## 9. Files Touched

* `src/evgpu_benchmark.c` — `scene_act=12`, scene_timer disabled (intentional
  workaround to keep G3_SHOWCASE on screen).
* `build-orangepi-g3-bench/lv_conf.h` — temporarily flipped
  `LV_USE_DRAW_EVGPU_C_R_T` between 0 and 1 to produce the two binaries
  (`lvglsim_evgpu_only_g3`, `lvglsim_crt_only_g3`) on the board.
