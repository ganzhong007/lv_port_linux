# Unity VECTOR: SW gold vs EVGPU / EVGPU_C_R_T (2026-07-21)

First-pass screenshot compare of `test_draw_vector` against **SW** `ref_imgs/` (no separate GPU golds yet).

## How to run

```bash
PREFIX=$PWD/.local-deps
export PKG_CONFIG_PATH="$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig"
export LD_LIBRARY_PATH="$PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"

cd lvgl/tests
# OPTIONS already configured under build_test_evgpu / build_test_evgpu_c_r_t

LV_TEST_MAE_CSV=../../benchmark_logs/mae_evgpu_vector_2026_07_21.csv \
LV_TEST_ACTUAL_DIR=../../benchmark_logs/unity_evgpu_actual_2026_07_21 \
  ./build_test_evgpu/test_draw_vector

LV_TEST_MAE_CSV=../../benchmark_logs/mae_crt_vector_2026_07_21.csv \
LV_TEST_ACTUAL_DIR=../../benchmark_logs/unity_crt_actual_2026_07_21 \
  ./build_test_evgpu_c_r_t/test_draw_vector
```

MAE CSV columns: `fn,result,mae,exact_pct`  
MAE = mean over pixels of `(|ΔR|+|ΔG|+|ΔB|)/3` (0–255 scale).

## Results (`test_draw_vector` vs `ref_imgs/`)

| case | SW (gold) | EVGPU MAE / exact% | CRT MAE / exact% |
|------|-----------|--------------------|------------------|
| `vector_draw_lines` | pass (reference) | **2.78** / 95.4% | **0.97** / 95.1% |
| `vector_draw_shapes` | pass (reference) | **15.68** / 84.5% | **10.70** / 82.7% |
| `vector_draw_shapes_during_rendering` | pass (reference) | **1.95** / 86.0% | **35.62** / 48.4% |
| `test_transform` / `matrix_rotation` | pass | pass | pass |

Exact pixel match vs SW is **not** expected on this pass; MAE + actual PNGs are the metric.

Artifacts:

- `benchmark_logs/mae_evgpu_vector_2026_07_21.csv`
- `benchmark_logs/mae_crt_vector_2026_07_21.csv`
- `benchmark_logs/unity_evgpu_actual_2026_07_21/`
- `benchmark_logs/unity_crt_actual_2026_07_21/`

## Unity OPTIONS

| CMake option | `LV_TEST_OPTION` | DrawUnit | Notes |
|--------------|------------------|----------|-------|
| `OPTIONS_TEST_EVGPU` | 8 | EVGPU (EVGR) | Headless EGL pbuffer; `REF_IMGS_PATH=ref_imgs/` |
| `OPTIONS_TEST_EVGPU_C_R_T` | 9 | EVGPU_C_R_T | Same gold path; CRT off EVGPU and vice versa |

Conf headers: `lvgl/tests/src/lv_test_conf_evgpu.h`, `lv_test_conf_evgpu_c_r_t.h`.  
GL helper: `lvgl/tests/src/lv_test_gl_context.c` (surfaceless/pbuffer + layer FBO readback).

## Fixes needed for this pass

1. **CRT:** do not `glBindFramebuffer(0)` when the layer already has an FBO; seed FBO from CPU `draw_buf` after clear (canvas `fill_bg`).
2. **EVGPU:** `lv_evgpu_native_gl_prepare()` must **rebind the layer FBO** when `layer->user_data` is set — previously it always bound default FB 0, so fills/images went to the pbuffer while screenshot readback read an empty layer FBO (all-black frames).
3. Absolute `LV_TEST_ACTUAL_DIR` mkdir: preserve leading `/` in `create_folders_if_needed`.

## Next (out of scope here)

- Dedicated `ref_imgs_evgpu/` / `ref_imgs_evgpu_c_r_t/` golds once MAE stabilizes
- Broader Unity suites beyond `test_draw_vector`
