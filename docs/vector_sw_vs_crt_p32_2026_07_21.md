# VECTOR SW vs CRT — P3.2 tess + stroke-grad demo（2026-07-21）

## 改动

1. **P3.2 tess**：`tess_polygon()` = 双向 ear-clip → 质心扇形回退  
2. **Demo**：`draw_stroke_gradient()` 红→蓝线性描边（ROI ~490–770 × 360–450）

## 指标（800×480，同 demo）

| 区域 | exact% | MAE | 说明 |
|------|--------|-----|------|
| full | 63.55 | 11.41 | 新加 stroke 渐变后全屏略差于 P2 |
| fill_grad | 1.54 | **42.57** | 与 P1/P2 相同（fill 未改） |
| **stroke_grad** | **78.64** | **21.80** | **新指标：描边渐变可对拍** |
| dash | 30.56 | 12.08 | |
| pattern | 43.18 | 36.77 | |

相对「无 stroke 渐变实现」时该 ROI 会对不齐（实色 vs 渐变）；现 MAE 21.8、exact ~79%，说明 CRT 与 SW 在同区域已同画渐变描边。

产物：`benchmark_logs/vector_sw_vs_crt_p32_2026_07_21/`
