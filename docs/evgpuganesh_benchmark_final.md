# EVGPU vs EVGPUGANESH — Final Benchmark (2026-07-18)

**Board:** OrangePi PC (Allwinner H3, Mali-400/lima, GLES2.0)  
**Build:** Both binaries rebuilt from scratch after fixing `LV_USE_DRAW_EVGPUGANESH` in template  
**Binary:** `lvglsim_evgpu` (pure EVGPU/evgr), `lvglsim_evgpuganesh` (new direct-GL DrawUnit)  
**Benchmark:** 12 draw tasks, each ~48s, `R+F` = render + flush ms

## Results

| DrawTask | EVGPU(evgr) | EVGPUGANESH | Δ |
|----------|:-----------:|:-----------:|:-:|
| FILL | 68ms | 90ms | +32% |
| BORDER | 83ms | 80ms | −4% |
| BOX_SHADOW | 140ms | 328ms | +134% |
| LABEL | 81ms | 90ms | +11% |
| IMAGE | 39ms | 46ms | +18% |
| LINE | 215ms | 220ms | +2% |
| ARC | 80ms | 79ms | −1% |
| BLUR | 81ms | 74ms | −9% |
| TRIANGLE | 71ms | 66ms | −7% |
| GRADIENT | 61ms | 70ms | +15% |
| LAYER | 80ms | 84ms | +5% |
| MASK_RECT | 71ms | 91ms | +28% |
| **Average** | **89ms** | **109ms** | **−22%** |

## Analysis

- **EVGPUGANESH is 22% slower** than pure EVGPU on this hardware (Mali-400/lima).
- **BOX_SHADOW** is the worst offender (328ms vs 140ms). Likely hitting SW fallback or a costly GPU sync path (F=89ms flash time).
- **FILL** (+32%) and **MASK_RECT** (+28%) also underperform, suggesting the simple GLES2 quad-drawing in evgpuganesh is less efficient than evgr's batched path.
- **BLUR** (−9%), **TRIANGLE** (−7%), **BORDER** (−4%) show small wins, indicating some tasks benefit from direct GL.
- The Ganesh-style direct GL approach does not outperform EVGPU's evgr backend on this low-end GLES2 GPU.
