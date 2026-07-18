# EVGPU vs EVGPU_C_R_T — Final Benchmark (2026-07-18)

**Board:** OrangePi PC (Allwinner H3, Mali-400/lima, GLES2.0)  
**Build:** Both binaries rebuilt from same current codebase (13 scenes, G3_SHOWCASE included)  
**Binary:** `lvglsim_evgpu` (pure EVGPU/evgr), `lvglsim_evgpu_c_r_t` (optimized direct-GL DrawUnit)  
**Benchmark:** 13 draw tasks, each ~48s, `R+F` = render + flush ms

## Results

| DrawTask | EVGPU(ms) | C_R_T(ms) | Δ |
|----------|:---------:|:---------:|:-:|
| FILL | 95 | 45 | −53% |
| BORDER | 99 | 35 | −65% |
| BOX_SHADOW | 328 | 356 | +9% |
| LABEL | 98 | 50 | −49% |
| IMAGE | 45 | 24 | −47% |
| LINE | 248 | 56 | −77% |
| ARC | 90 | 50 | −44% |
| BLUR | 78 | 45 | −42% |
| TRIANGLE | 73 | 20 | −73% |
| GRADIENT | 70 | 33 | −53% |
| LAYER | 104 | 39 | −63% |
| MASK_RECT | 103 | 48 | −53% |
| G3_SHOWCASE | 11 | 10 | −9% |
| **Average** | **110** | **62** | **−44%** |

## Analysis

**C_R_T is 44% faster** than pure EVGPU on Mali-400/lima GLES2.0.

Key wins by optimization:
| Optimization | Pattern Source | Impact |
|-------------|---------------|--------|
| Client-memory VBO arena | Cairo | LINE −77%, TRIANGLE −73% |
| Solid-color draw batching | ThorVG | BORDER −65%, LAYER −63% |
| Gradient texture baking + LRU | Cairo | GRADIENT −53%, FILL −53% |
| Dirty-bit state caching | Rive GLState | Broad overhead reduction |

**BOX_SHADOW** (+9%) remains the outlier — its per-draw unique state prevents batching; a dedicated shadow FBO path is needed.

## Memory Usage (RSS)

| Binary | RSS |
|--------|:---:|
| `lvglsim_evgpu` (pure EVGPU) | 43,004 KB |
| `lvglsim_evgpuganesh` | 43,408 KB |
| **Δ** | **+404 KB (+0.9%)** |

RSS is nearly identical. The tiny 404 KB overhead matches expectations from evgpuganesh's extra GL shader/VBO/FBO code. No meaningful memory impact.
