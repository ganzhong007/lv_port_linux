# VECTOR 三方对比（pattern UV 修复后 · 2026-07-21）

## 修复（CRT）

默认 `fill_units = OBJECT_BOUNDING_BOX`（`lv_zalloc` → 0）。EVGPU/SW 把图原点放在 **path 局部包围盒左上**；CRT 原先用局部坐标直接 `/ img_size`，UV 错位。

改动（`lv_draw_evgpu_c_r_t_vector.c`）：

- pattern UV：`local = inv(path)·world`，再减 bbox 原点，再 `inv(fill)`，再 `/ img`
- wrap：`GL_CLAMP_TO_EDGE`
- 曲线细分：MEDIUM 默认 12 步

## 成对指标（exact% / MAE）

| ROI | CRT vs SW | EVGPU vs SW | CRT vs EVGPU |
|-----|-----------|-------------|--------------|
| **full** | 63.6 / **9.84** | 63.0 / 7.96 | **87.9** / 9.71 |
| **avatar** (200–320×200–310) | 18.9 / **14.7** | 18.9 / 13.8 | **78.3** / **1.55** |
| fill_grad | 65.5 / 7.9 | 62.4 / 17.6 | 66.2 / 20.6 |
| stroke_grad | 78.4 / 22.1 | 78.4 / 22.3 | 84.3 / 9.0 |
| dash | 28.9 / 6.1 | 29.4 / 4.9 | 97.4 / 1.9 |

### Avatar 修复前后（同 ROI）

| | CRT vs SW MAE |
|--|---------------|
| 修复前 | **57.8** |
| 修复后 | **14.7**（≈ EVGPU 的 13.8） |

CRT↔EVGPU 在头像区 MAE **1.55**、exact **78%** —— pattern 通路已与 EVGPU 对齐。

> 旧 `pattern` ROI (40–220×180–340) 几乎盖不到头像（脸约在 x≈250+），且含虚线/色块，不宜再当 pattern 指标。

## 定性（未变部分）

| 能力 | SW | CRT | EVGPU |
|------|----|-----|-------|
| pattern 头像 UV | ✓ | ✓（已修） | ✓ |
| fill 线性渐变 | ✓ | ✓ | ✗ 实色橄榄 |
| stroke 线性渐变 | ✗ 无笔画 | ✓ | ✗ 实色紫 |

产物：`benchmark_logs/vector_sw_crt_evgpu_patfix_2026_07_21/`
